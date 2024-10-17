#include "ooo_cpu.h"
#include "cache.h"
#include "prefetch.h"
#include "cmc.h"
#include "memory_data.h"

#include <algorithm>

#ifdef ENABLE_CMC
extern CMCConfig cmc_config[NUM_CPUS];
extern LoadRet load_ret[NUM_CPUS];
extern CMC_AGQ cmc_agq[NUM_CPUS];
extern LoadIdentity load_identity[NUM_CPUS];
extern MetaData_OnChip metadata_onchip[NUM_CPUS];
extern LoadCounter_t load_counter[NUM_CPUS];
extern bool cmc_mode[NUM_CPUS];
#endif

bool pf_this_level = true;

bool cmc_stride_enable = true;
extern IPT_L1 ipt[NUM_CPUS][IPT_NUM];

extern MEMORY_DATA mem_data[NUM_CPUS];

extern uint64_t print_ip[14];
extern uint64_t print_rec_ip[4];
extern uint64_t start_print_cycle;

uint64_t lut_access_num, lut_hit_num = 0;

uint64_t rec_pref_cnt = 0, pf_rec_succ_cnt = 0, cache_fill_rec_pref_cnt = 0, 
         dlink_cnt = 0, pf_dlink_succ_cnt = 0 , cache_fill_dlink_cnt = 0,
         dload_cnt = 0, pf_dload_succ_cnt = 0 , cache_fill_dload_cnt = 0;

#define nPRINT_PREFETCH_LINE
#define nPRINT_DEMAND_LOAD
#define PRINT_IP 0xffffffffffffffff
#define COLLECT_LOAD_PC 0xffffffffffffffff

#define PRINT_ADDR_DOWN_RANGE 0x1200e3990
#define PRINT_ADDR_UP_RANGE 0x1200e3990

bool addr_in_range(uint64_t addr) { 
    return (addr >= PRINT_ADDR_DOWN_RANGE && addr <= PRINT_ADDR_UP_RANGE);
}

uint64_t last_load_addr = 0x0;
typedef struct cmc_load_info {
  uint64_t total_count;
  uint64_t next_same_count;
  uint64_t prev_same_count;
  uint64_t hit_count;
  uint64_t prefed_miss_count;
  uint64_t next_addr;
  uint64_t prev_addr;
} cmc_load_info_t;
unordered_map<uint64_t, cmc_load_info_t> load_info;

typedef struct pref_his {
  uint64_t addr;
  uint64_t cycle;
} pref_his_t;

vector <pref_his_t> prefetch_his; // <addr, cycle>

void CACHE::prefetcher_initialize() {
    std::cout << NAME << " [CMC] prefetcher"
              << ", Prefetch distance: " << PREFETCH_DISTANCE;

    if(!cmc_stride_enable){
        std::cout << ", Stride Prefetcher Off" << std::endl;
    } else {
        std::cout << ", Stride Prefetcher On" << std::endl;
    }

    cmc_config[cpu].cpu = cpu;

    // LoadRet
    cmc_config[cpu].load_ret_size = LoadReturn_SIZE;

    // LoadIdentity
    cmc_config[cpu].load_identity_size = LoadIdentity_SIZE;

    // AGQ

    // Metadata
    cmc_config[cpu].use_dynamic_assoc = true;
    cmc_config[cpu].assoc = 0;
    cmc_config[cpu].num_sets = static_cast<CACHE*>(lower_level)->NUM_SET;
    cmc_config[cpu].readq_size = 16;
    cmc_config[cpu].writeq_size = 16;
    cmc_config[cpu].prefq_size = 64;
    cmc_config[cpu].metadata_delay = 9;//static_cast<CACHE*>(lower_level)->HIT_LATENCY;
    //cmc_config[cpu].line_size = CMC_LINE_SIZE;

    cout << "num_sets: " << cmc_config[cpu].num_sets
         << ", load_ret_size: " << cmc_config[cpu].load_ret_size
         << ", load_identity_size: " << cmc_config[cpu].load_identity_size
         << endl;

    load_ret[cpu].init(&cmc_config[cpu]);
    load_identity[cpu].init(&cmc_config[cpu]);
    cmc_agq[cpu].init(&cmc_config[cpu]);
    metadata_onchip[cpu].init(&cmc_config[cpu]);

    // Dynamic CMC 
    load_counter[cpu].total_num = 0;
    load_counter[cpu].rec_num = 0;
#ifdef DYNAMIC_CMC
    cmc_mode[cpu] = false;
#else
    cmc_mode[cpu] = true;
#endif

    if(cmc_stride_enable){
        for(uint32_t i = 0; i < IPT_NUM; i++){
            ipt[cpu][i].conf = 0;
            ipt[cpu][i].rplc_bits = i;
        }
    }
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    assert(type == LOAD);

    /** Print Info */
#ifdef PRINT_DEMAND_LOAD
    if((find(begin(print_rec_ip), end(print_rec_ip), ip) != end(print_rec_ip) || ip == 0x120001658) && current_cycle >= start_print_cycle){
        //uint64_t aligned_addr = addr & ~0x3f;
        //auto pref_his_it = std::find_if(prefetch_his.begin(), prefetch_his.end(), 
        //                                [aligned_addr](const pref_his_t& item) { return item.addr == aligned_addr;});
        //bool pref_his_hit = pref_his_it != prefetch_his.end();

        static uint64_t last_parent = 0;

        uint64_t base_addr = load_identity[cpu].contain(ip)? (addr - load_identity[cpu].get_offset(ip)) : 0;
        cout << "[CMC] cycle: " << current_cycle 
             << ", ip: " << hex << ip << dec 
             << ", hit: " << +cache_hit
             << ", hit_pref: " << +hit_pref
             << ", is_rec: " << +load_identity[cpu].is_recursive(ip)
            //  << ", prefed: " << +pref_his_hit
            //  << ", cycle_dis: " << (pref_his_hit? current_cycle - pref_his_it->cycle : 0)
             << ", addr: " << hex << addr << dec 
             << ", base_addr: " << hex << base_addr << dec
             << ", data: " << hex << mem_data[cpu].read(addr, SIZE_DWORD, true) << dec

            //  << ", last_same: " << +(last_parent == last_load_addr)
            //  << ", last_addr: " << hex << last_load_addr << dec
            //  << ", last_parent: " << hex << last_parent << dec
             << endl;
    
        last_parent = last_load_addr;
    }
#endif

    /** Stride Prefetcher */
    if(cmc_stride_enable){
        pair<uint64_t, uint64_t> stride = stride_cache_operate(cpu, addr, ip);
        if(stride.first != 0){
            int stride_succ = prefetch_line(stride.first, true, stride.second);
        }
    }

    /** CMC Prefetcher */
    /** TODO:
     *  Firstly, There are bug for hit pref! Any type of request can reset .prefetch flag even for prefetch request.
     *  Secondly, demand request can hit/miss alternately.
     *  Now, we just train on the complete address stream. Maybe inte future, after we fix the bug in the first point,
     *  we can train on the miss adddress stream (regard hit in prefetch block as miss).
     */ 
    load_identity[cpu].predict(ip, addr, !cache_hit || (cache_hit && hit_pref), current_cycle);

    assert(NAME.rfind("L1D") != std::string::npos);
    uint32_t total_assoc = 0;
    for (uint32_t mycpu = 0; mycpu < NUM_CPUS; mycpu++)
        total_assoc += metadata_onchip[mycpu].get_assoc();
    total_assoc /= NUM_CPUS;
    uint32_t new_assoc = static_cast<CACHE*>(lower_level)->NUM_WAY - total_assoc;
    assert(new_assoc >= 0 && new_assoc <= static_cast<CACHE*>(lower_level)->NUM_WAY);
    static_cast<CACHE*>(lower_level)->adjust_assoc(new_assoc);

    /** Collect Info */
    if(ip == COLLECT_LOAD_PC && warmup_complete[cpu]){
        assert(addr != 0);

        if(last_load_addr != 0){
            // Set for prev
            auto now_it = load_info.find(addr);
            if(now_it == load_info.end()){
                load_info[addr] = {1, 0, 0, cache_hit, 0, 0, last_load_addr};
            } else {
                now_it->second.total_count++;
                now_it->second.hit_count += cache_hit;
                if(now_it->second.prev_addr == last_load_addr) {
                    now_it->second.prev_same_count++;
                }
                uint64_t aligned_addr = addr & ~0x3f;
                auto pref_his_it = std::find_if(prefetch_his.begin(), prefetch_his.end(), 
                                                [aligned_addr](const pref_his_t& item) { return item.addr == aligned_addr;});
                now_it->second.prefed_miss_count += (cache_hit == 0) && pref_his_it != prefetch_his.end();
                now_it->second.prev_addr = last_load_addr;
            }

            auto prev_it = load_info.find(last_load_addr);
            
            if(prev_it != load_info.end()){
                if(prev_it->second.next_addr == addr){
                    prev_it->second.next_same_count++;
                }
                prev_it->second.next_addr = addr;
            }
        }
        last_load_addr = addr;
    }

    return metadata_in; 
}

void CACHE::prefetcher_cycle_operate() {
    metadata_onchip[cpu].check_state();

    // Prefetch From Metadata ReadQ
    auto prefetch = metadata_onchip[cpu].pref();
    #if COLLECT_LOAD_PC != 0xffffffffffffffff
        if(prefetch_his.size() >= 16){
            prefetch_his.erase(prefetch_his.begin());
        }
        prefetch_his.push_back({prefetch.addr & ~0x3f, current_cycle});
    #endif

    uint64_t pref_ip = prefetch.ip;
    uint64_t pref_base_addr = prefetch.addr;
    uint64_t pref_base_addr_lut_idx = (pref_base_addr >> (11+5)) & ((1 << 11)-1);
    uint64_t pref_base_addr_lut_tag = metadata_onchip[cpu].lut_entries[pref_base_addr_lut_idx].tag;

    pref_base_addr = (pref_base_addr & 0xffff) | (pref_base_addr_lut_tag << (11+5));
    if(pref_ip != 0){
        lut_access_num++;
        lut_hit_num += (pref_base_addr == prefetch.addr);
    }

    if(load_identity[cpu].is_recursive(pref_ip)) {
        uint64_t pref_offset = load_identity[cpu].get_offset(pref_ip);
        uint64_t pref_addr = pref_base_addr + pref_offset;

    #ifdef PRINT_PREFETCH_LINE
        if(find(begin(print_rec_ip), end(print_rec_ip), pref_ip) != end(print_rec_ip) && current_cycle >= start_print_cycle){
            cout << "[Recurs Pref]"
                 << " cycle: " << current_cycle
                 << ", ip: " << hex << pref_ip << dec
                 << ", addr: " << hex << pref_addr << dec
                 << ", base: " << hex << pref_base_addr << dec
                 << ", offset: " << pref_offset
                 << ", dbg_cycle: " << prefetch.cycle
                 << endl;
        }
    #endif

        /** Prefetch Recursive Load*/
        int pf_rec_succ = prefetch_line(pref_addr, pf_this_level, 0xabc);
        rec_pref_cnt++; pf_rec_succ_cnt += pf_rec_succ;

    #ifndef CMC_PREF_ONLY_DIRECT
        /** Prefetch D-Link*/
        uint64_t offsets[LoadIdentity_SIZE] = {0};
        uint64_t dlinks[LoadIdentity_SIZE] = {0};
        uint64_t ips[LoadIdentity_SIZE] = {0};
        auto [offset_num, dlink_num] = load_identity[cpu].get_offset_array(pref_ip, pref_base_addr, pref_addr, offsets, dlinks, ips);

        vector<uint64_t> issued_pf_addr;

        if(pf_rec_succ == 1){
            issued_pf_addr.push_back(pref_addr >> 6);
        }

        for(uint64_t i = 0; i < offset_num; i++){
            uint64_t new_addr = pref_base_addr + offsets[i];
            int pf_dlink_succ = prefetch_line(new_addr, pf_this_level, 0xabcdef);
            dlink_cnt++; pf_dlink_succ_cnt += pf_dlink_succ;
            if(pf_dlink_succ == 1){
                issued_pf_addr.push_back(new_addr >> 6);
            }

        #ifdef PRINT_PREFETCH_LINE
            if(find(begin(print_rec_ip), end(print_rec_ip), pref_ip) != end(print_rec_ip) && current_cycle >= start_print_cycle){
                cout << "[d-link Pref]" 
                     << " cycle: " << current_cycle
                     << ", ip: " << hex << pref_ip << dec
                     << ", addr: " << hex << new_addr << dec
                     << ", base: " << hex  << pref_base_addr << dec
                     << ", offset: " << offsets[i]
                     << ", dbg_cycle: " << prefetch.cycle
                     << endl;
            }
        #endif
        }

        for(uint64_t i = 0; i < dlink_num; i++){
            if(find(begin(issued_pf_addr), end(issued_pf_addr), dlinks[i] >> 6) != end(issued_pf_addr)){
                CMC_AGQ_ITEM item;
                item.ip = ips[i];
                item.ret_value = dlinks[i];
                item.issued = true;
                item.size = SIZE_DWORD;
                item.cycle = current_cycle;
                cmc_agq[cpu].insert(item);
            }
        }
    #endif
    }
#ifdef CMC_RECORD_ALL
    else{
        int pf_rec_succ = prefetch_line(pref_base_addr, pf_this_level, 0xabc);
    }
#endif
    cmc_agq[cpu].remove_expired();

    // Prefetch From CMC_AGQ
    auto cmc_agq_item = cmc_agq[cpu].first_ready_item();
    if(cmc_agq_item != cmc_agq[cpu].entries.end()){

        if(!load_identity[cpu].contain(cmc_agq_item->ip)){
            cmc_agq[cpu].entries.erase(cmc_agq_item);
            cmc_agq[cpu].CMC_AGQ_IP_EXPIRED++;
        } else {
            auto load_id_item = load_identity[cpu].entries[cmc_agq_item->ip];

            bool has_successor = load_identity[cpu].has_successor(cmc_agq_item->ip);
            uint64_t pf_address  = cmc_agq_item->ret_value + load_id_item.offset;
            uint64_t pf_metadata = 0xabcdefabc;
            if(pf_address & 0xffffff0000000000 || pf_address == 0){
                cmc_agq[cpu].CMC_AGQ_BEYOND_NUM++;
                cmc_agq[cpu].entries.erase(cmc_agq_item);
            } else {
                int pf_dload_succ = prefetch_line(pf_address, pf_this_level, pf_metadata);
                dload_cnt++; pf_dload_succ_cnt += pf_dload_succ;

                if(pf_dload_succ == 1){
                    cmc_agq_item->issued = true;
                    cmc_agq_item->ret_value = pf_address;
                    if(!has_successor){
                        cmc_agq[cpu].entries.erase(cmc_agq_item);
                    }
                }

            #ifdef PRINT_PREFETCH_LINE
                if(find(begin(print_ip), end(print_ip), cmc_agq_item->ip) != end(print_ip) && current_cycle >= start_print_cycle){
                    cout << "[d-load Pref]" 
                         << " cycle: " << current_cycle
                         << ", ip: " << hex << cmc_agq_item->ip << dec
                         << ", addr: " << hex << pf_address << dec
                         << ", dbg_cycle: " << cmc_agq_item->cycle
                         << endl;
                }
            #endif
            }
        }
    }

    // Metadate WriteQ
    if (metadata_onchip[cpu].writeQ.size() != 0) {
        write_entry* write = &metadata_onchip[cpu].writeQ.front();
        if(write->first_write){
            BLOCK& evict_block = static_cast<CACHE*>(lower_level)->block[write->index * static_cast<CACHE*>(lower_level)->NUM_WAY + static_cast<CACHE*>(lower_level)->current_assoc + write->way];
            bool evicting_dirty = (static_cast<CACHE*>(lower_level)->lower_level != NULL) && evict_block.dirty;

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
    if(metadata_in == 0xabc){
        cache_fill_rec_pref_cnt++;
    } else if(metadata_in == 0xabcdef){
        cache_fill_dlink_cnt++;
    }

    cmc_agq[cpu].load_return(addr);

    return metadata_in;
}


void CACHE::prefetcher_final_stats() {
    printf("metadata total assoc: %lu\n", metadata_onchip[cpu].get_assoc());

    load_identity[cpu].print_final_stats();
    cmc_agq[cpu].print_final_stats();
    metadata_onchip[cpu].print_stats();

    cout << "rec_pref_cnt: " << rec_pref_cnt 
         << ", pf_rec_succ_cnt: " << pf_rec_succ_cnt
         << ", cache_fill_rec_pref_cnt: " << cache_fill_rec_pref_cnt
         << endl;

    cout << "dlink_cnt: " << dlink_cnt 
         << ", pf_dlink_succ_cnt: " << pf_dlink_succ_cnt
         << ", cache_fill_dlink_cnt: " << cache_fill_dlink_cnt
         << endl;

    cout << "dload_cnt: " << dload_cnt
         << ", pf_dload_succ_cnt: " << pf_dload_succ_cnt
         << ", cache_fill_dload_cnt: " << cache_fill_dload_cnt
         << endl;
    cout << "lut_access_num: " << lut_access_num << endl;
    cout << "lut_hit_num: " << lut_hit_num << endl;

#if COLLECT_LOAD_PC != 0xffffffffffffffff
    cout << "The Collected PC: " << hex << COLLECT_LOAD_PC << dec;
    cout << ", the Number of Address: " << load_info.size() << endl;

    uint64_t total_num = 0, hit_count = 0, prefed_miss_count = 0, same_count = 0;
    for (auto &[key, value] : load_info) {
        total_num += value.total_count;
        hit_count += value.hit_count;
        prefed_miss_count += value.prefed_miss_count;
        same_count += value.next_same_count;
    }
    cout << "Total Num: " << total_num
         << ", Hit Count: " << hit_count
         << ", Prefed Miss Count: " << prefed_miss_count
         << ", Same Count: " << same_count
         << endl;

    std::vector<uint64_t> load_addr;
    for(auto &[key, value]: load_info)
        load_addr.emplace_back(key);

    sort(load_addr.begin(), load_addr.end(), [&](const uint64_t&a, const uint64_t&b)->bool{
        return load_info[a].total_count==load_info[b].total_count? a<b : load_info[a].total_count>load_info[b].total_count;
    });

    uint32_t addr_num = 50;
    uint32_t addr_icount = 0;
    for(auto i = load_addr.begin(); i != load_addr.end(); i++){
        printf("[%02d]: addr: 0x%8lx, total_num: %8ld, hit_num: %8ld, prefed_miss_num: %8ld, prev_same: %8ld\n, next_same: %8ld\n", 
                        addr_icount, *i , load_info[*i].total_count, load_info[*i].hit_count,
                        load_info[*i].prefed_miss_count, load_info[*i].prev_same_count, load_info[*i].next_same_count);

        //icount++;
        if(++addr_icount == addr_num)
            break;
    }

#endif
}
