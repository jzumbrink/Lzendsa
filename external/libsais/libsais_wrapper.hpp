# ifndef _LIBSAIS_WRAPPER_HPP
# define _LIBSAIS_WRAPPER_HPP

#include <cstdint>

#include "libsais.h"
#include "libsais64.h"

namespace lib {
    // suffix array construction
    int32_t sais(const uint8_t* T, int32_t* SA, int32_t n) {
        return libsais(T, SA, n, 0, nullptr);
    }

    int64_t sais(const uint8_t* T, int64_t* SA, int64_t n) {
        return libsais64(T, SA, n, 0, nullptr);
    }

    // integer suffix array construction
    int32_t sais_int(int32_t* T, int32_t* SA, int32_t n, int32_t k) {
        return libsais_int(T, SA, n, k, 0);
    }

    int64_t sais_int(int64_t* T, int64_t* SA, int64_t n, int64_t k) {
        return libsais64_long(T, SA, n, k, 0);
    }

    // integer plcp array construction
    int32_t sais_plcp_int(const int32_t* T, const int32_t* SA, int32_t* PLCP, int32_t n) {
        return libsais_plcp_int(T, SA, PLCP, n);
    }

    int64_t sais_plcp_int(const int64_t* T, const int64_t* SA, int64_t* PLCP, int64_t n) {
        // todo documentation
        return libsais64_plcp_int(T, SA, PLCP, n);
    }

    // lcp array construction
    int32_t sais_lcp(const int32_t* PLCP, const int32_t* SA, int32_t* LCP, int32_t n) {
        return libsais_lcp(PLCP, SA, LCP, n);
    }

    int64_t sais_lcp(const int64_t* PLCP, const int64_t* SA, int64_t* LCP, int64_t n) {
        return libsais64_lcp(PLCP, SA, LCP, n);
    }
}

# endif