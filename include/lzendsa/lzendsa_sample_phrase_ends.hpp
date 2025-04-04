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
 
 #include "integer_lzend.hpp"
 #include "libsais_wrapper.hpp"
 #include "utils/time.hpp"
 #include "utils/definitions.hpp"
 
 template<typename IntWord, typename E>
 class LzendsaSamplePhraseEnds {
 private:
     std::unique_ptr<E> encoding;
 
     std::vector<IntWord> sample_phrase_end;
     IntWord n; // size of the suffix array/text
 public:
 
     // call when the suffix array of the input text is not yet calculated
     void load(std::string &s, int64_t h = -1, std::string filename = "none") {
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
 
         load(std::move(sa), text_length, h, filename);
     }
 
     void load(std::unique_ptr<IntWord[]> sa, IntWord sa_size, int64_t h = -1, std::string filename = "none") {
         n = sa_size;
 
         std::vector<lzend::IntPhrase<IntWord>> phrases;
 
         // we compute the lzend parsing of the differential sa before taking the sample, because we use z as a measure for the sample size/delta value
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
 
         IntWord z = phrases.size();
 
         // construct encoding
         encoding = std::make_unique<E>();
         encoding->build(phrases);
 
         // make differential suffix array to original values
         for(IntWord i = 1; i < n; i++) {
             sa[i] = sa[i] + sa[i-1];
         }
 
         // store the sample values
         sample_phrase_end.reserve(z);
         for (IntWord p = 0; p < z; p++) {
             sample_phrase_end.push_back(sa[encoding->get_end_position(p)]);
         }
         sa.reset();
     }
 
     // returns multiple consecutive suffix array values (which is faster than extracting all of them at once)
     std::vector<IntWord> sa_values(IntWord start, IntWord length, uint16_t NUM_THREADS = 1) const {
         assert(start >= 0 && length > 0 && start + length - 1 < n);
         IntWord end = start + length - 1;
 
         IntWord phraseId = encoding->binary_phrase_search(end);
         IntWord newEnd = encoding->get_end_position(phraseId); // todo rewrite extract function for beginning at phrase end
 
         std::vector<IntWord> result(length);
 
         std::vector<IntWord> decoded_result = encoding->extract(start + 1, newEnd - start, NUM_THREADS);
 
         IntWord current_value = sample_phrase_end[phraseId];
         for (IntWord i = newEnd - 1; i >= start; i--) {
             current_value -= decoded_result[i - start];
             assert(current_value >= 0 && current_value < n);
             if (i <= end) result[i - start] = current_value;
         }
 
         return result;
     }
 
     IntWord operator[](IntWord index) const {
         std::vector<IntWord> result = sa_values(index, 1, 1);
         assert(result.size() == 1);
         return result[0];
     }
 
     // todo: rewrite count
 
     // locate a pattern
     std::vector<IntWord> locate(IntWord sp, IntWord ep, uint16_t NUM_THREADS = 1) {
         return sa_values(sp, ep - sp + 1, NUM_THREADS);
     }
 
     IntWord z() const {
         return encoding->get_z(); // return the number of phrases (is saved in the encoding)
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
         size_t sample_space_usage = sizeof(IntWord) * sample_phrase_end.size();
 
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
 
         size_t sample_size;
         in.read((char*)&sample_size, sizeof(sample_size));
         sample_phrase_end.resize(sample_size);
         in.read((char*)sample_phrase_end.data(), sample_size * sizeof(IntWord));
 
         encoding = std::make_unique<E>();
         encoding->load(in);
     }
 
     // serialize the lzendsa construction to the output stream
     uint64_t serialize(std::ostream& out) {
         uint64_t out_size = 0;
 
         out.write((char*)&n, sizeof(n));
 
         size_t sample_size = sample_phrase_end.size();
         out.write((char*)&sample_size, sizeof(sample_size));
         out.write((char*)sample_phrase_end.data(), sample_size * sizeof(IntWord));
         out_size += sizeof(sample_size) + sample_size * sizeof(IntWord);
 
         out_size += encoding->serialize(out);
 
         return out_size;
     }
 
     uint64_t save(std::string const &filename) {
         std::ofstream out(filename);
         uint64_t out_bytes = serialize(out);
         out.close();
 
         return out_bytes;
     }
 };
 
 #endif