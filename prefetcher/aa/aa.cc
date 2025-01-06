#include "cache.h"
#include "prefetch.h"
#include "aidop.h"
#include "adatp.h"

void CACHE::prefetcher_initialize() {
    cout << "L2C [AidOP+AdaTP] Prefetcher" << endl;
    #ifdef ENABLE_AidOP
    aidop_prefetcher_initialize(this);
    #endif
    #ifdef ENABLE_AdaTP
    adatp_prefetcher_initialize(this);
    #endif
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    #ifdef ENABLE_AidOP
    aidop_prefetcher_cache_operate(this, addr, ip, cache_hit, hit_pref, type, metadata_in);
    #endif
    #ifdef ENABLE_AdaTP
    adatp_prefetcher_cache_operate(this, addr, ip, cache_hit, hit_pref, type, metadata_in);
    #endif
    return metadata_in; 
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    #ifdef ENABLE_AidOP
    aidop_prefetcher_cache_fill(this, addr, set, way, prefetch, evicted_addr, metadata_in, ret_val);
    #endif
    #ifdef ENABLE_AdaTP
    adatp_prefetcher_cache_fill(this, addr, set, way, prefetch, evicted_addr, metadata_in, ret_val);
    #endif
    return metadata_in;
}

void CACHE::prefetcher_cycle_operate() 
{
    #ifdef ENABLE_AidOP
    aidop_prefetcher_cycle_operate(this);
    #endif
    #ifdef ENABLE_AdaTP
    adatp_prefetcher_cycle_operate(this);
    #endif
}

void CACHE::prefetcher_final_stats()
{
    #ifdef ENABLE_AidOP
    aidop_prefetcher_final_stats(this);
    #endif
    #ifdef ENABLE_AdaTP
    adatp_prefetcher_final_stats(this);
    #endif
}