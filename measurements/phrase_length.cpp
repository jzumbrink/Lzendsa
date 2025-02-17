#include <vector>
#include <memory>
#include <fstream>

#include "libsais_wrapper.hpp"

#include "integer_lzend.hpp"
#include "../test/utils/utils.hpp"

# define INTERVAL_COUNT 20
# define MAX_PHRASE_LENGTH -1

template<typename Index>
void measure_phrase_length(std::string filename, std::string display_filename) {
    std::ifstream ifs(filename);
    std::string s = std::string(std::istreambuf_iterator<char>(ifs), {});

    const Index n = s.length();

    std::cout << "File " << filename << " successfully loaded (n = " << n << ")" << std::endl;

    auto sa = std::make_unique<Index[]>(n);
    lib::sais((uint8_t const*) s.data(), sa.get(), n);

    auto dsa = std::make_unique<Index[]>(n);
    if (n > 0) dsa[0] = sa[0];
    for (Index i = 1; i < n; ++i) {
        dsa[i] = sa[i] - sa[i-1];
    }

    std::vector<lzend::IntPhrase<Index>> phrases = lzend::parse<Index>(dsa.get(), n, true, MAX_PHRASE_LENGTH);
    
    std::cout << "z=" << phrases.size() << std::endl;

    std::vector<double> avg_phrase_length(INTERVAL_COUNT);
    std::vector<int64_t> phrase_length(INTERVAL_COUNT);
    std::vector<int64_t> phrases_per_interval(INTERVAL_COUNT);
    int64_t max_phrase_length = 0;
    int64_t max_phrase_index = 0;
    int64_t current_index = 0;

    for (Index i = 0; i < phrases.size(); i++) {
        Index index = (i * INTERVAL_COUNT) / phrases.size();
        phrase_length[index] += phrases.at(i).len;
        phrases_per_interval[index]++;
        current_index += phrases.at(i).len;
        if (phrases.at(i).len > max_phrase_length) {
            max_phrase_length = phrases.at(i).len;
            max_phrase_index = current_index; 
        }
    }

    for (Index j = 0; j < INTERVAL_COUNT; j++) {
        avg_phrase_length[j] = (double)phrase_length.at(j) / (double)phrases_per_interval.at(j);
    }

    std::cout << "Average phrase length" << std::endl;
    print_vector(avg_phrase_length);
    std::cout << "Cumulated phrase length" << std::endl;
    print_vector(phrase_length);
    std::cout << "Phrases per interval" << std::endl;
    print_vector(phrases_per_interval);
    std::cout << "Longest phrase: " << max_phrase_length << std::endl;

    for (Index i = 0; i < avg_phrase_length.size(); i++) {
        std::cout << "RESULT"
            << " interval=" << i + 1
            << " avg_phrase_length=" << avg_phrase_length.at(i)
            << " what=avg"
            << " h=" << MAX_PHRASE_LENGTH
            << " file=" << display_filename
            << std::endl;
    }
    std::cout << "RESULT"
        << " len=" << max_phrase_length
        << " index=" << max_phrase_index 
        << " rel_index=" << (double) max_phrase_index / (double)n * INTERVAL_COUNT
        << " what=max"
        << " h=" << MAX_PHRASE_LENGTH
        << " file=" << display_filename
        << std::endl;
}

int main() {
    measure_phrase_length<int32_t>("resources/r-index/texts/world_leaders", "world_leaders");
}