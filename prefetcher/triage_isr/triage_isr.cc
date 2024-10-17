#include "cache.h"
#include "prefetch.h"
#include "triage_wrapper.h"

bool triage_stride_enable = true;
extern IPT_L1 ipt[NUM_CPUS][IPT_NUM];
void CACHE::prefetcher_initialize() {
    std::cout << NAME << " [Triage] prefetcher, ";
#if TEMPORAL_L1D == false
    triage_stride_enable = false;
    cout << "Stride Prefetcher Off" << endl;
#else
    cout << "Stride Prefetcher On" << endl;
#endif

    triage_prefetcher_initialize(this);
    if(triage_stride_enable){
        for(uint32_t i = 0; i < IPT_NUM; i++){
            ipt[cpu][i].conf = 0;
            ipt[cpu][i].rplc_bits = i;
        }
    }
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    /** Stride Prefetcher */
    if(triage_stride_enable){
        pair<uint64_t, uint64_t> stride = stride_cache_operate(cpu, addr, ip);
        if(stride.first != 0){
            int stride_succ = prefetch_line(stride.first, true, stride.second);
        }
    }

    // #ifdef COLLECT_METADATA_CONFLICT
        // bool original_hit = cache_hit && !hit_pref;
    // #else
        bool original_hit = cache_hit;
    // #endif
    return triage_prefetcher_operate(addr, ip, original_hit, type, metadata_in, this);
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
