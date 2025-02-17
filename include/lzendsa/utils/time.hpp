# ifndef _TIME_HPP
# define _TIME_HPP

#include <chrono>

uintmax_t timestamp() {
    return std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
}

# endif