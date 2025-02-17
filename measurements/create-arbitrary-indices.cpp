#include <iostream>
#include <fstream>
#include <set>
#include <cstdint>
#include <string>
#include <random>


#include "utils/cli.hpp"

void help() {
    std::cout << "create-arbitrary-indices: create random indices for measuring random access and interval extraction speed for a given text." << std::endl << std::endl;
    
    std::cout << "Usage: create-arbitrary-indices [options] <text file>" << std::endl;
    std::cout << "\t<text file>       path to text file (should contain text)" << std::endl;
    std::cout << "\t-o                output file" << std::endl;
    std::cout << "\t-l                length of the intervals (standard is 1)" << std::endl;
    std::cout << "\t-n                count of indices (standard is 1000)" << std::endl;
}

int main(int argc, char** argv) {
    std::set<std::string> allowed_value_options;
    std::set<std::string> allowed_literal_options;

    allowed_value_options.insert("-l");
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

    std::cout << "File " << parsed_args.last_parameter.at(0) << " successfully loaded (text_size=" << text.length() << ")" << std::endl;

    int64_t text_size = text.length();
    text = "";

    std::string o = parsed_args.last_parameter.at(0).append(".indices");
    int64_t l = 1;
    int64_t indices_count = 1000;
    for (Option value_option : parsed_args.value_options) {
        if (value_option.name == "-l") {
            l = std::stol(value_option.value);
        }
        if (value_option.name == "-n") {
            indices_count = std::stol(value_option.value);
        }
        if (value_option.name == "-o") {
            o = value_option.value.append(".indices");
        }
    }

    // open target file
    std::ofstream out(o);

    // take patterns from text
    std::random_device rd; 
    std::mt19937 gen(rd());

    if (text_size - l < 0) {
        std::cout << "Invalid length of intervals (parameter -l)" << std::endl;
        return -1;
    }

    std::uniform_int_distribution<int64_t> dist(0, text_size - l);
    for (int64_t i = 0; i < indices_count; ++i) {
        int64_t index = dist(gen);
        out << index << "\n";
    }

    out.close();

    std::cout << "Successfully wrote " << indices_count << " indices (with interval_length=" << l << ") to the file " << o << std::endl;
}