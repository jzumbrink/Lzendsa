# ifndef _PERMUTED_INDICES_CPP
# define _PERMUTED_INDICES_CPP

#include <vector>
#include <numeric>
#include <algorithm>
#include <random>

template<typename T>
std::vector<T> permuted_indices(T const n, int64_t const iterations) {
    std::vector<T> permuted_indices;
    permuted_indices.reserve(iterations);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, n);

    for (int64_t i = 0; i < iterations; ++i) {
        permuted_indices.push_back(dist(gen));
    }

    return permuted_indices;
}

# endif