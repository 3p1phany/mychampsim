#include <algorithm>
#include <array>
#include <map>

#include "cache.h"
#include "prefetch.h"

#define L2_STRIDE_SHIFT 2

int32_t decode_stride(uint64_t metadata){
    int32_t stride=0;
    if(metadata & 0x80000000)
        stride = -1*(metadata & 0x7fffffff);
    else
        stride = metadata & 0x7fffffff;

    return stride;
}

PREF_TYPE decode_pref_type(uint64_t metadata){
    PREF_TYPE pref_type = PREF_TYPE ((metadata >> 32) & 0x7);
    return pref_type;
}

void CACHE::l2c_prefetcher_initialize() 
{
    std::cout << "L2C [Stride] prefetcher" << std::endl; 
}

void CACHE::prefetcher_cycle_operate()
{
}

uint64_t CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in)
{
    if(type == PREFETCH){
        PREF_TYPE pref_type = decode_pref_type(metadata_in);
        int32_t stride = decode_stride(metadata_in);

        if(pref_type == PREF_STRIDE){
            uint64_t pf_address = addr + (stride << L2_STRIDE_SHIFT);
            prefetch_line(pf_address, true, metadata_in);
        }
    }

    return metadata_in;
}


uint64_t CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
  return metadata_in;
}


void CACHE::l2c_prefetcher_final_stats() {}