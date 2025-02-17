/**
 * part of jzumbrink/Lzendsa
 * 
 * MIT License
 * 
 * Copyright (c) Jan Zumbrink
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

# ifndef _LZENDRI_HPP
# define _LZENDRI_HPP

#include <memory>
#include <utility>
#include <ostream>
#include <fstream>

#include "r_index.hpp"

#include "word_packing_encoding.hpp"
#include "utils/definitions.hpp"
#include "utils/time.hpp"
#include "libsais_wrapper.hpp"

#include "utils.hpp"

template<typename IntWord, typename E>
class Lzendri {
public:
    std::unique_ptr<E> encoding;
    std::unique_ptr<ri::r_index<>> r;

    void build(std::string &s, IntWord h = -1, std::string filename = "none") {
        # ifdef INFOS
        std::cout << "Compute Suffix Array..." << std::endl << std::flush;
        auto timer_all = timestamp();
        # endif

        // compute suffix array
        IntWord n = s.length();
        auto sa = std::make_unique<IntWord[]>(n);

        lib::sais((uint8_t const*) s.data(), sa.get(), n);

        // construct differential suffix array
        # ifdef INFOS
        std::cout << "Construct Differential Suffix Array..." << std::endl << std::flush;
        # endif

        for (IntWord i = n - 1; i > 0; i--) {
            sa[i] = sa[i] - sa[i-1];
        }
        
        // compute lzend parsing
        # ifdef INFOS
        std::cout << "Compute LZ-End parsing..." << std::endl << std::flush;
        # endif

        std::vector<lzend::IntPhrase<IntWord>> phrases = lzend::parse<IntWord>(sa, n, false, h);

        // construct lzend encoding
        # ifdef INFOS
        std::cout << "Compute LZ-End encoding..." << std::endl << std::flush;
        # endif

        encoding = std::make_unique<E>();
        encoding->build(phrases);
        
        // build r-index
        # ifdef INFOS
        std::cout << "Build r-index..." << std::endl << std::flush;
        # endif
        r = std::make_unique<ri::r_index<>>(s);

        # ifdef INFOS
        auto td_all = timestamp() - timer_all;
        std::cout << "lzendri index was build in " << td_all << " ms." << std::endl << std::flush;
        # endif
    }

    std::vector<IntWord> locate(std::string &pattern, uint16_t NUM_THREADS = 1) const {
        std::pair<std::pair<uint64_t, uint64_t>, uint64_t> count_result = r->count_and_get_occ(pattern);
        
        IntWord start_position = static_cast<IntWord>(std::get<0>(count_result).first) - 1;
        IntWord end_position = static_cast<IntWord>(std::get<0>(count_result).second) - 1;
        IntWord last_value = static_cast<IntWord>(std::get<1>(count_result));

        std::vector<IntWord> decoded_result = encoding->extract(start_position + 1, end_position - start_position, NUM_THREADS);

        decoded_result.push_back(last_value);

        for (IntWord i = decoded_result.size() - 2; i >= 0; i--) {
            decoded_result[i] = decoded_result[i+1] - decoded_result[i];
        }

        return decoded_result;
    }

    Interval count(std::string &pattern) {
        std::pair<uint64_t, uint64_t> result = r->count(pattern);
        return {static_cast<int64_t>(result.first), static_cast<int64_t>(result.second)};
    }

    IntWord get_n() {
        return encoding->get_n();
    }

    uint64_t serialize(std::ostream &out) {
        uint64_t out_size = 0;

        out_size += encoding->serialize(out);
        out_size += r->serialize(out);

        return out_size;
    }

    uint64_t save(std::string const &filename) {
        std::ofstream out(filename);
        uint64_t out_bytes = serialize(out);
        out.close();

        return out_bytes;
    }

    void load(std::istream &in) {
        encoding = std::make_unique<E>();
        encoding->load(in);
        r = std::make_unique<ri::r_index<>>();
        r->load(in);
    }

    size_t memory_usage() const {
        return 0;
    }
};

# endif