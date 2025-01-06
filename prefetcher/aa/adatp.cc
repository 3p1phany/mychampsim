#include "ooo_cpu.h"
#include "cache.h"
#include "prefetch.h"
#include "memory_data.h"
#include "adatp.h"

#include <algorithm>

extern AdaTP adatp[NUM_CPUS];
extern AdaTP_MetaData_OnChip adatp_metadata_onchip[NUM_CPUS];
extern AdaTPConfig adatp_config[NUM_CPUS];

extern MEMORY_DATA mem_data[NUM_CPUS];

void adatp_prefetcher_initialize(CACHE* l2) {
    cout << "L2C [AdaTP] Prefetcher" << endl;

    adatp_config[l2->cpu].cpu = l2->cpu;

    adatp_config[l2->cpu].criticality_mode = 1;
    adatp_config[l2->cpu].pattern_train_enable = true;
    adatp_config[l2->cpu].use_dynamic_threshold = true;
    adatp_config[l2->cpu].use_dynamic_assoc = true;
    adatp_config[l2->cpu].use_dynamic_pattern = true;

    adatp_config[l2->cpu].critical_conf_threshold = 32;
    adatp_config[l2->cpu].critical_conf_max = 63;
    adatp_config[l2->cpu].criticality_threshold = 0;
    adatp_config[l2->cpu].assoc = 4;

    adatp_config[l2->cpu].training_table_size = 4096;
    adatp_config[l2->cpu].missing_status_size = 32;
    adatp_config[l2->cpu].missing_status_ip_size = 32;
    adatp_config[l2->cpu].set_dueller_size = 64;

    CACHE* l3 = static_cast<CACHE*>(l2->lower_level);

    adatp_config[l2->cpu].cache_sets = l3->NUM_SET;
    adatp_config[l2->cpu].cache_ways = l3->NUM_WAY;
    adatp_config[l2->cpu].meta_sub_ways = 16;
    adatp_config[l2->cpu].num_sets = l3->NUM_SET * 16;

    printf("cache_sets: %lu, cache_ways: %lu, meta_sub_ways: %lu, num_sets: %lu\n", adatp_config[l2->cpu].cache_sets, adatp_config[l2->cpu].cache_ways, adatp_config[l2->cpu].meta_sub_ways, adatp_config[l2->cpu].num_sets);

    adatp_config[l2->cpu].lookahead = 8;
    adatp_config[l2->cpu].readq_size = 64;
    adatp_config[l2->cpu].writeq_size = 64;
    adatp_config[l2->cpu].prefq_size = 64;
    adatp_config[l2->cpu].metadata_delay = 19;//static_cast<CACHE*>(lower_level)->HIT_LATENCY;

    adatp[l2->cpu].init(&adatp_config[l2->cpu]);
    adatp_metadata_onchip[l2->cpu].init(&adatp_config[l2->cpu]);
}

uint64_t adatp_prefetcher_cache_operate(CACHE* l2, uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    if (type == LOAD) {
        // adjust assoc
        CACHE* l3 = static_cast<CACHE*>(l2->lower_level);
        uint32_t meta_assoc = adatp[l2->cpu].get_target_assoc();
        uint32_t cache_assoc = l3->NUM_WAY - meta_assoc;
        assert(cache_assoc >= 0 && cache_assoc <= l3->NUM_WAY);
        adatp_metadata_onchip[l2->cpu].adjust_assoc(meta_assoc);
        l3->adjust_assoc(cache_assoc);

        if (!cache_hit) {
            adatp[l2->cpu].adatp_cache_miss(ip, addr);
        }
        adatp[l2->cpu].training_table_update_on_access(ip, addr, cache_hit && !hit_pref, l2->current_cycle, ooo_cpu[l2->cpu]->num_retired);
    }
    return metadata_in; 
}

void adatp_prefetcher_cycle_operate(CACHE* l2) {
    adatp[l2->cpu].adatp_cycle_op();

    // Prefetch From Metadata PrefQ
    auto prefetch = adatp_metadata_onchip[l2->cpu].pref();
    if (prefetch.addr != 0 && !adatp[l2->cpu].recent_prefetch(prefetch.addr)) {
        l2->prefetch_line(prefetch.addr << 6, true, 0);
    }

    // Metadate WriteQ
    CACHE* l3 = static_cast<CACHE*>(l2->lower_level);
    if (adatp_metadata_onchip[l2->cpu].writeQ.size() != 0) {
        adatp_write_entry* write = &adatp_metadata_onchip[l2->cpu].writeQ.front();
        if(write->first_write){
            BLOCK& evict_block = l3->block[write->index * static_cast<CACHE*>(l2->lower_level)->NUM_WAY + static_cast<CACHE*>(l2->lower_level)->current_assoc + write->way];
            bool evicting_dirty = (l3->lower_level) != NULL && evict_block.dirty;

            if (evicting_dirty) {
                PACKET writeback_packet;
                writeback_packet.fill_level = static_cast<CACHE*>(l2->lower_level)->lower_level->fill_level;
                writeback_packet.cpu = l2->cpu;
                writeback_packet.address = evict_block.address;
                writeback_packet.data = evict_block.data;
                writeback_packet.instr_id = 0;
                writeback_packet.ip = 0;
                writeback_packet.type = WRITEBACK;
                auto result = static_cast<CACHE*>(l2->lower_level)->lower_level->add_wq(&writeback_packet);
                if (result == 0){
                    evict_block.dirty = false;
                    write->first_write = false;
                }
            } 
            else{
                write->first_write = false;
            }
        }
    }
}

uint64_t adatp_prefetcher_cache_fill(CACHE* l2, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    adatp[l2->cpu].adatp_cache_fill(addr, l2->current_cycle, prefetch);
    return metadata_in;
}


void adatp_prefetcher_final_stats(CACHE* l2) {
    adatp[l2->cpu].print_stats();
    adatp_metadata_onchip[l2->cpu].print_stats();
}
