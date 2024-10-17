#include <stdio.h>
#include "cache.h"
#include <map>
#include <set>
#include <cassert>
#include <set>
// #include "bo_percore.h" 
#define DEGREE PREFETCH_DISTANCE

unsigned int total_access;
unsigned int predictions;
unsigned int no_prediction;
uint64_t addr_context[2];
uint64_t pointer;

//#define HYBRID

struct Domino_read_entry{
    uint64_t addr;

    // DEBUG
    uint64_t cycle; // cycle that the demand access is handled
};

deque<Domino_read_entry> EIT_ReadQ;
deque<Domino_read_entry> HT_ReadQ;

bool domino_stride_enable = true;
extern IPT_L1 ipt[NUM_CPUS][IPT_NUM];

static const unsigned long long crc = 3988292384ULL;
static const unsigned long long crc_eit_ht_dist = 0x12301abcdef;

struct PointBuf
{
    uint64_t GHB_idx=0;
    uint8_t remaining_num=0;
};
PointBuf point_buf;

uint64_t Counter = 32;

struct EIT_Entry
{
    map<uint64_t, uint64_t> address_pointer_pair;
    map<uint64_t, uint64_t> access_time;
    uint64_t timer;
    uint64_t most_recent_addr;

    EIT_Entry()
    {
        timer = 0;  
        most_recent_addr = 0;
        address_pointer_pair.clear();
        access_time.clear();
    }

    uint64_t get_ghb_pointer(uint64_t curr_addr)
    {
        if(address_pointer_pair.find(curr_addr) != address_pointer_pair.end())
            return address_pointer_pair[curr_addr];
    
        assert(address_pointer_pair.find(most_recent_addr) != address_pointer_pair.end());
        return address_pointer_pair[most_recent_addr];
    }

    void remove_oldest()
    {
        uint64_t oldest = timer+1;
        uint64_t replace_addr;
        for(map<uint64_t, uint64_t>::iterator it=access_time.begin(); it != access_time.end(); it++)
        {
            if(it->second < oldest)
            {
                oldest = it->second;
                replace_addr = it->first;
            }
        }
        assert(oldest < (timer+1));
        assert(address_pointer_pair.find(replace_addr) != address_pointer_pair.end());
        address_pointer_pair.erase(replace_addr);
        access_time.erase(replace_addr);
    }

    void update(uint64_t curr_addr, uint64_t pointer)
    {
        timer++;
#ifdef EIT_ENTRY_LIMIT
        if(address_pointer_pair.find(curr_addr) == address_pointer_pair.end())
            if(address_pointer_pair.size() >= 3)
                remove_oldest();
       
        assert(address_pointer_pair.size() <= 3); 
        assert(access_time.size() <= 3); 
#endif
        address_pointer_pair[curr_addr] = pointer;
        access_time[curr_addr] = timer;
        most_recent_addr = curr_addr;
    }
};

struct Domino_prefetcher_t
{
    vector<uint64_t> GHB;
    map<uint64_t, EIT_Entry> index_table;
    uint64_t last_address;

    void domino_train(uint64_t curr_addr, uint64_t last_addr)
    {
        GHB.push_back(curr_addr);
        assert(GHB.size() >= 1);

        index_table[last_addr].update(curr_addr, (GHB.size() - 1));
    }

    vector<uint64_t> domino_predict(uint64_t curr_addr, uint64_t last_addr)
    {
        vector<uint64_t> candidates;
        candidates.clear();

        if(index_table.find(last_addr) != index_table.end())
        {
            uint64_t index = index_table[last_addr].get_ghb_pointer(curr_addr);

            for(unsigned int i=1; i<=32; i++)
            {
                if((index+i) >= GHB.size())
                    break;
                uint64_t candidate_phy_addr = GHB[index+i];
                candidates.push_back(candidate_phy_addr);
            }
            
        }
        else
            no_prediction++;

        return candidates;
    }

    public :
    Domino_prefetcher_t()
    {
        last_address = 0;
        GHB.clear();
        index_table.clear();
    }
};

Domino_prefetcher_t domino[NUM_CPUS];


void CACHE::prefetcher_initialize()
{
    cout << NAME << " [Domino] prefetcher, ";

#if TEMPORAL_L1D == false
    domino_stride_enable = false;
    std::cout << "Stride Prefetcher Off" << std::endl;
#else
    cout << "Stride Prefetcher On" << endl;
#endif

    total_access = 0;
    predictions = 0;
    no_prediction = 0;
#ifdef HYBRID
    bo_l2c_prefetcher_initialize();
#endif

    if(domino_stride_enable){
        for(uint32_t i = 0; i < IPT_NUM; i++){
            ipt[cpu][i].conf = 0;
            ipt[cpu][i].rplc_bits = i;
        }
    }
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t pc, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in)
{
    /** Stride Prefetcher */
    if(domino_stride_enable){
        pair<uint64_t, uint64_t> stride = stride_cache_operate(cpu, addr, pc);
        if(stride.first != 0){
            int stride_succ = prefetch_line(stride.first, true, stride.second);
        }
    }

    if (type != LOAD)
        return metadata_in;

//    if(cache_hit)
//        return metadata_in;

    uint64_t addr_B = (addr >> 6) << 6;

    if(addr_B == domino[cpu].last_address)
        return metadata_in;

    total_access++;

#ifdef HYBRID
    uint64_t bo_trigger_addr = 0;
    uint64_t bo_target_offset = 0;
    uint64_t bo_target_addr = 0;
    bo_l2c_prefetcher_operate(addr, pc, cache_hit, type, this, &bo_trigger_addr, &bo_target_offset, 0);

    if (bo_trigger_addr && bo_target_offset) {

        for(unsigned int i=1; i<=DEGREE; i++) {
            bo_target_addr = bo_trigger_addr + (i*bo_target_offset); 
            bo_issue_prefetcher(this, pc, bo_trigger_addr, bo_target_addr, FILL_LLC);
        }
    }
#endif

    //Predict before training
    vector<uint64_t> candidates = domino[cpu].domino_predict(addr_B, domino[cpu].last_address);

    bool pref_hit = (cache_hit != 0) || hit_pref;
    if(!pref_hit){
        Counter = 0;
        /** TODO: Open this get_metadata will dramatically decrease performance.
         *        But we should open this before estimate the impact of memory traffic.
        */
        // get_metadata(addr^crc);
    }

    unsigned int num_prefetched = 0;
    for(unsigned int i=0; i<candidates.size() && Counter > 0; i++)
    {
        int ret = prefetch_line(pc, addr, candidates[i], true, 0);
        if (ret == 1)
        {
            predictions++;
            num_prefetched++;
            Counter--;
        }

        if(num_prefetched >= DEGREE)
            break;
    }

    if (Counter == 0 && HT_ReadQ.empty() && !candidates.empty()) {
        int result = get_metadata((candidates[0]^crc) - crc_eit_ht_dist);
        if(result != -2){
            Domino_read_entry new_entry;
            new_entry.addr = (candidates[0]^crc) - crc_eit_ht_dist;
            new_entry.cycle = current_cycle;
            HT_ReadQ.push_back(new_entry);
        }
    }

    domino[cpu].domino_train(addr_B, domino[cpu].last_address);
    
    domino[cpu].last_address = addr_B;

    return metadata_in;
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
#ifdef HYBRID
    bo_l2c_prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr, this, 0);
#endif
    return metadata_in;
}

void CACHE::complete_metadata_req(uint64_t meta_data_addr)
{
    if(HT_ReadQ.size() > 0){
        if(meta_data_addr == HT_ReadQ[0].addr){
            assert(HT_ReadQ.size() == 1);
            Counter = 32;
            HT_ReadQ.pop_back();
        }
    }
}

void CACHE::prefetcher_cycle_operate() {
    assert(HT_ReadQ.size() <= 1);
    if(HT_ReadQ.size() != 0 && (current_cycle - HT_ReadQ[0].cycle) >= 5000){
        HT_ReadQ.pop_back();
    }
}

void CACHE::prefetcher_final_stats() 
{
#ifdef HYBRID
	bo_l2c_prefetcher_final_stats();
#endif
  printf("Prefetcher final stats\n");
    cout << "Index Table Size: " << domino[cpu].index_table.size() << endl;
    cout << "GHB size: " << domino[cpu].GHB.size() << endl;;
    cout << endl << endl;
    cout << "Triggers: " << total_access << endl;
    cout << "No Prediction: " << no_prediction << " " << 100*(double)no_prediction/(double)total_access << endl;
    cout << "Predictions: " << predictions << " " << 100*(double)predictions/(double)total_access << endl;
    cout << endl << endl;
}

