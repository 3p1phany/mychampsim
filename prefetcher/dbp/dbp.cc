#include "ooo_cpu.h"
#include "cache.h"
#include "prefetch.h"
#include "dbp.h"
#include "memory_data.h"

#include <algorithm>

#ifdef ENABLE_DBP
extern DBPConfig dbp_config[NUM_CPUS];
extern DBPLoadRet dbp_load_ret[NUM_CPUS];
extern DBPLoadIdentity dbp_load_identity[NUM_CPUS];
extern DBPPrefQ dbp_prefq[NUM_CPUS];
#endif

bool stride_enable = true;
extern IPT_L1 ipt[NUM_CPUS][IPT_NUM];

void CACHE::prefetcher_initialize() {
    std::cout << NAME << " DBP prefetcher" << "cpu: " << cpu 
              << std::endl;
    dbp_config[cpu].cpu = cpu;

#ifndef ENABLE_DBP
    cout << "DBP is not enabled" << endl;
    exit(1);
#endif

    // LoadRet
    dbp_config[cpu].dbp_load_ret_size = 32;

    // LoadIdentity
    dbp_config[cpu].dbp_load_identity_size = 64;

    dbp_config[cpu].dbp_prefq_size = 16;

    dbp_load_ret[cpu].init(&dbp_config[cpu]);
    dbp_load_identity[cpu].init(&dbp_config[cpu]);
    dbp_prefq[cpu].init(&dbp_config[cpu]);

    if(stride_enable){
        for(uint32_t i = 0; i < IPT_NUM; i++){
            ipt[cpu][i].conf = 0;
            ipt[cpu][i].rplc_bits = i;
        }
    }
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    assert(type == LOAD);

    /** Stride Prefetcher */
    if(stride_enable){
        pair<uint64_t, uint64_t> stride = stride_cache_operate(cpu, addr, ip);
        if(stride.first != 0){
            int stride_succ = prefetch_line(stride.first, true, stride.second);
        }
    }

    return metadata_in; 
}

void CACHE::prefetcher_cycle_operate() {
    uint64_t pref_addr = dbp_prefq[cpu].pref();
    if (pref_addr) {
        prefetch_line(pref_addr, true, 0xdb);
    }
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    return metadata_in;
}


void CACHE::prefetcher_final_stats() {
    dbp_load_identity[cpu].print();
}
