#include "cache.h"
#include "prefetch.h"
#include "triage_wrapper.h"

void CACHE::prefetcher_initialize() {
    std::cout << NAME << " [Triage] prefetcher, ";
    triage_prefetcher_initialize(this);
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    return triage_prefetcher_operate(addr, ip, cache_hit, type, metadata_in, this);
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    return triage_prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr, metadata_in, this);
}

void CACHE::prefetcher_cycle_operate() {
    triage_prefetcher_cycle_operate(cpu, this);
}

void CACHE::prefetcher_final_stats() {
    return triage_prefetcher_final_stats(this);
}
