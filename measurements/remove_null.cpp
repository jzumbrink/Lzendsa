#include <iostream>
#include <fstream>
#include <set>
#include <cstdint>
#include <string>
#include <random>


#include "utils/cli.hpp"

void help() {
    std::cout << "remove-null: removes all 0x0 and 0x1 characters." << std::endl << std::endl;
    
    std::cout << "Usage: remove-null <text file>" << std::endl;
    std::cout << "\t<text file>     path to text file (should contain text)" << std::endl;
}

int main(int argc, char** argv) {
    std::set<std::string> allowed_value_options;
    std::set<std::string> allowed_literal_options;

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

    uint8_t null_char = 0;
    uint64_t changed_chars = 0;

    for (int i = 0; i < n; i++) {
        if (text[i] == null_char) {
            text[i] = 2;
            changed_chars++;
        }
    }

    std::ofstream out(parsed_args.last_parameter.at(0));
    out << text;
    out.close();

    std::cout << "Changed " << changed_chars << " characters to Ox2" << std::endl;
}