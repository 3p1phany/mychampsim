#include "cache.h"
#include "prefetch.h"

#define ENABLE_AidOP
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

    void reset_offset() {
        for (int i = 0; i < DELTA_TABLE_OFFSET_NUM; i++) {
            offset[i] = 0;
            cycle[i] = 0;
        }
    }
    void reset_delta() {
        for (int i = 0; i < DELTA_TABLE_DELTA_NUM; i++) {
            delta[i] = 0;
            conf[i] = 0;
        }
    }
};

void aidop_prefetcher_initialize(CACHE* l2); 
uint64_t aidop_prefetcher_cache_operate(CACHE* l2, uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in);
uint64_t aidop_prefetcher_cache_fill(CACHE* l2, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val);
void aidop_prefetcher_cycle_operate(CACHE* l2);
void aidop_prefetcher_final_stats(CACHE* l2);