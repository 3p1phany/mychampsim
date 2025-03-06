#include "ooo_cpu.h"
#include "cache.h"
#include <vector>

#define NUM_IP_TABLE_ENTRIES 48
#define NUM_GLOBAL_TABLE_ENTRIES 8 

class IP_TABLE_L1 {
    public:
    uint8_t  state;
    uint16_t ip;
    uint8_t  rrip;
    uint32_t addr;
    uint8_t  cycle;
    uint8_t  step;
    uint32_t stride;
    uint8_t  pstride[5];
    int32_t  delta_sum;
    uint8_t  pat_hist;
    uint8_t  pat_last;
    uint8_t  useline;

    IP_TABLE_L1 () {
        state = 0;
        ip = 0;
        rrip = 0;
        addr = 0;
        cycle = 0;
        step = 0;
        stride = 0;
        delta_sum = 0;
        pat_hist = 0;
        pat_last = 0;
        for(int i=0; i<5; i++)
            pstride[i] = 0;
    };
};

/*	IP TABLE STORAGE OVERHEAD: 288 Bytes

	Single Entry:

	FIELD					STORAGE (bits)
    state                   2
    ip                      16
    rrip                    3
    addr                    32
    cycle                   5
    step                    2
    stride                  24
    pstride                 20
    delta_sum               18
    pat_hist                5
    pat_last                3
    useline                 1
	
	Total 					131

	Full Table Storage Overhead: 

	48 entries * 131 bits = 6288 bits = 786 Bytes
*/

class GLOBAL_TABLE_L1 {
    public:
        uint32_t region_tag;		
        uint8_t  dense_cnt;
        uint16_t bitmap;
        uint8_t  direction;
        uint8_t  dense;
        uint8_t  rrip;
        uint8_t  cycle;
        uint8_t  step;
        uint8_t  gid;

        GLOBAL_TABLE_L1 () {
            region_tag = 0;
            dense_cnt = 0;
            bitmap = 0;
            direction = 0;
            dense = 0;
            rrip = 0;
            cycle = 0;
            step = 0;
            gid = 0;
        };
};

/*	REGION STREAM TABLE STORAGE OVERHEAD:

	Single Entry: 

	FIELD					STORAGE (bits)
    region_tag				16
    dense_cnt				5
    bitmap					16
    direction				1
    dense                   1
    rrip					3
    cycle					5
    step					2
    gid                     3

	Total					52

	Full Table Storage Overhead:

	8 entries * 52 bits = 416 bits = 52 Bytes

*/

std::map<uint16_t, IP_TABLE_L1> ip_table[NUM_CPUS];
std::map<uint32_t, GLOBAL_TABLE_L1> global_table[NUM_CPUS];

uint32_t l1_pf_total_interval = 0;
uint32_t l1_pf_useful_interval = 0, l1_pf_useful_cnt = 0;
uint32_t l1_pf_late_interval = 0, l1_pf_late_cnt = 0;
uint32_t l1_last_pf_total, l1_last_pf_useful, l1_last_pf_late;
uint8_t l1_depth = 2;

uint32_t l2_pf_total_interval = 0;
uint32_t l2_pf_useful_interval = 0, l2_pf_useful_cnt = 0;
uint32_t l2_pf_late_interval = 0, l2_pf_late_cnt = 0;
uint32_t l2_last_pf_total, l2_last_pf_useful, l2_last_pf_late;
uint8_t l2_depth = 2;

uint32_t l3_pf_total_interval = 0;
uint32_t l3_pf_useful_interval = 0, l3_pf_useful_cnt = 0;
uint32_t l3_pf_late_interval = 0, l3_pf_late_cnt = 0;
uint32_t l3_last_pf_total, l3_last_pf_useful, l3_last_pf_late;
uint8_t l3_depth = 2;

uint64_t stride_pf_num = 0;
uint64_t stream_pf_num = 0;
uint64_t pattern_pf_num = 0;

uint64_t generate_stride_addr(uint64_t addr, uint32_t stride, uint8_t step, uint8_t radix, uint8_t depth) {
    if (depth == 0) {
        return 0;
    }
    if (depth == 1 && step > 0) {
        step -= 1;
    }
    if (depth == 3 && step < 2) {
        step += 1;
    }
    int64_t ext_stride = static_cast<int32_t>(stride << 8) >> 8;
    int64_t delta = (step >= 2) ? ext_stride << (3+radix) : 
                    (step == 1) ? ext_stride << (2+radix) :
                                  ext_stride << (1+radix) ;
    return addr + delta;
}
uint64_t generate_stream_addr(uint64_t addr, uint8_t direction, uint8_t step, uint8_t radix, uint8_t depth) {
    if (depth == 0) {
        return 0;
    }
    if (depth == 1 && step > 0) {
        step -= 1;
    }
    if (depth == 3 && step < 2) {
        step += 1;
    }
    int64_t ext_stride = direction ? -0x40 : 0x40;
    int64_t delta = (step >= 2) ? ext_stride << (3+radix) : 
                    (step == 1) ? ext_stride << (2+radix) :
                                  ext_stride << (1+radix) ;
    return addr + delta;
}

void CACHE::prefetcher_initialize() 
{
    cout << "L1D [LA864] Prefetcher" << endl;
}

void CACHE::prefetcher_cycle_operate()
{
    for (auto it = ip_table[cpu].begin(); it != ip_table[cpu].end(); it++) {
        if (it->second.state != 0) {
            it->second.cycle++;
            if (it->second.cycle >= 32) {
                it->second.cycle = 31;
            }
        }
    }
    for (auto it = global_table[cpu].begin(); it != global_table[cpu].end(); it++) {
        if (it->second.dense_cnt != 0) {
            it->second.cycle++;
            if (it->second.cycle >= 32) {
                it->second.cycle = 31;
            }
        }
    }
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in)
{
    l1_pf_total_interval += (pf_fill- l1_last_pf_total);
    l1_pf_useful_interval += (pf_useful - l1_last_pf_useful);
    l1_pf_late_interval += (pf_late - l1_last_pf_late);
    l1_last_pf_total = pf_fill;
    l1_last_pf_useful = pf_useful;
    l1_last_pf_late = pf_late;
    if (l1_pf_total_interval >= 0xFF) {
        l1_pf_late_cnt = (l1_pf_late_interval + l1_pf_late_cnt) / 2;
        l1_pf_useful_cnt = (l1_pf_useful_interval + l1_pf_useful_cnt) / 2;

        bool acc_high = (l1_pf_useful_cnt * 100) / 0xFF > 75;
        bool acc_low = (l1_pf_useful_cnt * 100) / 0XFF < 12.5;
        bool acc_med = !acc_high && !acc_low;
        bool late_high = (l1_pf_late_cnt * 100) / 0XFF > 6.25;
        bool depth_increase = acc_high && late_high;
        bool depth_decrease = acc_med && !late_high || acc_low;
        if (depth_increase) {
            if (l1_depth < 3) {
                l1_depth += 1;
            }
        }
        else if (depth_decrease) {
            if (l1_depth > 1) {
                l1_depth -= 1;
            }
        }

        l1_pf_total_interval = 0;
        l1_pf_useful_interval = 0;
        l1_pf_late_interval = 0;
    }

    CACHE* l2 = static_cast<CACHE*>(lower_level);
    l2_pf_total_interval += (l2->pf_fill- l2_last_pf_total);
    l2_pf_useful_interval += (l2->pf_useful - l2_last_pf_useful);
    l2_pf_late_interval += (l2->pf_late - l2_last_pf_late);
    l2_last_pf_total = l2->pf_fill;
    l2_last_pf_useful = l2->pf_useful;
    l2_last_pf_late = l2->pf_late;
    if (l2_pf_total_interval >= 0xFF) {
        l2_pf_late_cnt = (l2_pf_late_interval + l2_pf_late_cnt) / 2;
        l2_pf_useful_cnt = (l2_pf_useful_interval + l2_pf_useful_cnt) / 2;

        bool acc_high = (l2_pf_useful_cnt * 100) / 0xFF > 75;
        bool acc_low = (l2_pf_useful_cnt * 100) / 0XFF < 12.5;
        bool acc_med = !acc_high && !acc_low;
        bool late_high = (l2_pf_late_cnt * 100) / 0XFF > 6.25;
        bool depth_increase = acc_high && late_high;
        bool depth_decrease = acc_med && !late_high || acc_low;
        if (depth_increase) {
            if (l2_depth < 3) {
                l2_depth += 1;
            }
        }
        else if (depth_decrease) {
            if (l2_depth > 0) {
                l2_depth -= 1;
            }
        }

        l2_pf_total_interval = 0;
        l2_pf_useful_interval = 0;
        l2_pf_late_interval = 0;
    }

    CACHE* l3 = static_cast<CACHE*>(static_cast<CACHE*>(lower_level)->lower_level);
    l3_pf_total_interval += (l3->pf_fill- l3_last_pf_total);
    l3_pf_useful_interval += (l3->pf_useful - l3_last_pf_useful);
    l3_pf_late_interval += (l3->pf_late - l3_last_pf_late);
    l3_last_pf_total = l3->pf_fill;
    l3_last_pf_useful = l3->pf_useful;
    l3_last_pf_late = l3->pf_late;
    if (l3_pf_total_interval >= 0xFF) {
        l3_pf_late_cnt = (l3_pf_late_interval + l3_pf_late_cnt) / 2;
        l3_pf_useful_cnt = (l3_pf_useful_interval + l3_pf_useful_cnt) / 2;

        bool acc_high = (l3_pf_useful_cnt * 100) / 0xFF > 75;
        bool acc_low = (l3_pf_useful_cnt * 100) / 0XFF < 12.5;
        bool acc_med = !acc_high && !acc_low;
        bool late_high = (l3_pf_late_cnt * 100) / 0XFF > 6.25;
        bool depth_increase = acc_high && late_high;
        bool depth_decrease = acc_med && !late_high || acc_low;
        if (depth_increase) {
            if (l3_depth < 3) {
                l3_depth += 1;
            }
        }
        else if (depth_decrease) {
            if (l3_depth > 0) {
                l3_depth -= 1;
            }
        }

        l3_pf_total_interval = 0;
        l3_pf_useful_interval = 0;
        l3_pf_late_interval = 0;
    }

    uint16_t ip_tag = ((ip >> 2) & 0xFFFF) ^ ((ip >> 18) & 0xFFFF);
    uint64_t pf_l1_addr = 0;
    uint64_t pf_l2_addr = 0;
    uint64_t pf_l3_addr = 0;
    uint64_t pf_type = 0;
    uint64_t pf_metadata = 0;
    
    if (ip_table[cpu].find(ip_tag) != ip_table[cpu].end()) {
        IP_TABLE_L1 *entry = &ip_table[cpu][ip_tag];

        // prefetch
        int32_t bstride = (addr & 0xFFFFFFFF) - entry->addr;
        int32_t lstride = (addr & 0xFFFFFFC0) - (entry->addr & 0xFFFFFFC0);
        int32_t stride = entry->useline ? lstride : bstride;
        uint32_t pstride = (bstride & 0xF) ^ ((bstride >> 4) & 0xF) ^ ((bstride >> 8) & 0xF) ^ ((bstride >> 12) & 0xF);
        if  ((entry->delta_sum > 0x40 || entry->delta_sum < -0x40) && entry->pat_last > 1 && entry->pat_hist == 0x1F) {
            pf_l1_addr = addr + entry->delta_sum;
            pf_l2_addr = addr + ((entry->delta_sum) << 1);
            pf_l3_addr = addr + ((entry->delta_sum) << 3);
            pf_type = 3;
            pf_metadata = std::distance(ip_table[cpu].begin(), ip_table[cpu].find(ip_tag)) + 1;
        }
        if (stride != 0 && (entry->state == 3 || (entry->state == 2 && entry->stride == stride))) {
            pf_l1_addr = generate_stride_addr(addr, entry->stride, entry->step, 0, l1_depth); //x2 x4 x8
            pf_l2_addr = generate_stride_addr(addr, entry->stride, entry->step, 1, l2_depth); //x4 x8 x16
            pf_l3_addr = generate_stride_addr(addr, entry->stride, entry->step, 3, l3_depth); //x16 x32 x64
            pf_type = 2;
            pf_metadata = std::distance(ip_table[cpu].begin(), ip_table[cpu].find(ip_tag)) + 1;
        }

        // update
        if (stride != 0) {
            if (entry->stride == stride) {
                entry->state = entry->state + 1;
                if (entry->state > 3) {
                    entry->state = 3;
                }
            }
            else {
                if (entry->state > 1) {
                    entry->state = entry->state - 1;
                }
                if (entry->state == 1) {
                    if ((stride & 0xFF800000) == 0 || (stride & 0xFF800000) == 0xFF800000) {
                        if (stride > 0) {
                            entry->stride = (stride & 0x7FFFFF);
                        }
                        else {
                            entry->stride = (stride | 0xFF800000);
                        }

                        if (stride <= 0x40 && stride >= -0x40) {
                            entry->useline = 1;
                            entry->stride = stride > 0 ? 0x40 : -0x40;
                        }
                        else {
                            entry->useline = 0;
                        }
                    }
                    else 
                        entry->stride = 0;
                }
            }

            if (entry->cycle < 8) {
                entry->step = 2;
            }
            else if (entry->cycle < 16) {
                entry->step = 1;
            }
            else {
                entry->step = 0;
            }

            entry->cycle = 0;
            entry->rrip = entry->rrip < 2 ? 0 : entry->rrip - 2;
        }

        int hit_idx = -1;
        for (int i = 0; i < 5; i++) {
            if (entry->pstride[i] == pstride) {
                hit_idx = i + 1;
                break;
            }
        }
        uint8_t pat_hit = hit_idx != -1 && entry->pat_last == hit_idx;

        if (!pat_hit) {
            entry->delta_sum = 0;
        }
        else if ((entry->pat_hist & ((1 << hit_idx) - 1)) != ((1 << hit_idx) - 1)) {
            if ((bstride & 0xFFFF0000) == 0 || (bstride & 0xFFFF0000) == 0xFFFF0000) {
                entry->delta_sum += bstride;
            }
            else {
                entry->delta_sum = 0;
                entry->pat_hist = 0;
                entry->pat_last = 0;
            }
        }

        entry->pat_hist = ((entry->pat_hist << 1) | pat_hit) & 0x1F;
        entry->pat_last = hit_idx == -1 ? 0 : hit_idx;
        
        for (int i = 4; i > 0; i--) {
            entry->pstride[i] = entry->pstride[i-1];
        }
        entry->pstride[0] = pstride;
        
        entry->addr = addr;
    }
    else if (!cache_hit) {
        bool find = 0;
        if (ip_table[cpu].size() < NUM_IP_TABLE_ENTRIES) {
            IP_TABLE_L1 entry;
            entry.state = 1;
            entry.ip = ip_tag;
            entry.addr = addr;
            entry.cycle = 0;
            entry.step = 0;
            entry.stride = 0;
            entry.delta_sum = 0;
            entry.pat_hist = 0;
            entry.pat_last = 0;
            entry.rrip = 0x5;
            for (int i = 0; i < 5; i++) {
                entry.pstride[i] = 0;
            }
            ip_table[cpu][ip_tag] = entry;
            find = 1;
        }
        else {
            for (auto it = ip_table[cpu].begin(); it != ip_table[cpu].end(); it++) {
                if (it->second.state == 0 || it->second.rrip == 0x7) {
                    ip_table[cpu].erase(it);
                    IP_TABLE_L1 entry;
                    entry.state = 1;
                    entry.ip = ip_tag;
                    entry.addr = addr;
                    entry.cycle = 0;
                    entry.step = 0;
                    entry.stride = 0;
                    entry.delta_sum = 0;
                    entry.pat_hist = 0;
                    entry.pat_last = 0;
                    entry.rrip = 0x5;
                    for (int i = 0; i < 5; i++) {
                        entry.pstride[i] = 0;
                    }
                    ip_table[cpu][ip_tag] = entry;
                    find = 1;
                    break;
                }
            }
        }
        if (!find) {
            for (auto it = ip_table[cpu].begin(); it != ip_table[cpu].end(); it++) {
                it->second.rrip++;
                assert(it->second.rrip < 8);
            }
        }
    }

    uint32_t region_tag = (addr >> (LOG2_BLOCK_SIZE + 4)) & 0xFFFF;
    uint8_t line_offset = (addr >> LOG2_BLOCK_SIZE) & 0xF;
    uint16_t line_mask = 1 << line_offset;
    if (global_table[cpu].find(region_tag) != global_table[cpu].end()) {
        GLOBAL_TABLE_L1 *entry = &global_table[cpu][region_tag];

        // predict
        if (entry->dense) {
            pf_l1_addr = generate_stream_addr(addr, entry->direction, entry->step, 0, l1_depth); // x2 x4 x8
            pf_l2_addr = generate_stream_addr(addr, entry->direction, entry->step, 1, l2_depth); // x4 x8 x16
            pf_l3_addr = generate_stream_addr(addr, entry->direction, entry->step, 3, l3_depth); // x16 x32 x64
            pf_type = 1;
            pf_metadata = NUM_IP_TABLE_ENTRIES + entry->gid + 1;
        }

        // update
        if ((line_mask & entry->bitmap) == 0) {
            entry->dense_cnt += 1;
            assert(entry->dense_cnt <= 16);
            if (entry->dense_cnt >= 12) {
                entry->dense = 1;
            }

            if (entry->cycle < 8) {
                entry->step = 2;
            }
            else if (entry->cycle < 16) {
                entry->step = 1;
            }
            else {
                entry->step = 0;
            }

            entry->cycle = 0;
            entry->rrip = entry->rrip < 2 ? 0 : entry->rrip - 2;
        }
        entry->bitmap |= line_mask;

    }
    else if (!cache_hit) {
        bool find = 0;
        uint8_t gid = 63;
        uint8_t dense = 0;
        uint8_t direction = 0;
        uint8_t step = 0;
        if (global_table[cpu].find(region_tag+1) != global_table[cpu].end()) {
            direction = 1;
            dense = global_table[cpu][region_tag+1].dense;
            step = global_table[cpu][region_tag+1].step;
            gid = global_table[cpu][region_tag+1].gid;
        }
        if (global_table[cpu].find(region_tag-1) != global_table[cpu].end()) {
            direction = 0;
            dense = global_table[cpu][region_tag-1].dense;
            step = global_table[cpu][region_tag-1].step;
            gid = global_table[cpu][region_tag-1].gid;
        }

        // predict
        if (dense) {
            pf_l1_addr = generate_stream_addr(addr, direction, step, 0, l1_depth); // x2 x4 x8
            pf_l2_addr = generate_stream_addr(addr, direction, step, 1, l2_depth); // x4 x8 x16
            pf_l3_addr = generate_stream_addr(addr, direction, step, 3, l3_depth); // x16 x32 x64
            pf_metadata = NUM_IP_TABLE_ENTRIES + gid + 1;
            pf_type = 1;
        }

        if (global_table[cpu].size() < NUM_GLOBAL_TABLE_ENTRIES) {
            GLOBAL_TABLE_L1 entry;
            entry.region_tag = region_tag;
            entry.dense_cnt = 1;
            entry.bitmap = line_mask;
            entry.direction = direction;
            entry.dense = dense;
            entry.rrip = 0x5;
            entry.cycle = 0;
            entry.step = step;
            entry.gid = gid == 63 ? (gid + 1) % NUM_GLOBAL_TABLE_ENTRIES : gid;
            global_table[cpu][region_tag] = entry;
            find = 1;
        }
        else {
            for (auto it = global_table[cpu].begin(); it != global_table[cpu].end(); it++) {
                if (it->second.dense_cnt == 0 || it->second.rrip == 0x7) {
                    global_table[cpu].erase(it);
                    GLOBAL_TABLE_L1 entry;
                    entry.region_tag = region_tag;
                    entry.dense_cnt = 1;
                    entry.bitmap = line_mask;
                    entry.direction = direction;
                    entry.dense = dense;
                    entry.rrip = 0x5;
                    entry.cycle = 0;
                    entry.step = step;
                    entry.gid = gid == 63 ? (gid + 1) % NUM_GLOBAL_TABLE_ENTRIES : gid;
                    global_table[cpu][region_tag] = entry;
                    find = 1;
                    break;
                }
            }
        }
        if (!find) {
            for (auto it = global_table[cpu].begin(); it != global_table[cpu].end(); it++) {
                it->second.rrip++;
                assert(it->second.rrip < 8);
            }
        }
    }
    
    if (pf_l1_addr != 0) {
        prefetch_line(pf_l1_addr, FILL_L1, pf_metadata);
    }
    if (pf_l2_addr != 0) {
        prefetch_line(pf_l2_addr, FILL_L2, 0);
    }
    if (pf_l3_addr != 0) {
        prefetch_line(pf_l3_addr, FILL_LLC, 0);
    }
    if (pf_type == 1) {
        stream_pf_num++;
    }
    else if (pf_type == 2) {
        stride_pf_num++;
    }
    else if (pf_type == 3) {
        pattern_pf_num++;
    }

    return pf_metadata;
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    return metadata_in;
}

void CACHE::prefetcher_final_stats()
{
    cout << "Stream Num: " << stream_pf_num << endl;
    cout << "Stride Num: " << stride_pf_num << endl;
    cout << "Pattern Num: " << pattern_pf_num << endl;
}
