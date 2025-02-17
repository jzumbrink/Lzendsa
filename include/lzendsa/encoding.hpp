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

# ifndef _ENCODING_HPP
# define _ENCODING_HPP

#include "integer_lzend.hpp"

#include <vector>
#include <cstdint>

namespace lzend {

/*
Interface for every implemented encoding scheme.
Defines all relevant member functions needed for extracting values of the compressed data.
The <Index> is the integer type occuring in  the differential suffix array.
*/
template<typename Index>
class Encoding {
public:
    virtual void build(std::vector<lzend::IntPhrase<Index>> &phrases) = 0;
    virtual std::vector<Index> extract(Index index, Index len, uint16_t NUM_THREADS = 1) const = 0;
    virtual Index get_z() const = 0;
    virtual size_t memory_usage() const = 0;
    virtual uint64_t serialize(std::ostream& out) = 0;
    virtual void load(std::istream& in) = 0;

    virtual ~Encoding() = default;

    Encoding() = default;
};

}

# endif