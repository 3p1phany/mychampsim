#include "cache.h"
#include "prefetch.h"
#include "triangel.hh"

Triangel* triangel[NUM_CPUS];

void CACHE::prefetcher_initialize() {
    std::cout << NAME << " [Triangel] prefetcher, ";

    triangel[cpu] = new Triangel(cpu);

    std::cout << "Prefetch distance: " << triangel[cpu]->get_degree()
              << ", cpu: " << triangel[cpu]->get_cpu_id()
              << std::endl;
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    std::vector<AddrPriority> prefetches;
    triangel[cpu]->calculatePrefetch(type==LOAD, ip, addr,prefetches);
    for (auto prefetch : prefetches) {
        debug_printf("Prefetching %lx in %ld cycles\n", prefetch.addr, prefetch.priority);
        if (triangel[cpu]->PrefetchQueue.size() >= triangel[cpu]->pq_size) {
            triangel[cpu]->PrefetchQueue.pop_front();
        }
        triangel[cpu]->PrefetchQueue.push_back(PrefetchQueueEntry(prefetch.addr,prefetch.priority));
    }
    return metadata_in;
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {
#if TEMPORAL_L1D == true
    CACHE* l3 = static_cast<CACHE*>((static_cast<CACHE*>(lower_level))->lower_level);
#else
    CACHE* l3 = static_cast<CACHE*>(lower_level);
#endif
    uint32_t total_assoc = 0;
    for (uint32_t cpuno = 0; cpuno < NUM_CPUS; cpuno++){
        TriangelHashedSetAssociative* thsa = dynamic_cast<TriangelHashedSetAssociative*>(triangel[cpuno]->markovTablePtr->indexingPolicy);
        total_assoc += thsa->ways;
    }
    total_assoc /= NUM_CPUS;
    l3->current_assoc = l3->NUM_WAY - total_assoc;

    assert(l3->NAME=="LLC");
    triangel[cpu]->total_assoc += l3->current_assoc;
    triangel[cpu]->total_cycle ++;
    for (auto &prefetch : triangel[cpu]-> PrefetchQueue) {
        prefetch.delay -= 1;
        if (prefetch.delay == 0) {
            prefetch_line(prefetch.addr, FILL_L2, 0);
        }
    }
}

void CACHE::prefetcher_final_stats() {
    cout << "triangel prefetcher final stats:" << endl;
    cout << "metadata_conf_change: " << triangel[cpu]->metadata_update_diff <<endl;
    cout << "metadata_conf_same: " << triangel[cpu]->metadata_update_same <<endl;

#if TEMPORAL_L1D == true
    CACHE* l3 = static_cast<CACHE*>((static_cast<CACHE*>(lower_level))->lower_level);
#else
    CACHE* l3 = static_cast<CACHE*>(lower_level);
#endif
    assert(l3->NAME=="LLC");
    cout << "avg_assoc: " << (float)l3->NUM_WAY - (float)triangel[cpu]->total_assoc / triangel[cpu]->total_cycle <<endl;

    cout << "[Collect Entry Number]: " << endl;

    cout << "metadata_read_num: " << triangel[cpu]->metadata_rq_issue << endl;
    cout << "metadata_write_num: " << triangel[cpu]->metadata_wq_issue << endl;

    cout << "recursive entry: " << triangel[cpu]->trigger_addr_recursive.size() << endl;
    cout << "data entry: " << triangel[cpu]->trigger_addr_data.size() << endl;
    cout << "other entry: " << triangel[cpu]->trigger_addr_other.size() << endl;

    cout << "trigger addr: " << triangel[cpu]->trigger_addr.size() << endl;

    cout << "metadata_recursive_conf: " << triangel[cpu]->conf_dec_recursive << endl;
    cout << "metadata_data_conf: " << triangel[cpu]->conf_dec_data << endl;
    cout << "metadata_other_conf: " << triangel[cpu]->conf_dec_other << endl;
}
