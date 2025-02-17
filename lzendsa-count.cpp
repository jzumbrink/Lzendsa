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
#include <vector>
#include <sstream>
#include <string>

#include "lzendsa.hpp"
#include "word_packing_encoding.hpp"
#include "utils/cli.hpp"
#include "utils.hpp"
#include "utils/definitions.hpp"
#include "utils/time.hpp"

void help() {
    std::cout << "lzendsa-count: count all occurences of the input patterns." << std::endl << std::endl;
    
    std::cout << "Usage: lzendsa-count [options] <lzendsa file> <text file> <pattern file>" << std::endl;
    std::cout << "\t<lzendsa file>     path to lzendsa file (should the binary representation of the lzendsa construction)" << std::endl;
    std::cout << "\t<text file>        path to text file (should contain text)" << std::endl;
    std::cout << "\t<pattern file>     path to file containing the pattern in pizza&chili format." << std::endl;
    std::cout << "\t-o                 file where the intervals will be written to (if no value is provided, then the results are not written onto the disk)." << std::endl;
    std::cout << "\t(-filename         sets the filename only for the RESULT line)" << std::endl;
    std::cout << "\t(-h                sets h only for the RESULT line)" << std::endl;
}

template<typename IntWord, typename Enc>
void count(std::string &s, std::ifstream& lzendsa_in, std::ifstream& pattern_in, std::string output_filename, std::string result_filename, int32_t h) {
    uint8_t const* text = (uint8_t const*) s.data();

    std::cout << "Loading lzendsa..." << std::flush;
    Lzendsa<IntWord, Enc> lzendsa;
    lzendsa.load(lzendsa_in);
    std::cout << " done." << std::endl << std::flush;

    std::cout << "Loading patterns..." << std::flush;

    std::string pattern_header;
    std::getline(pattern_in, pattern_header);
    
    uint64_t pattern_length  = get_pattern_length(pattern_header);
    uint64_t pattern_count = get_pattern_count(pattern_header);

    std::vector<std::string> patterns_str = load_patterns(pattern_in, pattern_length, pattern_count);
    std::vector<std::vector<uint8_t>> patterns = strToUint8Vec(patterns_str);

    std::cout << " found " << pattern_count << " patterns of length " << pattern_length << "." << std::endl << std::flush;

    std::cout << "Count..." << std::flush;

    auto timer_count = timestamp();

    std::vector<Interval> intervals;
    intervals.reserve(pattern_count);
    for (std::vector<uint8_t> pattern: patterns) {
        Interval interval = lzendsa.count(text, pattern);
        intervals.push_back(interval);        
    }

    auto td_count = timestamp() - timer_count;

    int64_t occ_count = 0;
    for (Interval interval : intervals) {
        occ_count += interval.ep - interval.sp + 1;
    }

    std::cout << std::endl << "Found all patterns in " << td_count << " ms (occ=" << occ_count << ")." << std::endl;

    if (output_filename.length() > 0) {
        std::ofstream out(output_filename);
        // save on disk
        for (Interval interval : intervals) {
            out << interval.sp << " " << interval.ep << std::endl;
        }
        out.close();
    } 

    std::cout << "RESULT"
        << " algo=lzendsa_count"
        << " tp_us=" << (double) pattern_count / ((double) td_count * (double) 1000)
        << " tp_ms=" << (double) pattern_count / ((double) td_count)
        << " idx_size_bn=" << (double) lzendsa.memory_usage() / (double) lzendsa.get_n()
        << " time_ms=" << td_count
        << " occ=" << occ_count
        << " time_per_occ_ns=" << ((double) td_count * 1000000) / (double) occ_count
        << " query_time_us=" << ((double) td_count * (double) 1000) / (double) pattern_count
        << " file=" << result_filename
        << " m=" << pattern_length
        << " n=" << s.length()
        << " d=" << lzendsa.d()
        << " h=" << h
        << " index_size=" << lzendsa.memory_usage()
        << " bits_per_symbol=" << (double) lzendsa.memory_usage() * 8 / (double) s.length()
        << std::endl << std::flush;
}

int main(int argc, char** argv) {
    std::set<std::string> allowed_value_options;
    std::set<std::string> allowed_literal_options;

    allowed_value_options.insert("-o");
    allowed_value_options.insert("-h");
    allowed_value_options.insert("-filename");

    CommandLineArguments a = parse_args(argc, argv, allowed_value_options, allowed_literal_options, 3);

    if (!a.success) {
        help();
        return -1;
    }

    std::string pattern_out_file = "";
    std::string filename = a.last_parameter.at(0);
    int32_t h = -1;
    for (Option value_option : a.value_options) {
        if (value_option.name == "-o") {
            pattern_out_file = value_option.value.append(".intervals");
        }
        if (value_option.name == "-filename") {
            filename = value_option.value;
        }
        if (value_option.name == "-h") {
            h = std::stoi(value_option.value);
        }
    }

    std::string lzendsa_file = a.last_parameter.at(0);
    std::string text_file = a.last_parameter.at(1);
    std::string pattern_file = a.last_parameter.at(2);

    std::ifstream in(lzendsa_file);
    std::ifstream text_in(text_file);
    std::ifstream pattern_in(pattern_file);

    std::cout << "Loading text file..." << std::flush;
    std::string s = std::string(std::istreambuf_iterator<char>(text_in), {});
    std::cout << " done (n = " << s.length() << ")."<< std::endl << std::flush;

    uint8_t long_integer_flag;
    in.read((char*) &long_integer_flag, sizeof(long_integer_flag));
    
    if (long_integer_flag == 0) {
        count<int32_t, lzend::WordPackingEncoding<int32_t>>(s, in, pattern_in, pattern_out_file, filename, h);
    } else {
        count<int64_t, lzend::WordPackingEncoding<int64_t>>(s, in, pattern_in, pattern_out_file, filename, h);
    }
}