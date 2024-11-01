#include "cache.h"
#include "prefetch.h"
#include "bop.h"

RRTable rr_table;

void CACHE::prefetcher_initialize() {
    std::cout << "L2C [BOP] prefetcher" << std::endl;
    rr_table.init(8, 12, 100);    
    rr_table.offsets = {1,2,3,4,5,6,8,9,10,12,15,16,18,20,24,25,27,30,32,36,40,45,48,50,54,60,64,72,75,80,81,90,96,100,108,120,125,128,135,144,150,160,162,180,192,200,216,225,240,243,250,256};
    for (uint64_t i = 0; i < rr_table.offsets.size(); i++) {
        rr_table.scores.push_back(0);
    }
    rr_table.offset_index = 0;
    rr_table.train_round = 0;
    rr_table.trained_offset = 0;
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    uint64_t test_addr = (addr >> 6) - rr_table.offsets[rr_table.offset_index] ;
    if (rr_table.find(test_addr)) {
        rr_table.scores[rr_table.offset_index]++;
    }
    
    bool training_end = false;
    if (rr_table.scores[rr_table.offset_index] == rr_table.max_score) {
        training_end = true;
    }
    rr_table.offset_index++;
    if (rr_table.offset_index == rr_table.offsets.size()) {
        rr_table.offset_index = 0;
        rr_table.train_round++;
    }
    if (rr_table.train_round == rr_table.max_round) {
        training_end = true;
    }

    if (training_end) {
        uint64_t max_score = 0;
        uint64_t max_index = 0;
        for (uint64_t i = 0; i < rr_table.scores.size(); i++) {
            if (rr_table.scores[i] > max_score) {
                max_score = rr_table.scores[i];
                max_index = i;
            }
        }
        if (max_score > rr_table.bad_score) {
            rr_table.trained_offset = rr_table.offsets[max_index];
        }
        else {
            rr_table.trained_offset = 0;
        }
        for (uint64_t i = 0; i < rr_table.scores.size(); i++) {
            rr_table.scores[i] = 0;
        }
        rr_table.train_round = 0;
    }

    if (rr_table.trained_offset != 0) {
        uint64_t pf_address = ((addr >> LOG2_BLOCK_SIZE) + rr_table.trained_offset) << LOG2_BLOCK_SIZE;
        if ((pf_address >> LOG2_PAGE_SIZE) == (addr >> LOG2_PAGE_SIZE)) {
            prefetch_line(pf_address, true, 0);
        }
    }
    return metadata_in; 
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    if (rr_table.trained_offset == 0 || prefetch) {
        uint64_t base_addr = (addr >> 6) - rr_table.trained_offset;
        rr_table.insert(base_addr);
    }
    return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}
