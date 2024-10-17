#include "ooo_cpu.h"
#include "cache.h"
#include "prefetch.h"
#include "memory_data.h"
#include "catp.h"

#include <algorithm>

extern CATP catp[NUM_CPUS];
extern CATP_MetaData_OnChip catp_metadata_onchip[NUM_CPUS];
extern CATPConfig catp_config[NUM_CPUS];

bool stride_enable = true;
extern IPT_L1 ipt[NUM_CPUS][IPT_NUM];

extern MEMORY_DATA mem_data[NUM_CPUS];

void CACHE::prefetcher_initialize() {
    std::cout << NAME << " Critical Aware Temporal Prefetcher";
    #if TEMPORAL_L1D == false
        stride_enable = false;
        cout << "Stride Prefetcher Off" << endl;
    #else
        cout << "Stride Prefetcher On" << endl;
    #endif

    catp_config[cpu].cpu = cpu;

    catp_config[cpu].criticality_mode = 1;
    catp_config[cpu].pattern_train_enable = true;
    catp_config[cpu].use_dynamic_threshold = true;
    catp_config[cpu].use_dynamic_assoc = true;
    catp_config[cpu].use_dynamic_pattern = true;

    catp_config[cpu].critical_conf_threshold = 32;
    catp_config[cpu].critical_conf_max = 63;
    catp_config[cpu].criticality_threshold = 0;
    catp_config[cpu].assoc = 4;

    catp_config[cpu].training_table_size = 4096;
    catp_config[cpu].missing_status_size = 32;
    catp_config[cpu].missing_status_ip_size = 32;
    catp_config[cpu].set_dueller_size = 64;

    #if TEMPORAL_L1D == true
        CACHE* l2 = static_cast<CACHE*>(lower_level);
        CACHE* l3 = static_cast<CACHE*>(l2->lower_level);
    #else
        CACHE* l3 = static_cast<CACHE*>(lower_level);
    #endif

    catp_config[cpu].cache_sets = l3->NUM_SET;
    catp_config[cpu].cache_ways = l3->NUM_WAY;
    catp_config[cpu].meta_sub_ways = 16;
    catp_config[cpu].num_sets = l3->NUM_SET * 16;

    printf("cache_sets: %lu, cache_ways: %lu, meta_sub_ways: %lu, num_sets: %lu\n", catp_config[cpu].cache_sets, catp_config[cpu].cache_ways, catp_config[cpu].meta_sub_ways, catp_config[cpu].num_sets);

    catp_config[cpu].lookahead = 8;
    catp_config[cpu].readq_size = 64;
    catp_config[cpu].writeq_size = 64;
    catp_config[cpu].prefq_size = 64;
    catp_config[cpu].metadata_delay = 19;//static_cast<CACHE*>(lower_level)->HIT_LATENCY;

    catp[cpu].init(&catp_config[cpu]);
    catp_metadata_onchip[cpu].init(&catp_config[cpu]);

    if(stride_enable){
        for(uint32_t i = 0; i < IPT_NUM; i++){
            ipt[cpu][i].conf = 0;
            ipt[cpu][i].rplc_bits = i;
        }
    }
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    if (type == LOAD) {
        /** Stride Prefetcher */
        if(stride_enable){
            pair<uint64_t, uint64_t> stride = stride_cache_operate(cpu, addr, ip);
            if(stride.first != 0){
                int stride_succ = prefetch_line(stride.first, true, stride.second);
            }
        }

        // adjust assoc
        #if TEMPORAL_L1D == true
            CACHE* l2 = static_cast<CACHE*>(lower_level);
            CACHE* l3 = static_cast<CACHE*>(l2->lower_level);
        #else
            CACHE* l3 = static_cast<CACHE*>(lower_level);
        #endif
        uint32_t meta_assoc = catp[cpu].get_target_assoc();
        uint32_t cache_assoc = l3->NUM_WAY - meta_assoc;
        assert(cache_assoc >= 0 && cache_assoc <= l3->NUM_WAY);
        catp_metadata_onchip[cpu].adjust_assoc(meta_assoc);
        l3->adjust_assoc(cache_assoc);

        if (!cache_hit) {
            catp[cpu].catp_cache_miss(ip, addr);
        }
        catp[cpu].training_table_update_on_access(ip, addr, cache_hit && !hit_pref, current_cycle, ooo_cpu[cpu]->num_retired);
    }
    return metadata_in; 
}

void CACHE::prefetcher_cycle_operate() {
    catp[cpu].catp_cycle_op();

    // Prefetch From Metadata PrefQ
    auto prefetch = catp_metadata_onchip[cpu].pref();
    if (prefetch.addr != 0 && !catp[cpu].recent_prefetch(prefetch.addr)) {
        prefetch_line(prefetch.addr << 6, true, 0);
    }

    // Metadate WriteQ
    #if TEMPORAL_L1D == true
        CACHE* l2 = static_cast<CACHE*>(lower_level);
        CACHE* l3 = static_cast<CACHE*>(l2->lower_level);
    #else
        CACHE* l3 = static_cast<CACHE*>(lower_level);
    #endif
    if (catp_metadata_onchip[cpu].writeQ.size() != 0) {
        catp_write_entry* write = &catp_metadata_onchip[cpu].writeQ.front();
        if(write->first_write){
            BLOCK& evict_block = l3->block[write->index * static_cast<CACHE*>(lower_level)->NUM_WAY + static_cast<CACHE*>(lower_level)->current_assoc + write->way];
            bool evicting_dirty = (l3->lower_level) != NULL && evict_block.dirty;

            if (evicting_dirty) {
                PACKET writeback_packet;
                writeback_packet.fill_level = static_cast<CACHE*>(lower_level)->lower_level->fill_level;
                writeback_packet.cpu = cpu;
                writeback_packet.address = evict_block.address;
                writeback_packet.data = evict_block.data;
                writeback_packet.instr_id = 0;
                writeback_packet.ip = 0;
                writeback_packet.type = WRITEBACK;
                auto result = static_cast<CACHE*>(lower_level)->lower_level->add_wq(&writeback_packet);
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

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    catp[cpu].catp_cache_fill(addr, current_cycle, prefetch);
    return metadata_in;
}


void CACHE::prefetcher_final_stats() {
    catp[cpu].print_stats();
    catp_metadata_onchip[cpu].print_stats();
}
