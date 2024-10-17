#include "cache.h"
#include "prefetch.h"

extern IPT_L1 ipt[NUM_CPUS][IPT_NUM];
void CACHE::prefetcher_initialize() {
    std::cout << NAME << " [Stride] prefetcher" << std::endl;

    for(uint32_t i = 0; i < IPT_NUM; i++){
        ipt[cpu][i].conf = 0;
        ipt[cpu][i].rplc_bits = i;
    }
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    /** Stride Prefetcher */
    pair<uint64_t, uint64_t> stride = stride_cache_operate(cpu, addr, ip);
    if(stride.first != 0){
        int stride_succ = prefetch_line(stride.first, true, stride.second);
    }

    return metadata_in;
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {
    return ;
}

void CACHE::prefetcher_final_stats() {
    return ;
}
