/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <algorithm>
#include <iterator>

#include "champsim.h"
#include "champsim_constants.h"
#include "cache.h"
#include "util.h"
#include "vmem.h"
#include "memory_data.h"
#include "prefetch.h"
#include "cdp.h"
#include "triage.h"
#include "cmc.h"
#include "adatp.h"
#include "berti.h"

extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern VirtualMemory vmem;
extern uint8_t warmup_complete[NUM_CPUS];

#ifdef ENABLE_CMC
extern CMC_AGQ cmc_agq[NUM_CPUS];
extern MetaData_OnChip metadata_onchip[NUM_CPUS];
#endif

#ifdef ENABLE_AdaTP
extern AdaTP_MetaData_OnChip adatp_metadata_onchip[NUM_CPUS];
#endif

#ifdef ENABLE_TRIAGE
extern Triage triage[NUM_CPUS];
#endif

/* adjust current available association of cache */
void CACHE::adjust_assoc(uint32_t target){
    current_assoc = target;
}

/* refill cache from mshr */
void CACHE::handle_fill()
{
    while (writes_available_this_cycle > 0) {
        auto fill_mshr = MSHR.begin();
        if (fill_mshr == std::end(MSHR) || fill_mshr->event_cycle > current_cycle)
            return;

        // find victim
        uint32_t set = get_set(fill_mshr->address);
        auto set_begin = std::next(std::begin(block), set * NUM_WAY);
        auto set_end = std::next(set_begin, current_assoc);
        auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
        uint32_t way = std::distance(set_begin, first_inv);
        if (way == current_assoc)
        way = impl_replacement_find_victim(fill_mshr->cpu, fill_mshr->instr_id, set, &block.data()[set * NUM_WAY], fill_mshr->ip, fill_mshr->address,
                                            fill_mshr->type);

        // bypass metadata from LLC
        if(fill_mshr->type == METADATA){
            // COLLECT STATS
            sim_miss[fill_mshr->cpu][fill_mshr->type]++;
            sim_access[fill_mshr->cpu][fill_mshr->type]++;

            assert (fill_mshr->fill_level == FILL_LLC);
            assert (NAME == "LLC");

            CACHE* L1Cache = static_cast<CACHE*>(ooo_cpu[fill_mshr->cpu]->L1D_bus.lower_level);
            assert (L1Cache->NAME.rfind("L1D") != std::string::npos);
            #ifdef OFFCHIP_METADATA
            #if TEMPORAL_L1D == true
                L1Cache->complete_metadata_req(fill_mshr->address);
            #else
                CACHE* L2Cache = static_cast<CACHE*>(L1Cache->lower_level);
                assert (L2Cache->NAME.rfind("L2C") != std::string::npos);
                L2Cache->complete_metadata_req(fill_mshr->address);
            #endif
            #else
                cout << "Do not define OFFCHIP_METADATA!" << endl; exit(1);
            #endif

            MSHR.erase(fill_mshr);
            writes_available_this_cycle--;

            return;
        }

        bool success = filllike_miss(set, way, *fill_mshr);
        if (!success)
            return;
        if (way != current_assoc) {
            // update processed packets
            fill_mshr->data = block[set * NUM_WAY + way].data;

        for (auto ret : fill_mshr->to_return)
            ret->return_data(&(*fill_mshr));
        }

        MSHR.erase(fill_mshr);
        writes_available_this_cycle--;
    }
}

void CACHE::handle_writeback()
{
    while (writes_available_this_cycle > 0) {
        if (!WQ.has_ready())
            return;

        // handle the oldest entry
        PACKET& handle_pkt = WQ.front();

        // access cache
        uint32_t set = get_set(handle_pkt.address);
        uint32_t way = get_way(handle_pkt.address, set);

        if(handle_pkt.type == METADATA){
            assert (NAME == "LLC");
            #ifndef OFFCHIP_METADATA
                cout << "Do not define OFFCHIP_METADATA!" << endl; exit(1);
            #endif
            way = NUM_WAY;
        }

        BLOCK& fill_block = block[set * NUM_WAY + way];

        if (way < current_assoc) { // HIT
            assert(handle_pkt.type != METADATA);
            impl_replacement_update_state(handle_pkt.cpu, set, way, fill_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

            // COLLECT STATS
            sim_hit[handle_pkt.cpu][handle_pkt.type]++;
            sim_access[handle_pkt.cpu][handle_pkt.type]++;

            // mark dirty
            fill_block.dirty = 1;
        } else { // MISS
            if(handle_pkt.type == METADATA){
                #ifndef OFFCHIP_METADATA
                    cout << "Do not define OFFCHIP_METADATA!" << endl; exit(1);
                #endif
                PACKET wb_packet;

                wb_packet.fill_level = FILL_DRAM;
                wb_packet.type = METADATA;
                wb_packet.cpu = handle_pkt.cpu;
                wb_packet.address = handle_pkt.address;
                wb_packet.v_address = handle_pkt.v_address;
                wb_packet.data = handle_pkt.data;
                wb_packet.event_cycle = current_core_cycle[handle_pkt.cpu];

                lower_level->add_wq(&wb_packet);
                // COLLECT STATS
                sim_miss[handle_pkt.cpu][handle_pkt.type]++;
                sim_access[handle_pkt.cpu][handle_pkt.type]++;

                writes_available_this_cycle--;
                WQ.pop_front();
                return ;
            }

            bool success;
            if (handle_pkt.type == RFO && handle_pkt.to_return.empty()) {
                success = readlike_miss(handle_pkt);
            } else {
                // find victim
                auto set_begin = std::next(std::begin(block), set * NUM_WAY);
                auto set_end = std::next(set_begin, current_assoc);
                auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
                way = std::distance(set_begin, first_inv);
                if (way == current_assoc)
                way = impl_replacement_find_victim(handle_pkt.cpu, handle_pkt.instr_id, set, &block.data()[set * NUM_WAY], handle_pkt.ip, handle_pkt.address,
                                                    handle_pkt.type);

                success = filllike_miss(set, way, handle_pkt);
            }

            if (!success)
                return;
        }

        // remove this entry from WQ
        writes_available_this_cycle--;
        WQ.pop_front();
    }
}

void CACHE::handle_read()
{
    while (reads_available_this_cycle > 0) {
        if (!RQ.has_ready())
            return;

        // handle the oldest entry
        PACKET& handle_pkt = RQ.front();
        assert(handle_pkt.type != METADATA);

        // A (hopefully temporary) hack to know whether to send the evicted paddr or
        // vaddr to the prefetcher
        ever_seen_data |= (handle_pkt.v_address != handle_pkt.ip);

        uint32_t set = get_set(handle_pkt.address);
        uint32_t way = get_way(handle_pkt.address, set);

        sim_read_access[handle_pkt.cpu][handle_pkt.type]++;
        if (way < current_assoc) { // HIT
            sim_read_hit[handle_pkt.cpu][handle_pkt.type]++;
            readlike_hit(set, way, handle_pkt);
        } else {
            sim_read_miss[handle_pkt.cpu][handle_pkt.type]++;
            bool success = readlike_miss(handle_pkt);
            if (!success)
                return;
        }

        #ifdef COLLECT_CACHE_LOAD_INFO
        // Collect each load's hit/miss info
        if (warmup_complete[handle_pkt.cpu]) {
            auto it = sim_load_info[handle_pkt.cpu].find(handle_pkt.ip);
            if(it == sim_load_info[handle_pkt.cpu].end()) {
                sim_load_info[handle_pkt.cpu].insert(pair<uint64_t, load_miss_info_t>(handle_pkt.ip, {1, 0}));
                it = sim_load_info[handle_pkt.cpu].find(handle_pkt.ip);
            } else {
                it->second.total_count++;
            }

            if (way >= current_assoc) { // Miss
                it->second.miss_count++;
            }
        }
        #endif

        // remove this entry from RQ
        RQ.pop_front();
        reads_available_this_cycle--;
    }
}

void CACHE::handle_prefetch()
{
    uint32_t pref_available_this_cycle = 1;
    if(NAME.rfind("L1D") == std::string::npos){
        pref_available_this_cycle = reads_available_this_cycle;
    }

    while (pref_available_this_cycle > 0) {
        if (!PQ.has_ready())
        return;

        // handle the oldest entry
        PACKET& handle_pkt = PQ.front();

        uint32_t set = get_set(handle_pkt.address);
        uint32_t way = get_way(handle_pkt.address, set);

        if(handle_pkt.type == METADATA){
            assert (NAME == "LLC");
            #ifndef OFFCHIP_METADATA
                cout << "Do not define OFFCHIP_METADATA!" << endl; exit(1);
            #endif
            way = NUM_WAY;
        }

        if (way < current_assoc) // HIT
        {
            readlike_hit(set, way, handle_pkt);
            if(NAME.rfind("L1D") != std::string::npos){
                #ifdef ENABLE_CMC
                cmc_agq[cpu].load_return(handle_pkt.v_address);
                #endif
                #ifdef ENABLE_CDP
                cdp_handle_prefetch(cpu, this, handle_pkt.ip, handle_pkt.v_address, handle_pkt.pf_metadata);
                #endif
            }
        } else {
            bool success = readlike_miss(handle_pkt);
            if (!success) {
                return;
            }
            else {
                if (handle_pkt.fill_level == fill_level)
                    notify_prefetch(handle_pkt.v_address >> LOG2_BLOCK_SIZE, handle_pkt.ip, handle_pkt.cpu, current_core_cycle[handle_pkt.cpu]);
            }
        }

        // remove this entry from PQ
        PQ.pop_front();
        pref_available_this_cycle--;
    }
}

void CACHE::readlike_hit(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
    BLOCK& hit_block = block[set * NUM_WAY + way];
    handle_pkt.data = hit_block.data;

    // update prefetcher on load instruction
    if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
        cpu = handle_pkt.cpu;
        uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address); //& ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
        handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 1, hit_block.prefetch, handle_pkt.type, handle_pkt.pf_metadata);
    }

    // update replacement policy
    impl_replacement_update_state(handle_pkt.cpu, set, way, hit_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

    // COLLECT STATS
    sim_hit[handle_pkt.cpu][handle_pkt.type]++;
    sim_access[handle_pkt.cpu][handle_pkt.type]++;

    for (auto ret : handle_pkt.to_return)
        ret->return_data(&handle_pkt);

    // update prefetch stats and reset prefetch bit
    if (hit_block.prefetch) {
        pf_useful++;
        hit_block.prefetch = 0;
        // For IPCP
        if (hit_block.pref_type < 6) pref_useful[handle_pkt.cpu][hit_block.pref_type]++;
    }
}

bool CACHE::readlike_miss(PACKET& handle_pkt)
{
    // check mshr
    auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address, OFFSET_BITS));
    bool mshr_full = (MSHR.size() == MSHR_SIZE);

    // update prefetcher on load instructions and prefetches from upper levels
    if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
        cpu = handle_pkt.cpu;
        uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address); //& ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
        handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 0, 0, handle_pkt.type, handle_pkt.pf_metadata);
    }

    if (mshr_entry != MSHR.end()) // miss already inflight
    {
        // update fill location
        mshr_entry->fill_level = std::min(mshr_entry->fill_level, handle_pkt.fill_level);

        packet_dep_merge(mshr_entry->lq_index_depend_on_me, handle_pkt.lq_index_depend_on_me);
        packet_dep_merge(mshr_entry->sq_index_depend_on_me, handle_pkt.sq_index_depend_on_me);
        packet_dep_merge(mshr_entry->instr_depend_on_me, handle_pkt.instr_depend_on_me);
        packet_dep_merge(mshr_entry->to_return, handle_pkt.to_return);

        if (mshr_entry->type == PREFETCH && !(handle_pkt.type == PREFETCH && handle_pkt.fill_level == fill_level)) {
            // Mark the prefetch as useful
            if (mshr_entry->pf_origin_level == fill_level){
                pf_useful++;
                pf_late++;
                // For IPCP
                uint32_t pref_type = (mshr_entry->pf_metadata & 0xE00) >> 9;
                if (pref_type < 6) pref_useful[mshr_entry->cpu][pref_type]++;
            }

            uint64_t prior_event_cycle = mshr_entry->event_cycle;
            auto prior_to_return = std::move(mshr_entry->to_return);
            uint64_t prior_pf_metadata = mshr_entry->pf_metadata;
            *mshr_entry = handle_pkt;

            // in case request is already returned, we should keep event_cycle
            mshr_entry->event_cycle = prior_event_cycle;
            mshr_entry->to_return = std::move(prior_to_return);
            mshr_entry->pf_metadata = prior_pf_metadata;
        }
    } else {
        if (mshr_full){  // not enough MSHR resource
            MSHR_FULL++;
            return false;
        }

        bool is_read = prefetch_as_load || (handle_pkt.type != PREFETCH);

        // check to make sure the lower level queue has room for this read miss
        int queue_type = (is_read) ? 1 : 3;
        if (lower_level->get_occupancy(queue_type, handle_pkt.address) == lower_level->get_size(queue_type, handle_pkt.address))
            return false;

        // Allocate an MSHR
        if (handle_pkt.fill_level <= fill_level) {
            auto it = MSHR.insert(std::end(MSHR), handle_pkt);
            it->cycle_enqueued = current_cycle;
            it->event_cycle = std::numeric_limits<uint64_t>::max();
        }

        if (handle_pkt.fill_level <= fill_level)
            handle_pkt.to_return = {this};
        else
            handle_pkt.to_return.clear();

        if (!is_read)
            lower_level->add_pq(&handle_pkt);
        else
            lower_level->add_rq(&handle_pkt);
        
        if(handle_pkt.type == PREFETCH && handle_pkt.fill_level == fill_level) {
            pf_miss_issued++;      
            // For IPCP
            uint32_t pref_type = (handle_pkt.pf_metadata & 0xE00) >> 9;
            if (pref_type < 6) pref_filled[handle_pkt.cpu][pref_type]++;
        }    // update prefetcher on load instructions and prefetches from upper levels
    }

    return true;
}

bool CACHE::filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
    bool bypass = (way == current_assoc);
    #ifndef LLC_BYPASS
    assert(!bypass);
    #endif
    assert(handle_pkt.type != WRITEBACK || !bypass);

    BLOCK& fill_block = block[set * NUM_WAY + way];
    bool evicting_dirty = !bypass && (lower_level != NULL) && fill_block.dirty;
    uint64_t evicting_address = 0;

    if (!bypass) {
        if (evicting_dirty) {
            PACKET writeback_packet;

            writeback_packet.fill_level = lower_level->fill_level;
            writeback_packet.cpu = handle_pkt.cpu;
            writeback_packet.address = fill_block.address;
            writeback_packet.data = fill_block.data;
            writeback_packet.instr_id = handle_pkt.instr_id;
            writeback_packet.ip = 0;
            writeback_packet.type = WRITEBACK;

            auto result = lower_level->add_wq(&writeback_packet);
            if (result == -2)
                return false;
        }

        if (ever_seen_data)
            evicting_address = fill_block.address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
        else
            evicting_address = fill_block.v_address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);

        if (fill_block.prefetch) {
            pf_useless++;
        }

        if (handle_pkt.type == PREFETCH && handle_pkt.fill_level == fill_level) 
            pf_fill++;

        assert(handle_pkt.type != METADATA);
        fill_block.valid = true;
        fill_block.prefetch = (handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level == fill_level);
        // For IPCP
        fill_block.pref_type = (handle_pkt.pf_metadata & 0xE00) >> 9;
        fill_block.dirty = (handle_pkt.type == WRITEBACK || (handle_pkt.type == RFO && handle_pkt.to_return.empty()));
        fill_block.address = handle_pkt.address;
        fill_block.v_address = handle_pkt.v_address;
        fill_block.data = handle_pkt.data;
        fill_block.ip = handle_pkt.ip;
        fill_block.cpu = handle_pkt.cpu;
        fill_block.instr_id = handle_pkt.instr_id;
    }

    if (warmup_complete[handle_pkt.cpu] && (handle_pkt.cycle_enqueued != 0))
        total_miss_latency += current_cycle - handle_pkt.cycle_enqueued;

    // update prefetcher
    cpu = handle_pkt.cpu;
    uint64_t addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) /*& ~bitmask(match_offset_bits ? 0 : OFFSET_BITS)*/;

    if (handle_pkt.type == LOAD || handle_pkt.type == PREFETCH) {
        handle_pkt.pf_metadata =
        impl_prefetcher_cache_fill(addr, set, way,
                                    handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level == fill_level, evicting_address, handle_pkt.pf_metadata, handle_pkt.ip);
    }

    // update replacement policy
    impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, 0, handle_pkt.type, 0);

    // COLLECT STATS
    sim_miss[handle_pkt.cpu][handle_pkt.type]++;
    sim_access[handle_pkt.cpu][handle_pkt.type]++;

    return true;
}

void CACHE::operate()
{
  uint32_t writes_available_this_cycle = operate_writes();
  uint32_t reads_available_this_cycle = operate_reads();

  assert(NAME=="LLC" || stoul(NAME.substr(3,1))==cpu);

  #ifdef ENABLE_CMC
  if (NAME.rfind("L2C") != std::string::npos) {
    metadata_onchip[cpu].write(writes_available_this_cycle,current_cycle);
    metadata_onchip[cpu].read(reads_available_this_cycle,current_cycle);
  }
  #endif
  #ifdef ENABLE_AdaTP
  if (NAME.rfind("LLC") != std::string::npos) {
    adatp_metadata_onchip[cpu].write(writes_available_this_cycle,current_cycle);
    adatp_metadata_onchip[cpu].read(reads_available_this_cycle,current_cycle);
  }
  #endif
  #ifdef ENABLE_TRIAGE
  if (NAME.rfind("LLC") != std::string::npos) {
    for(uint32_t i = 0; i < writes_available_this_cycle; i++){
      triage[cpu].write(current_cycle);
    }
    for(uint32_t i = 0; i < reads_available_this_cycle; i++){
      triage[cpu].read(current_cycle);
    }
  }
  #endif
  impl_prefetcher_cycle_operate();
}

uint32_t CACHE::operate_writes()
{
  // perform all writes
  writes_available_this_cycle = MAX_WRITE;
  handle_fill();
  handle_writeback();

  WQ.operate();
  return writes_available_this_cycle;
}

uint32_t CACHE::operate_reads()
{
  // perform all reads
  reads_available_this_cycle = MAX_READ;
  handle_read();
  va_translate_prefetches();
  handle_prefetch();

  RQ.operate();
  PQ.operate();
  VAPQ.operate();
  return reads_available_this_cycle;
}

uint32_t CACHE::get_set(uint64_t address) { return ((address >> OFFSET_BITS) & bitmask(lg2(NUM_SET))); }

uint32_t CACHE::get_way(uint64_t address, uint32_t set)
{
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, current_assoc);
  return std::distance(begin, std::find_if(begin, end, eq_addr<BLOCK>(address, OFFSET_BITS)));
}

int CACHE::check_hit(PACKET *packet)
{
    uint32_t set = get_set(packet->address);
    uint32_t way = get_way(packet->address, set);

    if (way < current_assoc) {// HIT
        return way;
    } else {
        return -1;
    }
}

int CACHE::invalidate_entry(uint64_t inval_addr)
{
  uint32_t set = get_set(inval_addr);
  uint32_t way = get_way(inval_addr, set);

  if (way < current_assoc)
    block[set * NUM_WAY + way].valid = 0;

  return way;
}

int CACHE::add_rq(PACKET* packet)
{
    assert(packet->address != 0);
    RQ_ACCESS++;

    // check for the latest writebacks in the write queue
    champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

    if (found_wq != WQ.end()) {
        packet->data = found_wq->data;
        for (auto ret : packet->to_return)
            ret->return_data(packet);

        WQ_FORWARD++;
        return -1;
    }

    // check for duplicates in the read queue
    auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
    if (found_rq != RQ.end() && packet_can_merge) {
        packet_dep_merge(found_rq->lq_index_depend_on_me, packet->lq_index_depend_on_me);
        packet_dep_merge(found_rq->sq_index_depend_on_me, packet->sq_index_depend_on_me);
        packet_dep_merge(found_rq->instr_depend_on_me, packet->instr_depend_on_me);
        packet_dep_merge(found_rq->to_return, packet->to_return);
        RQ_MERGED++;

        return 0; // merged index
    }

    // check occupancy
    if (RQ.full()) {
        RQ_FULL++;
        return -2; // cannot handle this request
    }

    RQ.push_back(*packet);
    RQ_TO_CACHE++;
    return RQ.occupancy();
}

int CACHE::add_wq(PACKET* packet)
{
    WQ_ACCESS++;

    // check for duplicates in the write queue
    champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

    if (found_wq != WQ.end()) {
        WQ_MERGED++;
        return 0; // merged index
    }

    // Check for room in the queue
    if (WQ.full()) {
        ++WQ_FULL;
        return -2;
    }

    WQ.push_back(*packet);
    WQ_TO_CACHE++;
    WQ_ACCESS++;

    return WQ.occupancy();
}

int CACHE::prefetch_line(uint64_t pf_addr, uint8_t pf_fill_level, uint64_t prefetch_metadata)
{
    if (pf_fill_level == fill_level) {
        pf_requested++;

        PACKET pf_packet;
        pf_packet.type = PREFETCH;
        pf_packet.fill_level = fill_level;
        pf_packet.pf_origin_level = fill_level;
        pf_packet.pf_metadata = prefetch_metadata;
        pf_packet.cpu = cpu;
        pf_packet.address = pf_addr;
        pf_packet.v_address = virtual_prefetch ? pf_addr : 0;

        if (virtual_prefetch) {
            if (!VAPQ.full()) {
                VAPQ.push_back(pf_packet);
                return 1;
            }else {
                VAPQ_FULL++;
            }
        } else {
            int result = add_pq(&pf_packet);
            if (result != -2) {
                if (result > 0) {
                    pf_issued++;
                }
                return 1;
            }
        }

        return 0;
    }
    else {
        if (fill_level == FILL_L1 && virtual_prefetch) {
            pf_addr = vmem.va_to_pa(cpu, pf_addr).first;
        }
        return  static_cast<CACHE*>(lower_level)->prefetch_line(pf_addr, pf_fill_level, prefetch_metadata);
    }
}

int CACHE::get_metadata(uint64_t meta_data_addr)
{
    #ifndef OFFCHIP_METADATA
        cout << "Do not define OFFCHIP_METADATA!" << endl; exit(1);
    #endif
    PACKET pf_packet;
    pf_packet.type = METADATA;
    pf_packet.fill_level = FILL_LLC;
    pf_packet.cpu = cpu;
    pf_packet.address = meta_data_addr;
    pf_packet.v_address = meta_data_addr;
    pf_packet.event_cycle = current_core_cycle[cpu];

    #if TEMPORAL_L1D == true
        assert(NAME.rfind("L1D") != std::string::npos);
        CACHE* L2Cache = static_cast<CACHE*>(lower_level);
        int result = L2Cache->lower_level->add_pq(&pf_packet);
    #else
        assert(NAME.rfind("L2C") != std::string::npos);
        int result = lower_level->add_pq(&pf_packet);
    #endif

    return result;
}

int CACHE::write_metadata(uint64_t meta_data_addr)
{
    #ifndef OFFCHIP_METADATA
        cout << "Do not define OFFCHIP_METADATA!" << endl; exit(1);
    #endif
    PACKET wb_packet;
    wb_packet.type = METADATA;
    wb_packet.fill_level = FILL_DRAM;
    wb_packet.cpu = cpu;
    wb_packet.address = meta_data_addr;
    wb_packet.v_address = meta_data_addr;
    wb_packet.event_cycle = current_core_cycle[cpu];

    #if TEMPORAL_L1D == true
        assert(NAME.rfind("L1D") != std::string::npos);
        CACHE* L2Cache = static_cast<CACHE*>(lower_level);
        L2Cache->lower_level->add_wq(&wb_packet);
    #else
        assert(NAME.rfind("L2C") != std::string::npos);
        lower_level->add_wq(&wb_packet);
    #endif

    return 1;
}

void CACHE::va_translate_prefetches()
{
    // TEMPORARY SOLUTION: mark prefetches as translated after a fixed latency
    if (VAPQ.has_ready()) {
        VAPQ.front().address = vmem.va_to_pa(cpu, VAPQ.front().v_address).first;

        // move the translated prefetch over to the regular PQ
        int result = add_pq(&VAPQ.front());

        if (result > 0) {
            pf_issued++;
        }

        // remove the prefetch from the VAPQ
        if (result != -2)
            VAPQ.pop_front();

    }
}

int CACHE::add_pq(PACKET* packet)
{
    if(packet->type == METADATA){
        assert(NAME=="LLC");
    }

    assert(packet->address != 0);
    PQ_ACCESS++;

    // check for the latest wirtebacks in the write queue
    champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

    if (found_wq != WQ.end()) {
        packet->data = found_wq->data;
        for (auto ret : packet->to_return)
            ret->return_data(packet);

        if(NAME.rfind("L1D") != std::string::npos){
            idm_load_return(packet->cpu, packet->v_address);
            #ifdef ENABLE_CMC
                cmc_agq[cpu].load_return(packet->v_address);
            #endif
            #ifdef ENABLE_CDP
            cdp_handle_prefetch(cpu, this, packet->ip, packet->v_address, packet->pf_metadata);
            #endif
        }

        WQ_FORWARD++;
        return -1;
    }

    // check for duplicates in the PQ
    auto found = std::find_if(PQ.begin(), PQ.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
    if (found != PQ.end()) {
        found->fill_level = std::min(found->fill_level, packet->fill_level);
        packet_dep_merge(found->to_return, packet->to_return);

        PQ_MERGED++;
        return 0;
    }

    // check occupancy
    if (PQ.full()) {
        PQ_FULL++;
        return -2; // cannot handle this request
    }

    PQ.push_back(*packet);
    PQ_TO_CACHE++;
    return PQ.occupancy();
}

void CACHE::return_data(PACKET* packet)
{
    // check MSHR information
    auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
    auto first_unreturned = std::find_if(MSHR.begin(), MSHR.end(), [](auto x) { return x.event_cycle == std::numeric_limits<uint64_t>::max(); });

    if(packet->type == METADATA){
        assert(NAME=="LLC");
    }

    // sanity check
    if (mshr_entry == MSHR.end()) {
        std::cerr << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet->instr_id << " cannot find a matching entry!";
        std::cerr << " address: " << std::hex << packet->address;
        std::cerr << " v_address: " << packet->v_address;
        std::cerr << " address: " << (packet->address >> OFFSET_BITS) << std::dec;
        std::cerr << " event: " << packet->event_cycle << " current: " << current_cycle << std::endl;
        assert(0);
    }

    // MSHR holds the most updated information about this request
    mshr_entry->data = packet->data;
    mshr_entry->pf_metadata = packet->pf_metadata;
    mshr_entry->event_cycle = current_cycle +  FILL_LATENCY;

    // Order this entry after previously-returned entries, but before non-returned
    // entries
    std::iter_swap(mshr_entry, first_unreturned);
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 0)
        return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
    else if (queue_type == 1)
        return RQ.occupancy();
    else if (queue_type == 2)
        return WQ.occupancy();
    else if (queue_type == 3)
        return PQ.occupancy();

    return 0;
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 0)
        return MSHR_SIZE;
    else if (queue_type == 1)
        return RQ.size();
    else if (queue_type == 2)
        return WQ.size();
    else if (queue_type == 3)
        return PQ.size();

    return 0;
}

bool CACHE::should_activate_prefetcher(int type) { return (1 << static_cast<int>(type)) & pref_activate_mask; }

void CACHE::notify_prefetch(uint64_t addr, uint64_t tag, uint32_t cpu, uint64_t cycle) 
{
    #ifdef ENABLE_BERTI
    latency_table_add(addr, tag, cpu, cycle & TIME_MASK);
    #endif
}


void CACHE::print_deadlock()
{
    if (!std::empty(MSHR)) {
        std::cout << NAME << " MSHR Entry" << std::endl;
        std::size_t j = 0;
        for (PACKET entry : MSHR) {
            std::cout << "[" << NAME << " MSHR] entry: " << j++ << " instr_id: " << entry.instr_id;
            std::cout << " address: " << std::hex << (entry.address >> LOG2_BLOCK_SIZE) << " full_addr: " << entry.address << std::dec << " type: " << +entry.type;
            std::cout << " fill_level: " << +entry.fill_level << " event_cycle: " << entry.event_cycle;
            cout << " to_return_size: " << entry.to_return.size()
            << std::endl;
        }
    } else {
        std::cout << NAME << " MSHR empty" << std::endl;
    }
}
