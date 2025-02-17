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

#include <iostream>
#include <fstream>
#include <set>
#include <cstdint>
#include <string>

#include "malloc_count.h"

#include "lzendri.hpp"
#include "word_packing_encoding.hpp"
#include "utils/cli.hpp"
#include "utils/time.hpp"
#include "utils/definitions.hpp"
#include "utils.hpp"

void help() {
    std::cout << "lzendri-locate: locates all occurences of the provided patterns." << std::endl << std::endl;
    
    std::cout << "Usage: lzendri-build [options] <lzendri file> <pattern file>" << std::endl;

    std::cout << "\t<lzendri>       path to the computed lzendri index (file with extension .lzendri)" << std::endl;
    std::cout << "\t<pattern>       path to the pattern file in the pizza&chili format" << std::endl;
    std::cout << "\t-t              number of threads used for extracting lz-end-values (default: only use one thread)" << std::endl;
    std::cout << "\t(-h             sets h only for the RESULT line)" << std::endl;
    std::cout << "\t(-filename      sets the filename only for the RESULT line)" << std::endl;
}

template<typename IntWord, typename Enc>
void locate(std::ifstream& lzendri_in, std::ifstream& pattern_in, std::string &filename, int32_t h, uint16_t NUM_THREADS) {

    size_t pre_load_memory = malloc_count_current();

    std::cout << "Loading lzendri index..." << std::flush;
    Lzendri<IntWord, Enc> lzendri;
    lzendri.load(lzendri_in);
    std::cout << " done." << std::endl;

    size_t lzendri_size = malloc_count_current() - pre_load_memory;

    std::cout << "Loading patterns..." << std::flush;

    std::string pattern_header;
    std::getline(pattern_in, pattern_header);
    uint64_t pattern_length  = get_pattern_length(pattern_header);
    uint64_t pattern_count = get_pattern_count(pattern_header);

    std::vector<std::string> patterns = load_patterns(pattern_in, pattern_length, pattern_count);

    std::cout << " found " << pattern_count << " patterns of length " << pattern_length << "." << std::endl;

    std::cout << "Locate..." << std::flush;
    int64_t occ_count = 0;
    auto timer_locate = timestamp();

    for (std::string p : patterns) {
        std::vector<IntWord> x = lzendri.locate(p, NUM_THREADS);
        occ_count += x.size();
    }

    auto td_locate = timestamp() - timer_locate;

    std::cout << "Located " << patterns.size() << " patterns (with " << occ_count << " occurences) in " << td_locate << " ms" << std::endl;

    std::string dev_null = "/dev/null";
    uint64_t index_size = lzendri.save(dev_null);

    std::cout << "RESULT"
        << " algo=lzendri_locate"
        << " tp_us=" << (double) pattern_count / ((double) td_locate * (double) 1000)
        << " tp_ms=" << (double) pattern_count / ((double) td_locate)
        << " idx_size_bn=" << (double) index_size / (double) lzendri.get_n()
        << " time_ms=" << td_locate
        << " time_per_occ_ns=" << ((double) td_locate * 1000000) / occ_count
        << " occ=" << occ_count
        << " file=" << filename
        << " m=" << pattern_length
        << " h=" << h
        << " t=" << NUM_THREADS
        << " index_size_malloc=" << lzendri_size
        << " index_size_written=" << index_size
        << " bits_per_symbol=" << (double) index_size * 8 / (double) lzendri.get_n()
        << std::endl << std::endl << std::flush;
}

int main(int argc, char** argv) {
    std::set<std::string> allowed_value_options;
    std::set<std::string> allowed_literal_options;

    allowed_value_options.insert("-h");
    allowed_value_options.insert("-filename");
    allowed_value_options.insert("-t");

    CommandLineArguments a = parse_args(argc, argv, allowed_value_options, allowed_literal_options, 2);

    if (!a.success) {
        help();
        return -1;
    }

    int32_t h = 0;
    std::string filename = a.last_parameter.at(0);
    uint16_t NUM_THREADS = 1;
    for (Option value_option : a.value_options) {
        if (value_option.name == "-h") {
            h = std::stoi(value_option.value);
        }
        if (value_option.name == "-filename") {
            filename = value_option.value;
        }
        if (value_option.name == "-t") {
            int64_t t = std::stol(value_option.value);
            if (t < 1 || t > std::numeric_limits<uint16_t>::max()) {
                std::cout << "t has to be in the range of 1 to " << std::numeric_limits<uint16_t>::max() << ". Continuing with t=1" << std::endl << std::flush;
            } else {
                NUM_THREADS = static_cast<uint16_t>(t);
            }
        }
    }

    std::string lzendri_file = a.last_parameter.at(0);
    std::string pattern_file = a.last_parameter.at(1);

    std::ifstream lzendri_in(lzendri_file);
    std::ifstream pattern_in(pattern_file);

    uint8_t long_integer_flag;
    lzendri_in.read((char*) &long_integer_flag, sizeof(uint8_t));

    if (long_integer_flag == 0) {
        locate<int32_t, lzend::WordPackingEncoding<int32_t>>(lzendri_in, pattern_in, filename, h, NUM_THREADS);
    } else {
        locate<int64_t, lzend::WordPackingEncoding<int64_t>>(lzendri_in, pattern_in, filename, h, NUM_THREADS);
    }
}