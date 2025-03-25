
#include "cache.h"
#include "triage.h"
#include <map>

#define L2C_WAY 8
#define LLC_WAY 16

#define MAX_ALLOWED_DEGREE 64

TriageConfig conf[NUM_CPUS];
extern Triage triage[NUM_CPUS];
uint64_t last_address[NUM_CPUS];

std::set<uint64_t>unique_addr;
std::map<uint64_t, uint64_t> total_usage_count;
std::map<uint64_t, uint64_t> actual_usage_count;

//16K entries = 64KB
void triage_prefetcher_initialize(CACHE *cache) {
    uint32_t cpu = cache->cpu;
    conf[cpu].lookahead = 1;
    conf[cpu].degree = PREFETCH_DISTANCE;
    conf[cpu].training_unit_size = 10000000;
    //conf[cpu].repl = TRIAGE_REPL_LRU;
    conf[cpu].repl = TRIAGE_REPL_HAWKEYE;
    //conf[cpu].repl = TRIAGE_REPL_PERFECT;
    conf[cpu].use_dynamic_assoc = true;
    conf[cpu].on_chip_assoc = 0;
    conf[cpu].on_chip_set = 65536;
    assert( (conf[cpu].on_chip_set & (conf[cpu].on_chip_set - 1)) == 0);
    conf[cpu].metadata_delay = 19;//static_cast<CACHE*>(cache->lower_level)->HIT_LATENCY;
    std::cout << "CPU " << cpu << " assoc: " << conf[cpu].on_chip_assoc << std::endl;
    std::cout << "Prefetch distance: " << conf[cpu].degree << std::endl;

#if TEMPORAL_L1D == true
    CACHE* l2 = static_cast<CACHE*>(cache->lower_level);
    CACHE* l3 = static_cast<CACHE*>(l2->lower_level);
#else
    CACHE* l3 = static_cast<CACHE*>(cache->lower_level);
#endif
    conf[cpu].max_assoc = l3->NUM_WAY/2;

    triage[cpu].set_conf(&conf[cpu]);
    triage[cpu].test();
}

uint64_t triage_prefetcher_operate(uint64_t addr, uint64_t pc, uint8_t cache_hit, uint8_t type, uint64_t metadata_in, CACHE *cache) {
    if (type != LOAD)
        return metadata_in;

    //if (cache_hit)
        //return metadata_in;

    uint32_t cpu = cache->cpu;
#ifdef USE_FULL_ADDR
    //addr <<= LOG2_BLOCK_SIZE;
#else
    addr >>= LOG2_BLOCK_SIZE;
#endif
    if (addr == last_address[cpu])
        return metadata_in;
    last_address[cpu] = addr;
    unique_addr.insert(addr);

    // clear the prefetch list
    uint64_t prefetch_addr_list[MAX_ALLOWED_DEGREE];
    for (int i = 0; i < MAX_ALLOWED_DEGREE; i++)
        prefetch_addr_list[i] = 0;

    // set the prefetch list by operating the prefetcher
    triage[cpu].calculatePrefetch(pc, addr, cache_hit, prefetch_addr_list, MAX_ALLOWED_DEGREE, cpu, cache->current_cycle);

    // // prefetch desired lines
    // int prefetched = 0;
    // for (int i = 0; i < MAX_ALLOWED_DEGREE; i++) {
    // #ifdef USE_FULL_ADDR
    //     uint64_t target = prefetch_addr_list[i] ;//<< LOG2_BLOCK_SIZE;
    // #else
    //     uint64_t target = prefetch_addr_list[i] << LOG2_BLOCK_SIZE;
    // #endif

    //     // check if prefetch requested
    //     if (target == 0)
    //         break;

    //     assert(i < conf[cpu].degree);

    //     // check L2 and LLC for request to keep track of which metadata is used
    //     PACKET test_packet;
    //     test_packet.address = target;

    //     // TODO: Triage
    //     bool llc_hit = static_cast<CACHE*>(cache->lower_level)->check_hit(&test_packet) != -1;
    //     bool l2_hit = cache->check_hit(&test_packet) != -1;
    
    //     uint64_t md_in = addr;
    //     if (llc_hit)
    //         md_in = 0;
    //     total_usage_count[addr]++;
    //     if (!l2_hit && !llc_hit)
    //        actual_usage_count[addr]++; 
            
    //     // check if prefetch actually issued
    //     if (cache->prefetch_line(pc, addr, target, true, md_in)) {
    //         prefetched++;
    //     }
    // }

    // Set cache assoc if dynamic
    uint32_t total_assoc = 0;
    for (uint32_t mycpu = 0; mycpu < NUM_CPUS; mycpu++)
        total_assoc += triage[mycpu].get_target_assoc();
    total_assoc /= NUM_CPUS;

    // set associativity
    assert(total_assoc < LLC_WAY);
    if (conf[cpu].repl != TRIAGE_REPL_PERFECT){
#if TEMPORAL_L1D == true
        CACHE* L2Cache = static_cast<CACHE*>(cache->lower_level);
        static_cast<CACHE*>(L2Cache->lower_level)->current_assoc = LLC_WAY - total_assoc;
#else
        static_cast<CACHE*>(cache->lower_level)->current_assoc = LLC_WAY - total_assoc;
#endif
    }


    return metadata_in;
}

uint64_t triage_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, CACHE *cache) {
    return metadata_in;
}

void triage_prefetcher_cycle_operate(uint32_t cpu, CACHE* cache) {
    /** Send Prefetch Request*/
    // prefetch desired lines
    auto prefetch = triage[cpu].pref();

    #ifdef USE_FULL_ADDR
        uint64_t target = prefetch.addr ;//<< LOG2_BLOCK_SIZE;
    #else
        uint64_t target = prefetch.addr << LOG2_BLOCK_SIZE;
    #endif
    if(target != 0)
        cache->prefetch_line(target, FILL_L2, 0);

    /** Handle Write*/
    TriageOnchip* t = &triage[cpu].on_chip_data;
#if TEMPORAL_L1D == true
    CACHE* l2 = static_cast<CACHE*>(cache->lower_level);
    CACHE* l3 = static_cast<CACHE*>(l2->lower_level);
#else
    CACHE* l3 = static_cast<CACHE*>(cache->lower_level);
#endif
    assert(t->wb_index < l3->NUM_SET);
    while (t->target_assoc > t->assoc){
        uint32_t wb_way = LLC_WAY - t->assoc - 1;

        BLOCK& evict_block = l3->block[t->wb_index * l3->NUM_WAY + wb_way];
        bool evicting_dirty = evict_block.dirty;

        if (evicting_dirty) {
            PACKET writeback_packet;
            writeback_packet.fill_level = l3->lower_level->fill_level;
            writeback_packet.cpu = cpu;
            writeback_packet.address = evict_block.address;
            writeback_packet.data = evict_block.data;
            writeback_packet.instr_id = 0;
            writeback_packet.ip = 0;
            writeback_packet.type = WRITEBACK;
            auto result = l3->lower_level->add_wq(&writeback_packet);
            if (result == 0){
                evict_block.dirty = false;
                t->wb_index++;
                if (t->wb_index == l3->NUM_SET){
                    t->wb_index = 0;
                    t->assoc++;
                }
            }
            break;
        }
        else {
            t->wb_index++;
            if (t->wb_index == l3->NUM_SET){
                t->wb_index = 0;
                t->assoc++;
            }
        }
    }
    if(t->assoc > t->target_assoc){
        t->assoc = t->target_assoc;
    }
    // assert(t->target_assoc == t->assoc);
    assert(t->assoc <= t->max_assoc);
}

void triage_prefetcher_final_stats(CACHE *cache) {
    uint32_t cpu = cache->cpu;
    cout << "CPU " << cpu << " TRIAGE Stats:" << endl;

    triage[cpu].print_stats();

    std::map<uint64_t, uint64_t> total_pref_count;
    std::map<uint64_t, uint64_t> actual_pref_count;
    for (auto it = total_usage_count.begin(); it != total_usage_count.end(); it++)
        total_pref_count[it->second]++;
    for (auto it = actual_usage_count.begin(); it != actual_usage_count.end(); it++)
        actual_pref_count[it->second]++;

    cout << "Unique Addr Size: " << unique_addr.size() << endl;
}

