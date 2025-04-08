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

 #ifndef _LZENDSA_HPP
 #define _LZENDSA_HPP
 
 #include <cmath>
 #include <vector>
 #include <numeric>
 #include <iostream>
 #include <type_traits>
 #include <cassert>
 #include <ostream>
 #include <fstream>
 #include <memory>
 #include <bit>
 #include <type_traits>
 
 #include "integer_lzend.hpp"
 #include "libsais_wrapper.hpp"
 #include "utils/time.hpp"
 #include "utils/definitions.hpp"
 
template<typename IntWord, typename E>
class Lzendsa {
protected:
    static_assert(
        std::is_same_v<IntWord, int32_t> ||
        std::is_same_v<IntWord, int64_t>);

public:

    void build_encoding(std::unique_ptr<IntWord[]> sa, IntWord n, IntWord h = -1) {
        auto timer_parse = timestamp();
        std::vector<lzend::IntPhrase<IntWord>> phrases = lzend::parse<IntWord>(sa, n, false, h);
        auto td_parse = timestamp() - timer_parse;
        std::cout << "RESULT "
            << " what=parsing_time"
            << " algo=lzendsa"
            << " file=" << filename
            << " time_ms=" << td_parse
            << std::endl;
        encoding = std::make_unique<E>();
        encoding->build(phrases);
    }

    // call when the suffix array of the input text is not yet calculated
    void load(std::string &s, int64_t d = -1, int64_t h = -1, std::string filename = "none") {
        IntWord text_length = s.length();

        # ifdef INFOS
        std::cout << "Free up space for Suffix Array" << std::endl;
        std::cout.flush();
        # endif
        auto sa = std::make_unique<IntWord[]>(text_length);

        # ifdef INFOS
        std::cout << "Calculate Suffix-Array";
        std::cout.flush();
        auto timer_sa = timestamp();
        # endif
        lib::sais((uint8_t const*) s.data(), sa.get(), text_length);
        
        // discard text
        s = "";
        
        # ifdef INFOS
        auto td_sa = timestamp() - timer_sa;
        std::cout << " ... construction done in " << td_sa << " ms" << std::endl;
        std::cout.flush();
        # endif

        load(std::move(sa), text_length, d, h, filename);
    }

    void load(std::unique_ptr<IntWord[]> sa, IntWord sa_size, int64_t d = -1, int64_t h = -1, std::string filename = "none") {
        n = sa_size;

        std::vector<lzend::IntPhrase<IntWord>> phrases;

        if (d > 0) {
            // a delta value was provided
            // we can take the sample before computing the lzend parsing
            delta = d;
            sample_size = std::ceil((double)n / (double)delta);

            sample = std::make_unique<IntWord[]>(sample_size);
            for (IntWord j = 0; j < sample_size; j++) {
                sample[j] = sa[j * delta];
            }

            // transform suffix array into the differential suffix array
            for (IntWord i = n - 1; i > 0; i--) {
                sa[i] = sa[i] - sa[i-1];
            }

            auto timer_parse = timestamp();
            phrases = lzend::parse<IntWord>(sa, n, false, h);
            auto td_parse = timestamp() - timer_parse;
            std::cout << "RESULT "
                << " what=parsing_time"
                << " algo=lzendsa"
                << " file=" << filename
                << " time_ms=" << td_parse
                << std::endl;
        } else {
            // no delta value was provided
            // we compute the lzend parsing of the differential sa before taking the sample, because we use z as a measure for the sample size/delta value
            std::unique_ptr<IntWord[]> dsa = std::make_unique<IntWord[]>(n);
            if (n > 0) dsa[0] = sa[0];
            for (IntWord i = n - 1; i > 0; i--) {
                dsa[i] = sa[i] - sa[i-1];
            }

            auto timer_parse = timestamp();
            phrases = lzend::parse<IntWord>(dsa, n, false, h);
            dsa.reset();
            auto td_parse = timestamp() - timer_parse;
            std::cout << "RESULT "
                << " what=parsing_time"
                << " algo=lzendsa"
                << " file=" << filename
                << " time_ms=" << td_parse
                << std::endl;

            IntWord z = phrases.size();
            size_t bits_per_source = std::bit_width(static_cast<uint64_t>(z));
            size_t bits_per_extension = std::bit_width(static_cast<uint64_t>(2 * n));

            // we choose the sample to be approx. the size of 10 % of the lzend compressed dsa
            sample_size = (z * (bits_per_source + bits_per_extension + sizeof(IntWord) * 8)) / (8 * sizeof(IntWord) * 10);
            delta = std::ceil((double) n / (double)sample_size);
            sample_size = std::ceil((double)n / (double)delta);

            // store the sample values
            sample = std::make_unique<IntWord[]>(sample_size);
            for (IntWord j = 0; j < sample_size; j++) {
                sample[j] = sa[j * delta];
            }
        }
        sa.reset();

        // construct encoding
        encoding = std::make_unique<E>();
        encoding->build(phrases);
    }

    // returns multiple consecutive suffix array values (which is faster than extracting all of them at once)
    std::vector<IntWord> sa_values(IntWord start, IntWord length, uint16_t NUM_THREADS = 1) const {
        assert(start >= 0 && length > 0 && start + length - 1 < n);
        IntWord lower_sample_index = start - start % delta; // find next lowest element in the sample before start (or start is this index)
        IntWord upper_sample_index = start + (start % delta == 0 ? 0 : (delta - start % delta)); // find next highest element in the sample after start

        IntWord end = start + length - 1;

        // determine wheter to use the lower of upper sample
        IntWord lower_extraction_length = start - lower_sample_index;
        IntWord upper_extraction_length = upper_sample_index - end;

        std::vector<IntWord> result(length);

        if (upper_sample_index <= end) {
            // Case A: one or more sample values are present in the desired interval
            std::vector<IntWord> decoded_result = encoding->extract(start + 1, length-1, NUM_THREADS);
            IntWord upper_sample_value = sample[upper_sample_index / delta];
            result[upper_sample_index - start] = upper_sample_value;
            IntWord current_value = upper_sample_value;
            for (IntWord i = upper_sample_index - 1; i >= start; i--) { // set all values from the upper sample to the start of the interval
                current_value -= decoded_result[i - start];
                assert(current_value >= 0 && current_value < n);
                result[i - start] = current_value;
            }

            current_value = upper_sample_value;
            for (IntWord i = upper_sample_index + 1; i <= end; i++) {
                current_value += decoded_result[i - start - 1];
                assert(current_value >= 0 && current_value < n);
                result[i - start] = current_value;
            }
        } else if (lower_extraction_length <= upper_extraction_length || upper_sample_index / delta > sample_size) {
            // Case B: the next sample value is before the interval (or the value before the interval is the last sample value)
            std::vector<IntWord> decoded_result = encoding->extract(lower_sample_index + 1, end - lower_sample_index, NUM_THREADS);
            IntWord current_value = sample[lower_sample_index / delta];
            for (IntWord i = lower_sample_index + 1; i <= end; i++) {
                current_value += decoded_result[i - lower_sample_index - 1];
                assert(current_value >= 0 && current_value < n);
                if (i >= start) result[i - start] = current_value;
            }
        } else {
            // Case C: the next sample value is after the interval
            std::vector<IntWord> decoded_result = encoding->extract(start + 1, upper_sample_index - start, NUM_THREADS);
            IntWord current_value = sample[upper_sample_index / delta];
            for (IntWord i = upper_sample_index - 1; i >= start; i--) {
                current_value -= decoded_result[i - start];
                assert(current_value >= 0 && current_value < n);
                if (i <= end) result[i - start] = current_value;
            }
        }

        return result;
    }

    IntWord operator[](IntWord index) const {
        std::vector<IntWord> result = sa_values(index, 1, 1);
        assert(result.size() == 1);
        return result[0];
    }

    // counts the occurences of the provided pattern, i.e. calculates the suffix array interval
    // where the occurences of the patterns are located
    Interval count(const uint8_t* text, std::vector<uint8_t> &pattern) const {
        // do binary search on the uncompressed samples first

        // first establish a lower bound for the samples, the result is a sample index, that
        //  1. contains the text position which is lexicographically equal or greater than the pattern and
        //  2. the sample value before contains a text position that is lex. smaller than the pattern
        IntWord lower_sample_bound = binary_search_sample(0, sample_size - 1, text, pattern, true);

        // when the appproximate bounds are found, we can continue to find the exact start point of the interval
        // the exact boundary lies in the interval from the previous sample (exl.) value to the current (incl.)
        IntWord exact_lower_boundary;
        IntWord start;
        if (lower_sample_bound > 0) { 
            // we want to search in the interval before the lower bound sample
            start = (lower_sample_bound - 1) * delta + 1;
        } else {
            // if the lower bound sample is the first sample, then we have to search in interval after the sample
            start = 0;
        }
        // extract all original suffix array values in the interval between the two sample values
        std::vector<IntWord> lower_interval = sa_values(start, delta + start - 1 < n ? delta : n - start);
        // search in this interval for the exact start position
        exact_lower_boundary = start + binary_search_vector(lower_interval, text, pattern, true);

        // check if pattern was found
        if (compare_position_with_pattern(lower_interval[exact_lower_boundary - start], text, pattern) != 0) {
            // there is no matching pattern
            return {-1, -1};
        }

        // establiash a upper bound for the sample, i.e. a sample index, that
        //  1. contains the text position which is lex. equal or smaller than the pattern and
        //  2. the next sample value refers to a text position that is lex. greater than the pattern
        IntWord upper_sample_bound = binary_search_sample(lower_sample_bound / 2, sample_size - 1, text, pattern, false);
        
        // now we can find the exact value of the upper interval
        IntWord exact_upper_boundary = upper_sample_bound * delta;
        if (upper_sample_bound * delta < n - 1) {
            // the upper sample value is not the last value of the suffix array
            // determine exact boundary in the interval from the upper sample value to the next sample value
            IntWord upper_start = upper_sample_bound * delta;
            std::vector<IntWord> upper_interval;
            upper_interval.reserve(delta);
            if (upper_start + 1 == start) {
                upper_interval.push_back(upper_sample_bound);
                upper_interval.insert(upper_interval.end(), lower_interval.begin(), lower_interval.end());
                upper_interval.pop_back();
            } else {
                upper_interval = sa_values(upper_start, delta + upper_start - 1 < n ? delta : n - upper_start);
            }
            exact_upper_boundary = upper_start + binary_search_vector(upper_interval, text, pattern, false);
        }

        return {exact_lower_boundary, exact_upper_boundary};
    }

    /*
    * Compares the suffix starting at position <index_in_text> with the pattern.
    * Returns -1 if the pattern is lexicographically smaller than the text position,
    * 0 if they are equal and
    * 1 if the pattern is lex. greater than the text position.
    */
    inline int compare_position_with_pattern(IntWord index_in_text, const uint8_t* text, std::vector<uint8_t> &pattern) const {
        IntWord i = 0;
        while(i < pattern.size() && index_in_text + i < n) { // ensures that the index is in text
            if (pattern[i] > text[index_in_text + i]) { // pattern begins right of m
                return 1;
            } else if (pattern[i] < text[index_in_text + i]) { // patterns begins left of m
                return -1;
            }
            i++;
        }
        if (i < pattern.size()) { 
            // the suffix at the text position is not long enough, so the suffix is lex. smaller
            return 1;
        }

        return 0;
    }

    // Searches for the pattern in the specified suffix array interval <v>.
    // If <find_lower_bound> is true, then the text index whose prefix in the text is equal to the pattern
    // (only considerend the first pattern_size characters) with a lex. smaller predecessor is searched for.
    // If <find_lower_bound> is false, then the text index whose prefix in the text is equal to the pattern
    // with a lex. greater successor is searched for
    IntWord binary_search_vector(std::vector<IntWord> v, const uint8_t* text, std::vector<uint8_t> &pattern, bool find_lower_bound) const {
        IntWord l = 0;
        IntWord r = v.size() - 1;
        IntWord m = l + (r - l) / 2;
        while(l < r) {
            int compared = compare_position_with_pattern(v[m], text, pattern);
            if (compared == 1) {
                // the text is lex. smaller than the pattern at position m
                l = m + 1;
            } else if (compared == -1) {
                // the text is lex. greater than the pattern at position m
                r = m - 1;
            } else {
                // compare with the successor or predecessor index (depending of <find_lower_bound>)
                IntWord neighbor_index = m + (find_lower_bound ? -1 : 1);
                if (neighbor_index < 0 || neighbor_index >= v.size()) return m;
                int compared_with_neighbor = compare_position_with_pattern(v[neighbor_index], text, pattern);
                if (compared_with_neighbor == 1 && find_lower_bound || compared_with_neighbor == -1 && !find_lower_bound) {
                    return m;
                } else { // if equal or pattern lex. smaller/greater
                    if (find_lower_bound) r = m - 1;
                    if (!find_lower_bound) l = m + 1;;
                }
            }
            m = l + (r - l) / 2;
        }

        return m;
    }

    // Search for the pattern in the sample of the suffix array.
    IntWord binary_search_sample(IntWord l, IntWord r, const uint8_t* text, std::vector<uint8_t> &pattern, bool find_lower_bound) const {
        IntWord m = l + (r - l) / 2;
        while(l < r) {
            int compared = compare_position_with_pattern(sample[m], text, pattern);
            if (compared == 1 && find_lower_bound) {
                l = m + 1;

            } else if (compared == -1 && !find_lower_bound) {
                r = m - 1;
            } else {
                IntWord neighbor_index = m + (find_lower_bound ? -1 : 1);
                if (neighbor_index < 0 || neighbor_index >= sample_size) return m;
                int compared_with_neighbor = compare_position_with_pattern(sample[neighbor_index], text, pattern);
                if (compared_with_neighbor == 1 && find_lower_bound || compared_with_neighbor == -1 && !find_lower_bound) {
                    return m;
                } else { // if equal or pattern lex. smaller/greater
                    if (find_lower_bound) r = m - 1;
                    if (!find_lower_bound) l = m + 1;;
                }
            }
            m = l + (r - l) / 2;
        }

        assert(0 <= m && m < sample_size);
        return m;
    }

    // locate a pattern
    std::vector<IntWord> locate(IntWord sp, IntWord ep, uint16_t NUM_THREADS = 1) {
        return sa_values(sp, ep - sp + 1, NUM_THREADS);
    }

    //locate directly
    std::vector<IntWord> locate(const uint8_t* text, std::vector<uint8_t> &pattern, uint16_t NUM_THREADS = 1) const {
        Interval interval = count(text, pattern);
        if (interval.sp == -1) {
            return std::vector<IntWord>();
        }

        return sa_values(interval.sp, interval.ep - interval.sp + 1, NUM_THREADS);
    }

    IntWord z() const {
        return encoding->get_z(); // return the number of phrases (is saved in the encoding)
    }

    IntWord d() const {
        return delta;
    }

    IntWord get_n() const {
        return n;
    }

    IntWord sa_size() {
        return n;
    }

    size_t memory_usage() const {
        size_t encoding_size = encoding->memory_usage();
        size_t base_size = sizeof(*this);
        size_t sample_space_usage = sizeof(IntWord) * sample_size;

        # ifdef DDEBUG
        std::cout << "\t\tspace usage encoding: " << encoding_size << " bytes" << std::endl;
        std::cout << "\t\tspace usage base: " << base_size << " bytes" << std::endl;
        std::cout << "\t\tspace usage sample: " << sample_space_usage << " bytes" << std::endl;
        std::cout << "\t\tspace usage construction: " << encoding_size + base_size + sample_space_usage << " bytes" << std::endl;
        # endif

        return encoding_size + base_size + sample_space_usage;
    }

    size_t encoding_size() {
        return encoding->memory_usage();
    }

    E get_encoding() { // only for testing
        return *(encoding.get());
    }

    // load the lzendsa construction from a stream
    void load(std::istream &in) {
        in.read((char*)&n, sizeof(n));
        in.read((char*)&sample_size, sizeof(sample_size));
        in.read((char*)&delta, sizeof(delta));

        sample = std::make_unique<IntWord[]>(sample_size);
        in.read((char*)sample.get(), sizeof(IntWord) * sample_size);

        encoding = std::make_unique<E>();
        encoding->load(in);
    }

    // serialize the lzendsa construction to the output stream
    uint64_t serialize(std::ostream& out) {
        uint64_t out_size = 0;

        out.write((char*)&n, sizeof(n));
        out.write((char*)&sample_size, sizeof(sample_size));
        out.write((char*)&delta, sizeof(delta));

        // write sample to output file
        out.write((char*)sample.get(), sizeof(IntWord) * sample_size);

        out_size += sizeof(n) + sizeof(sample_size) + sizeof(delta) + sizeof(IntWord) * sample_size;

        out_size += encoding->serialize(out);

        return out_size;
    }

    uint64_t save(std::string const &filename) {
        std::ofstream out(filename);
        uint64_t out_bytes = serialize(out);
        out.close();

        return out_bytes;
    }

private:
    void transform_to_differential(std::unique_ptr<IntWord[]> sa, IntWord n) {
        // transform suffix array into the differential suffix array
        for (IntWord i = n - 1; i > 0; i--) {
            sa[i] = sa[i] - sa[i-1];
        }
    }

    void transform_to_absolute(std::unique_ptr<IntWord[]> dsa, IntWord n) {
        for (IntWord i = 1; i < n; i++) {
            dsa[i] = dsa[i-1] + dsa[i];
        }
    }

    std::unique_ptr<E> encoding;
    IntWord n; // size of the suffix array/text
};

#endif