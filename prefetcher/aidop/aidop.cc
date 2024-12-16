#include "cache.h"
#include "prefetch.h"

#define DELTA_TABLE_ENTRY_NUM (1+48+8)
#define DELTA_TABLE_OFFSET_NUM 16
#define DELTA_TABLE_DELTA_NUM 4

class delta_table_entry {
    public:
    uint8_t page_tag;
    uint8_t idx;
    uint16_t cycle[DELTA_TABLE_OFFSET_NUM];
    uint8_t offset[DELTA_TABLE_OFFSET_NUM];
    int16_t delta[DELTA_TABLE_DELTA_NUM];
    uint8_t conf[DELTA_TABLE_DELTA_NUM];
    int16_t best_delta;

    delta_table_entry() {
        page_tag = 0;
        idx = 0;
        for (int i = 0; i < DELTA_TABLE_OFFSET_NUM; i++) {
            offset[i] = 0;
            cycle[i] = 0;
        }
        for (int i = 0; i < DELTA_TABLE_DELTA_NUM; i++) {
            delta[i] = 0;
            conf[i] = 0;
        }
        best_delta = 0;
    }
};

delta_table_entry delta_table[NUM_CPUS][DELTA_TABLE_ENTRY_NUM];
uint32_t avg_delay = 500;

void CACHE::prefetcher_initialize() {
    cout << "L2C [AidOP] Prefetcher" << endl;
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    assert(metadata_in < DELTA_TABLE_ENTRY_NUM);
    delta_table_entry *entry = &delta_table[cpu][metadata_in];

    uint8_t page_tag = (addr >> LOG2_PAGE_SIZE) & 0xFF;
    uint8_t offset = (addr >> LOG2_BLOCK_SIZE) & 0xFF;
    uint32_t current_cycle = current_core_cycle[cpu] & 0xFFFF;
    if (page_tag == entry->page_tag) {
        if (offset != entry->offset[entry->idx]) {
            for (uint8_t i = entry->idx > 0 ? entry->idx - 1 : DELTA_TABLE_OFFSET_NUM - 1; i != entry->idx; i = i > 0 ? i-1 : DELTA_TABLE_OFFSET_NUM - 1) {
                if (entry->cycle[i] != 0 && current_cycle > entry->cycle[i] && 
                    current_cycle - entry->cycle[i] > avg_delay) {
                    int16_t delta = offset > entry->offset[i] ? offset - entry->offset[i] : -1*(entry->offset[i] - offset);
                    bool exist = false;
                    for (uint8_t j = 0; j < DELTA_TABLE_DELTA_NUM; j++) {
                        if (delta == entry->delta[j]) {
                            exist = true;
                            entry->conf[j] = entry->conf[j] < 7 ? entry->conf[j] + 1 : 7;
                            break;
                        }
                    }
                    bool replace = false;
                    if (!exist) {
                        for (uint8_t j = 0; j < DELTA_TABLE_DELTA_NUM; j++) {
                            if (entry->conf[j] == 0) {
                                entry->delta[j] = delta;
                                entry->conf[j] = 1;
                                replace = true;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
            entry->offset[entry->idx] = offset;
            entry->cycle[entry->idx] = current_cycle;
            entry->idx = entry->idx < DELTA_TABLE_OFFSET_NUM - 1 ? entry->idx + 1 : 0;
        }
    }
    else {
        uint8_t max_conf = 0;
        uint8_t max_idx = 0;
        for (uint8_t i = 0; i < DELTA_TABLE_DELTA_NUM; i++) {
            if (entry->conf[i] > max_conf) {
                max_conf = entry->conf[i];
                max_idx = i;
            }
        }
        if (max_conf > 4) {
            entry->best_delta = entry->delta[max_idx];
        }
        for (uint8_t i = 0; i < DELTA_TABLE_OFFSET_NUM; i++) {
            entry->offset[i] = 0;
            entry->cycle[i] = 0;
        }
        for (uint8_t i = 0; i < DELTA_TABLE_DELTA_NUM; i++) {
            entry->delta[i] = 0;
            entry->conf[i] = 0;
        }
        entry->idx = 0;
        entry->page_tag = page_tag;
        entry->offset[entry->idx] = offset;
        entry->cycle[entry->idx] = current_cycle;
        entry->idx = entry->idx < DELTA_TABLE_OFFSET_NUM - 1 ? entry->idx + 1 : 0;
    }

    if (entry->best_delta != 0) {
        uint64_t pf_addr = addr + ((entry->best_delta) << LOG2_BLOCK_SIZE);
        prefetch_line(pf_addr, FILL_L2, 0);
    }
    return metadata_in; 
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}
