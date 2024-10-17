#ifndef __CDP_H__
#define __CDP_H__

#include <iostream>
#include <map>
#include <vector>

#include "champsim.h"

struct CDPConfig {
    uint8_t depth;
    uint8_t cmp_bits;
    uint8_t filter_bits;
    uint8_t align_bits;
    uint64_t cmp_mask;
    uint64_t filter_mask;
};

extern void cdp_handle_prefetch(uint32_t cpu, CACHE* cache, uint64_t ip, uint64_t addr, uint64_t md);

#endif // __CDP_H__