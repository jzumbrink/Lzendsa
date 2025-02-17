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

#include "lzendri.hpp"
#include "word_packing_encoding.hpp"
#include "utils/cli.hpp"
#include "utils/time.hpp"
#include "utils/definitions.hpp"
#include "utils.hpp"

void help() {
    std::cout << "lzendri-build: builds the Lzendri-Index from the input file." << std::endl << std::endl;
    
    std::cout << "Usage: lzendri-build [options] <text file>" << std::endl;
    std::cout << "\t<text file>     path to the input file (should contain text)" << std::endl;
    std::cout << "\t-o              path to the desired output file (the extension .lzendri will be added automatically)" << std::endl;
    std::cout << "\t-h              longest phrase length, leave blank or put -1 for unbounded phrase length" << std::endl;
    std::cout << "\t--f64           explicitly use 64-bit-integers regardless of the file size" << std::endl;
    std::cout << "\t(-filename      sets the filename only for the RESULT line)" << std::endl;
}

int main(int argc, char** argv) {
    std::set<std::string> allowed_value_options;
    std::set<std::string> allowed_literal_options;

    allowed_value_options.insert("-o");
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

    std::string o = parsed_args.last_parameter.at(0).append(".lzendri");
    int64_t h = -1;
    std::string filename = parsed_args.last_parameter.at(0);
    for (Option value_option : parsed_args.value_options) {
        if (value_option.name == "-o") {
            o = value_option.value.append(".lzendri");
        }

        if (value_option.name == "-h") {
            h = std::stoi(value_option.value);
        }
        if (value_option.name == "-filename") {
            filename = value_option.value;
        }
    }

    auto timer_complete_construction = timestamp();

    Lzendri<int32_t, lzend::WordPackingEncoding<int32_t>> lzendri32;
    Lzendri<int64_t, lzend::WordPackingEncoding<int64_t>> lzendri64;
    if (n <= INT32_MAX && !use64) {
        std::cout << "Using 32-bit-integers..." << std::endl << std::flush;
        lzendri32.build(s, h);
    } else {
        std::cout << "Using 64-bit-integers..." << std::endl << std::flush;
        lzendri64.build(s, h);
    }

    auto td_complete_construction = timestamp() - timer_complete_construction;

    uint64_t out_bytes = sizeof(uint8_t);

    std::ofstream out(o);
    if (n <= INT32_MAX && !use64) {
        uint8_t long_integer_flag = 0;
        out.write((char*) &long_integer_flag, sizeof(uint8_t));
        out_bytes = lzendri32.serialize(out);
    } else {
        uint8_t long_integer_flag = 1;
        out.write((char*) &long_integer_flag, sizeof(uint8_t));
        out_bytes = lzendri64.serialize(out);
    }
    out.close();

    uint64_t malloc_space_peak = 0;
    std::string algo = "lzendri_build";
    malloc_space_peak = malloc_count_peak();
    std::cout << "RESULT"
        << " algo=" << algo
        << " time_ms=" << td_complete_construction
        << " time_n_micro_s=" << (double) td_complete_construction * 1000 / ((double) n * 8)
        << " malloc_peak=" << malloc_space_peak
        << " file=" << filename
        << " disk_size_bytes=" << out_bytes
        << " n=" << n
        << " h=" << h
        << std::endl << std::endl << std::flush;
}