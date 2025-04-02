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
#include "malloc_count.h"

#include <iostream>
#include <fstream>
#include <set>
#include <cstdint>
#include <string>

#include "lzendsa.hpp"
#include "word_packing_encoding.hpp"
#include "utils/cli.hpp"
#include "utils/time.hpp"

void help() {
    std::cout << "lzendsa-build: builds the Lzendsa-index from the input file." << std::endl << std::endl;
    
    std::cout << "Usage: lzendsa-build [options] <text file>" << std::endl;
    std::cout << "\t<text file>     path to the input file (should contain text)" << std::endl;
    std::cout << "\t-o              path to the desired output file (the extension .lzendsa will be added automatically)" << std::endl;
    std::cout << "\t-h              longest phrase length, leave blank or put -1 for unbounded phrase length" << std::endl;
    std::cout << "\t-d              delta, if not provided the sample will be about 10% of the index size" << std::endl;
    std::cout << "\t--f64           explicitly use 64-bit-integers regardless of the file size" << std::endl;
    std::cout << "\t(-filename      sets the filename only for the RESULT line)" << std::endl;
}

int main(int argc, char** argv) {

    std::set<std::string> allowed_value_options;
    std::set<std::string> allowed_literal_options;

    allowed_value_options.insert("-o");
    allowed_value_options.insert("-d");
    allowed_value_options.insert("-h");
    allowed_value_options.insert("-filename");

    allowed_literal_options.insert("--f64");

    CommandLineArguments parsed_args = parse_args(argc, argv, allowed_value_options, allowed_literal_options, 1);

    if (!parsed_args.success) {
        help();
        return -1;
    }

    std::string s;

    {
        std::ifstream ifs(parsed_args.last_parameter.at(0));
        s = std::string(std::istreambuf_iterator<char>(ifs), {});
    }

    int64_t n = s.length();

    std::cout << "File " << parsed_args.last_parameter.at(0) << " successfully loaded (n=" << n << ")" << std::endl << std::flush;

    bool use64 = false;
    if (parsed_args.literal_options.contains("--f64")) {
        use64 = true;
    }

    std::string o = parsed_args.last_parameter.at(0).append(".lzendsa");
    int32_t delta = -1;
    int32_t h = -1;
    std::string filename = parsed_args.last_parameter.at(0);
    for (Option value_option : parsed_args.value_options) {
        if (value_option.name == "-o") {
            o = value_option.value.append(".lzendsa");
        }
        if (value_option.name == "-d") {
            delta = std::stoi(value_option.value);
        }

        if (value_option.name == "-h") {
            h = std::stoi(value_option.value);
        }
        if (value_option.name == "-filename") {
            filename = value_option.value;
        }
    }

    auto timer_complete_construction = timestamp();

    Lzendsa<int32_t, lzend::WordPackingEncoding<int32_t>> cdsa32;
    Lzendsa<int64_t, lzend::WordPackingEncoding<int64_t>> cdsa64;
    if (n <= INT32_MAX && !use64) {
        std::cout << "Constructing using 32-bit integers..." << std::endl;
        cdsa32.load(s, delta, h, parsed_args.last_parameter.at(0));
    } else {
        std::cout << "Constructing using 64-bit integers..." << std::endl;
        cdsa64.load(s, delta, h, parsed_args.last_parameter.at(0));
    }

    auto td_complete_construction = timestamp() - timer_complete_construction;

    int64_t z, memory_usage;

    if (n <= INT32_MAX && !use64) {
        z = cdsa32.z();
        memory_usage = cdsa32.memory_usage();
    } else {
        z = cdsa64.z();
        memory_usage = cdsa64.memory_usage();
    }

    std::cout << "lzendsa index successfully constructed (z=" << z << ")" << std::endl;

    uint64_t out_bytes = sizeof(uint8_t);

    std::ofstream out(o);
    if (n <= INT32_MAX && !use64) {
        uint8_t long_integer_flag = 0;
        out.write((char*) &long_integer_flag, sizeof(uint8_t));
        out_bytes = cdsa32.serialize(out);
    } else {
        uint8_t long_integer_flag = 1;
        out.write((char*) &long_integer_flag, sizeof(uint8_t));
        out_bytes = cdsa64.serialize(out);
    }
    out.close();

    std::cout << "Wrote " << out_bytes << " bytes to disk." << std::endl;

    uint64_t malloc_space_peak = malloc_count_peak();
    std::cout << "RESULT"
        << " algo=lzendsa_build"
        << " time_ms=" << td_complete_construction
        << " time_n_micro_s=" << (double) td_complete_construction * 1000 / ((double) n * 8)
        << " malloc_peak=" << malloc_space_peak
        << " file=" << filename
        << " disk_size_bytes=" << out_bytes
        << " n=" << n
        << " z=" << z
        << " h=" << h
        << std::endl << std::flush;
}