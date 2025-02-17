#include <iostream>
#include <fstream>
#include <set>
#include <cstdint>
#include <string>
#include <random>


#include "utils/cli.hpp"

void help() {
    std::cout << "create-patterns: creates a pattern file for a given text." << std::endl << std::endl;
    
    std::cout << "Usage: create-patterns [options] <text file>" << std::endl;
    std::cout << "\t<text file>     path to text file (should contain text)" << std::endl;
    std::cout << "\t-m              length of the patterns (standard is 8)" << std::endl;
    std::cout << "\t-n              count of patterns (standard is 1000)" << std::endl;
    std::cout << "\t-o              output file" << std::endl;
}

int main(int argc, char** argv) {
    std::set<std::string> allowed_value_options;
    std::set<std::string> allowed_literal_options;

    allowed_value_options.insert("-m");
    allowed_value_options.insert("-n");
    allowed_value_options.insert("-o");

    CommandLineArguments parsed_args = parse_args(argc, argv, allowed_value_options, allowed_literal_options, 1);

    if (!parsed_args.success) {
        help();
        return -1;
    }

    std::string text;

    {
        std::ifstream ifs(parsed_args.last_parameter.at(0));
        text = std::string(std::istreambuf_iterator<char>(ifs), {});
    }

    int64_t n = text.length();

    std::cout << "File " << parsed_args.last_parameter.at(0) << " successfully loaded (n=" << n << ")" << std::endl;

    std::string o = parsed_args.last_parameter.at(0).append(".patt");
    int64_t pattern_length = 8;
    int64_t pattern_count = 1000;
    for (Option value_option : parsed_args.value_options) {
        if (value_option.name == "-m") {
            pattern_length = std::stol(value_option.value);
        }
        if (value_option.name == "-n") {
            pattern_count = std::stol(value_option.value);
        }
        if (value_option.name == "-o") {
            o = value_option.value.append(".patt");
        }
    }

    // open target file
    std::ofstream out(o);
    out << "# number=" << pattern_count << " length=" << pattern_length << " file=" << parsed_args.last_parameter.at(0) << std::endl;

    // take patterns from text
    std::random_device rd; 
    std::mt19937 gen(rd());

    if (n - pattern_length < 0) {
        std::cout << "Invalid pattern_length (parameter -m)" << std::endl;
        return -1;
    }
    std::uniform_int_distribution<int64_t> dist(0, n - pattern_length);

    for (int64_t i = 0; i < pattern_count; ++i) {
        int64_t index = dist(gen);
        out << text.substr(index, pattern_length);
    }

    out.close();

    std::cout << "Successfully wrote " << pattern_count << " pattern of the length " << pattern_length << " to the file " << o << std::endl;
}