/*
 * Copyright (c) 2018 Inria
 * Copyright (c) 2012-2014,2017 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2003-2005,2014 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Definitions of a common framework for indexing policies.
 */

#include "indexing_base.hh"
#include "replaceable_entry.hh"

#include <cstdlib>

template <class T>
static constexpr std::enable_if_t<std::is_integral_v<T>, int>
floorLog2(T x)
{
    assert(x > 0);

    // A guaranteed unsigned version of x.
    uint64_t ux = (typename std::make_unsigned<T>::type)x;

    int y = 0;
    constexpr auto ts = sizeof(T);

    if (ts >= 8 && (ux & 0xffffffff00000000ULL)) { y += 32; ux >>= 32; }
    if (ts >= 4 && (ux & 0x00000000ffff0000ULL)) { y += 16; ux >>= 16; }
    if (ts >= 2 && (ux & 0x000000000000ff00ULL)) { y +=  8; ux >>=  8; }
    if (ux & 0x00000000000000f0ULL) { y +=  4; ux >>=  4; }
    if (ux & 0x000000000000000cULL) { y +=  2; ux >>=  2; }
    if (ux & 0x0000000000000002ULL) { y +=  1; }

    return y;
}

template <class T>
static constexpr bool
isPowerOf2(const T& n)
{
    // If n is non-zero, and subtracting one borrows all the way to the MSB
    // and flips all bits, then this is a power of 2.
    return n && !(n & (n - 1));
}

BaseIndexingPolicy::BaseIndexingPolicy(uint64_t entry_size, uint64_t assoc, uint64_t size)
    : assoc(assoc),
      numSets(size / (entry_size * assoc)),
      setShift(floorLog2(entry_size)), setMask(numSets - 1), sets(numSets),
      tagShift(setShift + floorLog2(numSets))
{
    assert(isPowerOf2(numSets));
    assert(assoc > 0);

    // Make space for the entries
    for (uint32_t i = 0; i < numSets; ++i) {
        sets[i].resize(assoc);
    }
}

ReplaceableEntry*
BaseIndexingPolicy::getEntry(const uint32_t set, const uint32_t way) const
{
    return sets[set][way];
}

void
BaseIndexingPolicy::setEntry(ReplaceableEntry* entry, const uint64_t index)
{
    // Calculate set and way from entry index
    const std::lldiv_t div_result = std::div((long long)index, assoc);
    const uint32_t set = div_result.quot;
    const uint32_t way = div_result.rem;

    // Sanity check
    assert(set < numSets);

    // Assign a free pointer
    sets[set][way] = entry;

    // Inform the entry its position
    entry->setPosition(set, way);
}

uint64_t
BaseIndexingPolicy::extractTag(const uint64_t addr) const
{
    return (addr >> tagShift);
}
