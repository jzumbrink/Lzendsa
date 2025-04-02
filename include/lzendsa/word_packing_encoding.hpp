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

# ifndef _WORD_PACKING_ENCODING_HPP
# define _WORD_PACKING_ENCODING_HPP

#include <vector>
#include <bit>
#include <algorithm>
#include <omp.h>


#include "integer_lzend.hpp"
#include "utils.hpp"
#include "word_packing.hpp"
#include "encoding.hpp"

using Pack = uint64_t;

namespace lzend {

/**
 * This encoding is designed to be rather small (3 words per phrase) while being simple and enabling efficient extraction.
 */
template<typename Index>
class WordPackingEncoding : public Encoding<Index> {
private:
    Pack* packed_extensions;
    Pack* packed_sources;
    size_t bits_per_source;
    size_t bits_per_extension;
    Index min_ext;

    // The vector <end_positions> stores the index of the last element of a phrase in the original uncompressed text.
    // Essentially serves as a constant-time select operation (index of the j-th 1)
    // and a log(z)-time rank operation over the ending positions of the text (number of 1s in a given interval).
    // First phrase ends at position 0, so end_positions[0] = 0 holds.
    # ifdef END_POSITION_PACK
    size_t bits_per_end_positions;
    Pack* packed_end_positions;
    # else
    std::vector<Index> end_positions;
    # endif
    Index z; // the number of phrases of the lzend factorization
    Index n; // the number of symbols in the uncompressed text
public:
    WordPackingEncoding() {}

    void build(std::vector<lzend::IntPhrase<Index>> &phrases) {
        z = phrases.size();
        // sources
        bits_per_source = std::bit_width(static_cast<uint64_t>(z));
        packed_sources = new Pack[word_packing::num_packs_required<Pack>(z, bits_per_source)];
        auto sources = word_packing::accessor(packed_sources, bits_per_source);

        // extensions
        n = 0;
        min_ext = 0;
        Index max_ext = 0;
        bool first_iteration = true;
        for (auto phrase : phrases) {
            n += phrase.len;
            if (first_iteration || min_ext > phrase.ext) min_ext = phrase.ext;
            if (first_iteration || max_ext < phrase.ext) max_ext = phrase.ext;
            if (first_iteration) first_iteration = false;
        }

        min_ext = std::abs(min_ext);

        bits_per_extension = std::bit_width(static_cast<uint64_t>(min_ext) + max_ext);
        packed_extensions = new Pack[word_packing::num_packs_required<Pack>(z, bits_per_extension)];
        auto extensions = word_packing::accessor(packed_extensions, bits_per_extension);

        # ifdef END_POSITION_PACK
        // end positions
        bits_per_end_positions = std::bit_width(static_cast<uint64_t>(n));
        packed_end_positions = new Pack[word_packing::num_packs_required<Pack>(z, bits_per_end_positions)];
        auto end_positions = word_packing::accessor(packed_end_positions, bits_per_end_positions);
        # else
        end_positions.reserve(z);
        # endif

        Index current_n = -1;
        Index i = 0;
        for (auto phrase : phrases) {
            current_n += phrase.len; // add phrase length to text length
            extensions[i]= phrase.ext + min_ext; // store the extensions of phrases
            sources[i] = phrase.lnk; // store the sources of phrases
            # ifdef END_POSITION_PACK
            end_positions[i] = current_n; // store the end position of the source in the original text
            # else
            end_positions.push_back(current_n);
            # endif
            i++;
        }
    }

    // extracts an intervall of lzend compressed values
    // uses the iterative extraction method to extract values in the sequential case
    // and parallel_extract for the parallel extraction of values
    std::vector<Index> extract(Index start, Index len, uint16_t NUM_THREADS = 1) const {
        std::vector<Index> result;
        result.reserve(len);
        if(NUM_THREADS == 1) {
            iter_extract(start, len, result);
        } else {
            parallel_extract(start, len, result, NUM_THREADS);
        }
        return std::move(result);
    }

    inline Index extension(Index index) const {
        auto extensions = word_packing::accessor(packed_extensions, bits_per_extension);
        return extensions[index] - min_ext;
    }

    // Takes an index for a text position and returns the phrase the position is encoded in
    // This binary search takes O(log(z)) time, which in practice tends to be very efficient
    Index binary_phrase_search(Index const index) const {
        # ifdef END_POSITION_PACK
        auto end_positions = word_packing::accessor(packed_end_positions, bits_per_end_positions);
        # endif
        Index left = 0;
        Index right = z-1;
        Index m = right / 2; // m represents the phrase id which should contain the given index

        while ((m != 0 || index > end_positions[m]) &&                                  // if m == 0 and the index is the same as the end of phrase 0 or
                (m == 0 || index > end_positions[m] || end_positions[m-1] >= index)) {  // if m>0 and the index is smaller or equal to the end position of phrase m and the end position of the phrase before m is smaller than the index, the correct phrase id is found
            if (index < end_positions[m]) { 
                // the searched phrase will be before phrase m
                right = m - 1;
                m = left + (m - left) / 2;
            } else { 
                // the searched phrase will be after phrase m
                left = m + 1;
                m = m + (right - m + 1) / 2;
            }
        }

        assert(m == 0 && index == 0 || end_positions[m] >= index && end_positions[m-1] < index); // assert if the correct phrase was found
        
        return m;
    }

    // Wrapper for recursive extraction method
    // can have problems with recursion depth, is therefore not the standard extraction method
    std::vector<Index> recursive_extract(Index start, Index len) const {
        # ifdef END_POSITION_PACK
        auto end_positions = word_packing::accessor(packed_end_positions, bits_per_end_positions);
        # endif

        assert(start >= 0 && start < n);
        assert(len > 0 && start + len - 1 < n);
        std::vector<Index> result;
        result.reserve(len);
        const Index phrase_id = binary_phrase_search(start + len - 1);
        assert(phrase_id == 0 && start + len - 1 == 0 || end_positions[phrase_id] >= start + len - 1 && end_positions[phrase_id-1] < start + len - 1);
        assert(phrase_id >= 0 && phrase_id < z);
        recursive_extract(start, len, phrase_id, &result);
        return result;
    }

    // can have problems with recursion depth, is therefore not the standard extraction method
    void recursive_extract(Index start, Index len, Index const phrase_id, std::vector<Index>* result) const {
        # ifdef END_POSITION_PACK
        auto end_positions = word_packing::accessor(packed_end_positions, bits_per_end_positions);
        # endif

        if (len <= 0) {
            return;
        }

        Index end = start + len - 1;
        assert(phrase_id == 0 && end == 0 || end_positions[phrase_id] >= end && end_positions[phrase_id-1] < end); // assert if the correct phrase was given

        // check if the current index is still in the correct phrase_id ??
        if (end_positions[phrase_id] == end) {
            // the current index is the end of a phrase (an extension)
            // if the end position of the previous phrase is equal to the value before end,
            // i.e. the the current phrase has len == 1, then the previous value must be in the previous phrase
            Index new_phrase_id  = phrase_id;
            //std::cout << "\textracted " << extension(phrase_id) << std::endl;
            if (phrase_id != 0 && end_positions[phrase_id - 1] == end - 1) {
                --new_phrase_id;
                //std::cout << "\tone->p=" << new_phrase_id << ", ep[p]=" << end_positions[new_phrase_id] << std::endl;
            }
            recursive_extract(start, len - 1, new_phrase_id, result);
            assert(phrase_id >= 0 && phrase_id < z);
            result->push_back(extension(phrase_id));
        } else {
            assert(phrase_id != 0); // disable with NDEBUG for measurements or production
            // we can assume, that phrase_id != 0 here, because otherwise <end> would be an extension
            Index pos = end_positions[phrase_id - 1] + 1; // first position of the current phrase
            
            if (start < pos) {
                // part of the desired interval is not contained in the current phrase
                // extract this part in the previous phrase(s)
                recursive_extract(start, pos - start, phrase_id - 1, result);
                len = end - pos + 1;
                start = pos;
            }
            // extract the rest of the current phrase (phrase_id) by referencing the source phrases recursively
            auto sources = word_packing::accessor(packed_sources, bits_per_source);
            Index src = sources[phrase_id];
            Index new_start = end_positions[src] - end_positions[phrase_id] + start + 1;
            while (src != 0 && end_positions[src - 1] >= new_start + len - 1) {
                // the desired value is not encoded in the phrase with phrase_id
                // decrement the phrase_id until the correct phrase is found
                //std::cout << "\tsrc--=" << src - 1 << std::endl;
                src--;
            }
            //std::cout << "\tp=" << src << ", ep[p]=" << end_positions[src] << std::endl;
            assert(src == 0 || end_positions[src] >= new_start + len - 1 && end_positions[src-1] < new_start + len -1);
            recursive_extract(new_start, len, src, result);
        }
    }

    // iterative extraction method, used for sequential extraction
    void iter_extract(Index start, Index len, std::vector<Index>& result) const {
        # ifdef END_POSITION_PACK
        auto end_positions = word_packing::accessor(packed_end_positions, bits_per_end_positions);
        # endif
        auto sources = word_packing::accessor(packed_sources, bits_per_source);
        auto ext = word_packing::accessor(packed_extensions, bits_per_extension);

        Index end = start + len - 1;
        Index phrase_id = binary_phrase_search(end);

        struct SearchInterval {
            Index start;
            Index end;
            Index phrase_id;
        };
        
        std::vector<SearchInterval> intervals;
        intervals.reserve(32);
        intervals.push_back({start, end, phrase_id});

        // to avoid recusion, we simply store the starting- & endpositions and the corresponding phrase id
        //  in a stack
        //Index original_start = start;
        //Index pos, src, phrase_pos_shift, phrase_id;
        Index pos, src, phrase_pos_shift;

        while (!intervals.empty()) {
            SearchInterval current_interval = intervals.back();
            intervals.pop_back();

            start = current_interval.start;
            end = current_interval.end;
            phrase_id = current_interval.phrase_id;

            if (phrase_id > 0) {
                pos = end_positions[phrase_id - 1] + 1; // first position of the current phrase
            } else {
                pos = 0;
            }

            if (start < pos) {
                intervals.push_back({start, pos - 1, phrase_id - 1});
                start = pos;
            }

            while (end >= start) {

                if (end_positions[phrase_id] == end) {
                    result.push_back(ext[phrase_id] - min_ext);
                    if (phrase_id != 0 && end_positions[phrase_id - 1] == end - 1) {
                        --phrase_id;
                    }
                    --end;
                } else {
                    src = sources[phrase_id];

                    //Index new_start = end_positions[src] - end_positions[phrase_id] + start + 1;
                    phrase_pos_shift = - end_positions[phrase_id] + end_positions[src] + 1;
                    start += phrase_pos_shift;
                    end += phrase_pos_shift;

                    while (src != 0 && end_positions[src - 1] >= end) {
                        src--;
                    }
                    phrase_id = src;

                    // make pos
                    if (phrase_id > 0 && start <= end_positions[phrase_id - 1]) {
                        pos = end_positions[phrase_id - 1] + 1; // first position of the current phrase
                        intervals.push_back({start, pos - 1, phrase_id - 1});
                        start = pos;
                    }
                }
            }
        }

        std::reverse(result.begin(), result.end());
    }

    // parallel extraction of a interval
    // calls itself in a new thread if there is an extraction task above the value of treshold
    void parallel_extract(Index start, Index end, Index phrase_id, std::vector<Index> &result, Index resultIndex) const {
        const int treshold = 1000000;
        # ifdef END_POSITION_PACK
        auto end_positions = word_packing::accessor(packed_end_positions, bits_per_end_positions);
        # endif
        auto sources = word_packing::accessor(packed_sources, bits_per_source);
        auto ext = word_packing::accessor(packed_extensions, bits_per_extension);

        Index pos, src, phrase_pos_shift;

        if (phrase_id > 0) {
            pos = end_positions[phrase_id - 1] + 1; // first position of the current phrase
        } else {
            pos = 0;
        }

        if (start < pos) {
            if (pos - start >= treshold) {
                #pragma omp task
                {
                    parallel_extract(start, pos - 1, phrase_id - 1, result, resultIndex - end + pos - 1);
                }
            } else {
                parallel_extract(start, pos - 1, phrase_id - 1, result, resultIndex - end + pos - 1);
            }
            start = pos;
        }

        while (end >= start) {

            if (end_positions[phrase_id] == end) {

                result[resultIndex] = ext[phrase_id] - min_ext;
                resultIndex--;
                if (phrase_id != 0 && end_positions[phrase_id - 1] == end - 1) {
                    --phrase_id;
                }
                --end;
            } else {
                src = sources[phrase_id];

                phrase_pos_shift = - end_positions[phrase_id] + end_positions[src] + 1;
                start += phrase_pos_shift;
                end += phrase_pos_shift;

                while (src != 0 && end_positions[src - 1] >= end) {
                    src--;
                }
                phrase_id = src;
                if (phrase_id > 0 && start <= end_positions[phrase_id - 1]) {
                    pos = end_positions[phrase_id - 1] + 1; // first position of the current phrase
                    if (pos - start >= treshold) {
                        #pragma omp task
                        {
                            parallel_extract(start, pos - 1, phrase_id - 1, result, resultIndex - end + pos - 1);
                        }
                    } else {
                        parallel_extract(start, pos - 1, phrase_id - 1, result, resultIndex - end + pos - 1);
                    }
                    start = pos;
                }
            }
        }
    }

    /* Entry point for the parallel extraction of a interval.
    Execute the extraction of subintervals in a new thread if the value is above a certain treshold,
    that is calculated of the NUM_THREADS which the method should use.
    The idea behind this parallelization is that if the interval lies in multiple phrase, then we can
     create a new thread to execute the extraction of the first phrase while the algorithm simultaneously
      continues with extracting the other phrases. */
    void parallel_extract(Index start, Index len, std::vector<Index> &result, uint16_t NUM_THREADS = 16) const { // todo check for full correctness
        omp_set_num_threads(NUM_THREADS);
        # ifdef END_POSITION_PACK
        auto end_positions = word_packing::accessor(packed_end_positions, bits_per_end_positions);
        # endif
        auto sources = word_packing::accessor(packed_sources, bits_per_source);
        auto ext = word_packing::accessor(packed_extensions, bits_per_extension);

        Index end = start + len - 1;
        Index phrase_id = binary_phrase_search(end);

        result.resize(len);
        std::fill(result.begin(), result.end(), 0);

        const int64_t treshold = std::max(len / (static_cast<int64_t>(NUM_THREADS) * 10), static_cast<int64_t>(2500));
        Index dp = phrase_id;

        #pragma omp parallel
        {
            #pragma omp single
            {
                while(end >= start) {
                    Index pos = end_positions[phrase_id - 1] + 1; // first position of the current phrase
                    phrase_id--;
                    if (end - pos + 1 >= treshold || pos < start || phrase_id < 1) {
                        #pragma omp task
                        {
                            parallel_extract(start > pos ? start : pos , end, dp, result, end - start);
                        }
                        dp = phrase_id;
                        end = pos -1;
                    }
                }
            }
        }
    }

    Index get_z() const {
        return z;
    }

    Index get_n() const {
        return n;
    }

    Index get_end_position(Index i) const {
        return end_positions[i];
    }

    size_t memory_usage() const {
        size_t base_size = sizeof(*this);
        size_t sources_size = sizeof(Pack) * word_packing::num_packs_required<Pack>(z, bits_per_source);
        # ifdef END_POSITION_PACK
        size_t end_positions_size = sizeof(Pack) * word_packing::num_packs_required<Pack>(z, bits_per_end_positions);
        # else
        size_t end_positions_size = sizeof(Index) * z;
        # endif
        size_t extensions_size = sizeof(Pack) * word_packing::num_packs_required<Pack>(z, bits_per_extension);

        # ifdef DDEBUG
        std::cout << "\t\tspace usage base encoding: " << sizeof(*this) << " bytes" << std::endl;
        std::cout << "\t\tspace usage source: " << sources_size << " bytes" << std::endl;
        std::cout << "\t\tspace usage end_positions: " << end_positions_size << " bytes" << std::endl;
        std::cout << "\t\tspace usage extensions: " << extensions_size << " bytes" << std::endl;
        # endif

        return base_size + sources_size + end_positions_size + extensions_size;
    }

    void load(std::istream &in) {
        in.read((char*)&n, sizeof(n));
        in.read((char*)&z, sizeof(z));
        in.read((char*)&min_ext, sizeof(min_ext));
        in.read((char*)&bits_per_source, sizeof(bits_per_source));
        in.read((char*)&bits_per_extension, sizeof(bits_per_extension));

        packed_extensions = new Pack[word_packing::num_packs_required<Pack>(z, bits_per_extension)];
        in.read((char*)packed_extensions, sizeof(Pack) * word_packing::num_packs_required<Pack>(z, bits_per_extension));

        packed_sources = new Pack[word_packing::num_packs_required<Pack>(z, bits_per_source)];
        in.read((char*)packed_sources, sizeof(Pack) * word_packing::num_packs_required<Pack>(z, bits_per_source));

        # ifdef END_POSITION_PACK
        in.read((char*)&bits_per_end_positions, sizeof(bits_per_end_positions));
        packed_end_positions = new Pack[word_packing::num_packs_required<Pack>(z, bits_per_end_positions)];
        in.read((char*)packed_end_positions, sizeof(Pack) * word_packing::num_packs_required<Pack>(z, bits_per_end_positions));
        # else
        end_positions.clear();
        end_positions.reserve(z);
        for (Index i = 0; i < z; i++) {
            Index end_position;
            in.read((char*)&end_position, sizeof(Index));
            end_positions.push_back(end_position);
        }
        # endif
    }

    uint64_t serialize(std::ostream& out) {
        uint64_t out_size = 0;

        out.write((char*)&n, sizeof(n));
        out.write((char*)&z, sizeof(z));
        out.write((char*)&min_ext, sizeof(min_ext));
        out.write((char*)&bits_per_source, sizeof(bits_per_source));
        out.write((char*)&bits_per_extension, sizeof(bits_per_extension));

        out_size += sizeof(n) + sizeof(z) + sizeof(min_ext) + sizeof(bits_per_source) + sizeof(bits_per_extension);
        
        out.write((char*)packed_extensions, sizeof(Pack) * word_packing::num_packs_required<Pack>(z, bits_per_extension));
        out.write((char*)packed_sources, sizeof(Pack) * word_packing::num_packs_required<Pack>(z, bits_per_source));

        out_size += sizeof(Pack) * (word_packing::num_packs_required<Pack>(z, bits_per_extension) + word_packing::num_packs_required<Pack>(z, bits_per_source));

        # ifdef END_POSITION_PACK
        out.write((char*)&bits_per_end_positions, sizeof(bits_per_end_positions));
        out.write((char*)packed_end_positions, sizeof(Pack) * word_packing::num_packs_required<Pack>(z, bits_per_end_positions));
        # else
        for (Index end_position : end_positions) {
            out.write((char*)&end_position, sizeof(Index));
            out_size += sizeof(Index);
        }
        # endif

        return out_size;
    }
};
}

# endif