/**
 * part of pdinklag/lzend
 * 
 * MIT License
 * 
 * Copyright (c) Patrick Dinklage
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

# ifndef _INTEGER_LZEND_HPP
# define _INTEGER_LZEND_HPP


#include <cstdint>
#include <iostream>
#include <functional>
#include <vector>
#include <memory>
#include <algorithm>

#include <rmq/rmq.hpp>
#include <ordered/btree/map.hpp>
#include "libsais_wrapper.hpp"
#include "ips4o.hpp"

#include "utils/time.hpp"

namespace lzend {

template<typename Index = int32_t>
struct IntPhrase {
    Index lnk; // phrase id, where the source ends (a new phrase can extend multiple phrases)
    Index len; // len of phrase (including the extension)
    Index ext; // extension
};

template<typename Index = int32_t>
struct DSAValueIndexPair { // used for constructing the transformed reverse delta suffix array 
    Index value;
    Index index;

    bool operator < (const DSAValueIndexPair &other) const {
        return value < other.value;
    }
};

/*
Reverses the given delta suffix array and transforms every value into a non-negative value, such that
the order of the values is preserved, but the distances between the values are reduced to one.
This transformation makes the generation of the suffix array of the reversed delta suffix array faster,
 because the suffix array construction is linear with regards to the size of the given alphabet.
*/
template<typename Index>
std::unique_ptr<Index[]> reverse_delta_suffix_array(std::unique_ptr<Index[]> &dsa, Index const n, Index* new_alphabet_size) {
    std::vector<DSAValueIndexPair<Index>> value_index_pairs;
    value_index_pairs.reserve(n);

    for (Index i = 0; i < n; i++) {
        value_index_pairs.push_back({dsa[i], i});
    }

    // sort all value index pairs by the original delta suffix array value
    # ifdef INFOS
    std::cout << "\t\tvalue index pairs created." << std::endl;
    std::cout.flush();
    auto const timer_sort = timestamp();
    # endif
    ips4o::sort(value_index_pairs.begin(), value_index_pairs.end());
    # ifdef INFOS
    auto const time_diff = timestamp() - timer_sort;
    std::cout << "\t\tvalue index pairs were sorted in " << time_diff << " ms" << std::endl;
    # endif


    // use this sorted vector of value index pairs to construct the transformed reverse delta suffix array
    std::unique_ptr<Index[]> rdsa = std::make_unique<Index[]>(n);

    Index last_value = -1;
    bool first_iteration = true;
    Index new_value = -1;
    
    for (auto it = value_index_pairs.begin(); it != value_index_pairs.end(); ++it) {
        if (it->value != last_value || first_iteration) {
            // if the current value is unequal to the last value, the reverse delta suffix array should also have an unequal value
            // if the current value is equal to the last value, the rdsa should have the same value for both indices 
            last_value = it->value;
            new_value++;
            first_iteration = false;
        }
        rdsa[n - it->index - 1] = new_value;
    }

    *new_alphabet_size = new_value + 1; // use this pointer to "return" the alphabet size, because it is important to know for the sa construction
    return rdsa;
}

template<typename Index = int32_t>
std::vector<IntPhrase<Index>> parse(std::unique_ptr<Index[]> &dsa, Index const n, bool print_progress = false, Index h = -1) {
    # ifdef INFOS
    print_progress = true;
    # endif

    if(print_progress) std::cout << "Integer-LZ-End input: n=" << n << std::endl;

    // reverse delta suffix array
    if(print_progress) std::cout << "\treverse delta suffix array..." << std::endl;

    Index new_alphabet_size = -1;
    std::unique_ptr<Index[]> rdsa = reverse_delta_suffix_array<Index>(dsa, n, &new_alphabet_size);

    // construct suffix array of reverse delta suffix array
    if(print_progress) std::cout << "\tconstruct suffix array ..." << std::endl;
    auto int_sa = std::make_unique<Index[]>(n);
    # ifdef INFOS
    auto const timer_sa_construction = timestamp(); 
    # endif
    
    // Segmentation fault, when negative Integer are present in rdsa
    // libsais_int only works on non-negative integers
    // squash all values into a dense intervall of non-negative integers to reduce alphabet size
    lib::sais_int(rdsa.get(), int_sa.get(), n, new_alphabet_size);
    
    # ifdef INFOS
    auto const td_sa_construction = timestamp() - timer_sa_construction;
    std::cout << "\t\tsuffix array of reverse delta suffix array was constructed in " << td_sa_construction << " ms" << std::endl;
    std::cout << "\tconstruct LCP array ..." << std::endl << std::flush;
    # endif

    // construct PLCP array and the LCP array from it
    auto isa = std::make_unique<Index[]>(n);
    auto& plcp = isa;
    lib::sais_plcp_int(rdsa.get(), int_sa.get(), isa.get(), n);
    rdsa.reset();

    auto lcp = std::make_unique<Index[]>(n);
    lib::sais_lcp(plcp.get(), int_sa.get(), lcp.get(), n);

    // construct RMQ data structure
    if(print_progress) std::cout << "\tconstruct RMQ ..." << std::endl << std::flush;
    rmq::RMQ<Index> rmq(lcp.get(), n);

    // construct permuted inverse suffix array
    if(print_progress) std::cout << "\tconstruct permuted inverse suffix array ..." << std::endl << std::flush;
    for (Index i = 0; i < n; i++)
    {
        isa[n-int_sa[i]-1] = i;
    }

    // discard suffix array and reverse text
    int_sa.reset();
    
    // initialize predecessor/successor
    ordered::btree::Map<Index, Index> marked;
    

    // helpers
    struct Candidate { Index lex_pos; Index lnk; Index len; };

    auto lex_smaller_phrase = [&](Index const x){
        auto const r = marked.predecessor(x-1);
        return r.exists
            ? Candidate { r.key, r.value, lcp[rmq(r.key+1, x)] }
            : Candidate { 0, 0, 0 };
    };

    auto lex_greater_phrase = [&](Index const x){
        auto const r = marked.successor(x+1);
        return r.exists
            ? Candidate { r.key, r.value, lcp[rmq(x+1, r.key)] }
            : Candidate { 0, 0, 0 };
    }; 

    // parse
    if (print_progress) {
        std::cout << "\tparse ... " << std::endl;
        std::cout.flush();
    }

    std::vector<IntPhrase<Index>> parsing;
    parsing.push_back({0, 1, dsa[0]}); // initial empty phrase
    Index z = 0; // index of latest phrase (number of phrases would be z+1)

    for (Index i = 1; i < n; i++) {
        Index const len1 = parsing[z].len;
        Index const len2 = len1 + (z > 0 ? parsing[z-1].len : 0);

        Index const isa_last = isa[i-1];

        // find source phrase candidates
        Index p1 = -1, p2 = -1;
        auto find_copy_source = [&](std::function<Candidate(Index)> f){
            auto c = f(isa_last);
            if(c.len >= len1) {
                p1 = c.lnk; // only mark the small enough phrases omits the phrase length check here
                if(i > len1 && (h == -1 || len2 < h)) { // only check for merge candidates if the new phrase would be small enough (or the max phrase size is unbounded)
                    if(c.lnk == z-1) c = f(c.lex_pos);
                    if(c.len >= len2) p2 = c.lnk;
                }
            }
        };

        if (h == -1 || len1 < h) {
            find_copy_source(lex_smaller_phrase);
            if(p1 == -1 || p2 == -1) {
                find_copy_source(lex_greater_phrase);
            }
        }

        // case distinction according to Lemma 1
        if(p2 != -1) {
            // merge last two phrases
            marked.erase(isa[i - 1 - len1]);
            
            parsing.pop_back();
            --z;
            
            parsing.back() = IntPhrase<Index> { p2, len2 + 1, dsa[i] };
        } else if(p1 != -1 && (h == -1 || len1 < h)) {
            // extend last phrase
            parsing.back() = IntPhrase<Index> { p1, len1 + 1, dsa[i] };
        } else {
            // lazily mark previous phrase
            marked.insert(isa_last, z);

            // begin new phrase
            parsing.push_back(IntPhrase<Index> { 0, 1, dsa[i] });
            ++z;
        }
    }

    return parsing;
}
}

#endif