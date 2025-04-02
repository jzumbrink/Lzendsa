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

#include "utils/cli.hpp"
#include "utils/time.hpp"
#include "lzendsa.hpp"
#include "word_packing_encoding.hpp"
#include "measurements/permuted_indices.cpp"

void help() {
    std::cout << "lzendsa-ra: measures the random access time of the lzendsa construction." << std::endl << std::endl;
    
    std::cout << "Usage: lzendsa-ra [options] <lzendsa file> <integer file>" << std::endl;
    std::cout << "\t<lzendsa file>     path to lzendsa file (should the binary representation of the lzendsa construction)." << std::endl;
    std::cout << "\t<integer file>     path to file which contains an integer value at each line." << std::endl;
    std::cout << "\t-t                 number of threads used for extracting lz-end-values (default: only use one thread)" << std::endl;
    std::cout << "\t-l                 interval length" << std::endl;
    std::cout << "\t(-filename         sets the filename, only for the RESULT line)" << std::endl;
    std::cout << "\t(-h                sets h, only for the RESULT line)" << std::endl;
}

template<typename IntWord, typename Enc>
void random_access(std::ifstream &lzendsa_in, std::vector<IntWord> &arbitrary_indices, std::string filename, int64_t h, int64_t interval_length, uint16_t NUM_THREADS) {
    std::cout << "Loading lzendsa..." << std::flush;
    Lzendsa<IntWord, Enc> lzendsa;
    lzendsa.load(lzendsa_in);
    std::cout << " done." << std::endl << std::flush;

    std::cout << "Start random access..." << std::flush;
    auto timer_random_access = timestamp();

    for (IntWord i = 0; i < arbitrary_indices.size(); i++) {
        std::vector<IntWord> _x = lzendsa.sa_values(arbitrary_indices[i], interval_length, NUM_THREADS);
    }

    auto td_random_access = timestamp() - timer_random_access;
    std::cout << " done." << std::endl << std::flush;

    std::cout << "Measured time random access: " << td_random_access << " ms (with " << arbitrary_indices.size() << " Iterations)" << std::endl;

    std::cout << "RESULT"
        << " algo=lzendsa_ra"
        << " tp_us=" << (double) arbitrary_indices.size() / ((double) td_random_access * 1000)
        << " iterations=" << arbitrary_indices.size()
        << " file=" << filename
        << " interval_length=" << interval_length
        << " h=" << h
        << " idx_size_bn=" << (double) lzendsa.memory_usage() / (double) lzendsa.get_n()
        << " time_ms=" << td_random_access
        << " query_time_us=" << ((double) td_random_access * 1000) / arbitrary_indices.size()
        << " n=" << lzendsa.get_n()
        << " num_threads=" << NUM_THREADS
        << " lzend_size=" << lzendsa.encoding_size()
        << std::endl;
    std::cout.flush();
}

int main(int argc, char** argv) {
    std::set<std::string> allowed_value_options;
    std::set<std::string> allowed_literal_options;

    allowed_value_options.insert("-l");
    allowed_value_options.insert("-h");
    allowed_value_options.insert("-filename");
    allowed_value_options.insert("-t");

    CommandLineArguments a = parse_args(argc, argv, allowed_value_options, allowed_literal_options, 2);

    if (!a.success) {
        help();
        return -1;
    }

    int64_t l = 1;
    int64_t h = -1;
    uint16_t NUM_THREADS = 1;
    std::string filename = a.last_parameter.at(0);
    for (Option value_option : a.value_options) {
        if (value_option.name == "-l") {
            l = std::stol(value_option.value);
        }
        if (value_option.name == "-h") {
            h = std::stol(value_option.value);
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

    std::string lzendsa_file = a.last_parameter.at(0);
    std::string arbitrary_indices_file = a.last_parameter.at(1);

    std::ifstream lzendsa_in(lzendsa_file);
    std::ifstream arbitrary_indices_in(arbitrary_indices_file);

    uint8_t long_integer_flag;
    lzendsa_in.read((char*) &long_integer_flag, sizeof(long_integer_flag));

    std::cout << "Loading Indices..." << std::flush;
    std::string last_index;
    if (long_integer_flag == 0) {
        std::vector<int32_t> arbitrary_indices;
        while(arbitrary_indices_in >> last_index) {
            arbitrary_indices.push_back(std::stoi(last_index));
        }
        std::cout << " done." << std::endl << std::flush;

        random_access<int32_t, lzend::WordPackingEncoding<int32_t>>(lzendsa_in, arbitrary_indices, filename, h, l, NUM_THREADS);
    } else {
        std::vector<int64_t> arbitrary_indices;
        while(arbitrary_indices_in >> last_index) {
            arbitrary_indices.push_back(std::stol(last_index));
        }
        std::cout << " done." << std::endl << std::flush;

        random_access<int64_t, lzend::WordPackingEncoding<int64_t>>(lzendsa_in, arbitrary_indices, filename, h, l, NUM_THREADS);
    }
}