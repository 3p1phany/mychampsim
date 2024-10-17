/**
 * @file
 * Describes a history prefetcher.
 */

#ifndef __MEM_CACHE_PREFETCH_TRIANGEL_HH__
#define __MEM_CACHE_PREFETCH_TRIANGEL_HH__

#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <unordered_set>

#include "ooo_cpu.h"
#include "cache.h"
#include "prefetch.h"
#include "memory_data.h"

#include "sat_counter.hh"
#include "associative_set.hh"
#include "replaceable_entry.hh"
#include "set_associative.hh"
#include "lru_rp.hh"
#include "weighted_lru_rp.hh"
#include "fifo_rp.hh"
#include "brrip_rp.hh"

#include "bloom.h"

#ifdef TRIANGEL_DEBUG
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...) (void)0
#endif

class AddrPriority
{
  public:
    AddrPriority(uint64_t addr, int priority)
      : addr(addr), priority(priority)
    {
    }

    uint64_t addr;
    int priority;
};
class PrefetchQueueEntry {
    public:
    PrefetchQueueEntry(uint64_t addr, uint64_t delay)
      : addr(addr), delay(delay)
    {
    }
    uint64_t addr;
    uint64_t delay;
};

/**
 * Override the default set associative to apply a specific hash function
 * when extracting a set.
 */
class TriangelHashedSetAssociative : public SetAssociative
{
  public:
    uint32_t extractSet(const uint64_t addr) const override;
    uint64_t extractTag(const uint64_t addr) const override;

  public:
    int ways;
    int max_ways;
    TriangelHashedSetAssociative()
      : SetAssociative(1,16,524288), ways(0),max_ways(8)
    {
    }
    ~TriangelHashedSetAssociative() = default;
};



class Triangel
{
    uint32_t cpu = 123;
    static std::mt19937 gen;
    bool randomChance(int r, int s);
    /** Number of maximum prefetches requests created when predicting */
    unsigned degree;

    /**
     * Training Unit Entry datatype, it holds the last accessed address and
     * its secure flag
     */

    static CACHE* scache;
    unsigned cacheDelay;
    bool should_lookahead;
    bool should_rearrange;
    
    bool use_scs;
    bool use_bloom;
    bool use_reuse;
    bool use_pattern;
    bool use_pattern2;
    bool use_mrb;
    bool perfbias;
    bool smallduel;
    bool timed_scs;
    bool useSampleConfidence;

    int max_size;
    int size_increment;
    static int64_t global_timestamp;
    uint64_t second_chance_timestamp;
    static int current_size;
    static int target_size;
    int maxWays;    
    
    static bloom* blptr;

    bloom bl;
    int bloomset=-1;
    
    std::vector<int> way_idx;
    
    SatCounter8  globalReuseConfidence;
    SatCounter8  globalPatternConfidence;
    SatCounter8 globalHighPatternConfidence;    
   
    struct TrainingUnitEntry : public TaggedEntry
    {
        uint64_t lastAddress;
        uint64_t lastLastAddress;
        int64_t local_timestamp;
        SatCounter8 reuseConfidence;
        SatCounter8 patternConfidence;
        SatCounter8 highPatternConfidence;
        SatCounter8 replaceRate;
        SatCounter8 hawkConfidence;
        bool currently_twodist_pf;

        TrainingUnitEntry() : lastAddress(0), lastLastAddress(0), local_timestamp(0),reuseConfidence(4,8), patternConfidence(4,8), highPatternConfidence(4,8), replaceRate(4,8), hawkConfidence(4,8), currently_twodist_pf(false)
        {}

        void
        invalidate() override
        {
        	TaggedEntry::invalidate();
            lastAddress = 0;
            lastLastAddress = 0;
            //local_timestamp=0; //Don't reset this, to handle replacement and still give contiguity of timestamp
            reuseConfidence.reset();
            patternConfidence.reset();
            highPatternConfidence.reset();
            replaceRate.reset();
            currently_twodist_pf = false;
                
        }
    };
    /** Map of PCs to Training unit entries */
    SetAssociative index_for_training_unit;
    LRU replacement_for_training_unit;
    AssociativeSet<TrainingUnitEntry> trainingUnit;
    
    uint64_t lookupTable[1024];
    uint64_t lookupTick[1024];
    int lookupAssoc;
    int lookupOffset;
    
    static std::vector<uint32_t> setPrefetch; 
 
    public:
    struct SizeDuel
    {
        int idx;
        uint64_t set;
        uint64_t setMask;
        uint64_t temporalMod; //[0..12] Entries Per Line
        
        uint64_t temporalModMax; //12 by default
        uint64_t cacheMaxAssoc;
        
        
        std::vector<uint64_t> cacheAddrs; // [0..16] should be set by the nsets of the L3 cache.
        std::vector<uint64_t> cacheAddrTick;
        std::vector<uint64_t> temporalAddrs;
        std::vector<uint64_t> temporalAddrTick;
        std::vector<bool> inserted;

        SizeDuel()
        {
        }

        void reset(uint64_t mask, uint64_t modMax, uint64_t cacheAssoc) {
        	setMask=mask;
        	temporalModMax = modMax;
        	cacheMaxAssoc=cacheAssoc;
        	cacheAddrTick.resize(cacheMaxAssoc);
        	temporalAddrs.resize(cacheMaxAssoc);
        	cacheAddrs.resize(cacheMaxAssoc);
        	temporalAddrTick.resize(cacheMaxAssoc);
        	inserted.resize(cacheMaxAssoc,false);
        	for(int x=0;x<cacheMaxAssoc;x++) {
                cacheAddrTick[x]=0;
                temporalAddrs[x]=0;
                cacheAddrs[x]=0;
                temporalAddrTick[x]=0;
            }
            set = std::uniform_int_distribution<uint64_t>(0,setMask)(gen);
            temporalMod = std::uniform_int_distribution<uint64_t>(0,modMax-1)(gen); // N-1, as range is inclusive.	
        }
  	
        int checkAndInsert(uint64_t addr, bool should_pf) {
            int ret = 0;
            bool foundInCache=false;
            bool foundInTemp=false;
            if((addr & setMask) != set) return ret;
            for(int x=0;x<cacheMaxAssoc;x++) {
                if(addr == cacheAddrs[x]) {
                    foundInCache=true; 
                    int index=cacheMaxAssoc-1;
                    for(int y=0;y<cacheMaxAssoc;y++) {
                        if(cacheAddrTick[x]>cacheAddrTick[y]) index--;
                        assert(index>=0);
                    }  
                    cacheAddrTick[x] = scache->current_cycle;
                    ret += index+1;	
                }
                if(should_pf && addr == temporalAddrs[x]) {
                    foundInTemp=true;
                    int index=cacheMaxAssoc-1;
                    for(int y=0;y<cacheMaxAssoc;y++) {
                        if(temporalAddrTick[x]>temporalAddrTick[y]) index--;
                        assert(index>=0);
                    }  
                    ret += 128*(index+1);
                    temporalAddrTick[x] = scache->current_cycle;
                    inserted[x]=true;
                }
            }
            if(!foundInCache) {
                uint64_t oldestTick = (uint64_t)-1;
                int idx = -1;
                for(int x=0; x<cacheMaxAssoc;x++) {
                    if(cacheAddrTick[x]<oldestTick) {idx = x; oldestTick = cacheAddrTick[x];}
                }
                assert(idx>=0);
                cacheAddrs[idx]=addr;
                cacheAddrTick[idx]=scache->current_cycle;
            }
            if(!foundInTemp && should_pf && (((addr / (setMask+1)) % temporalModMax) == temporalMod)) {
                uint64_t oldestTick = (uint64_t)-1;
                int idx = -1;
                for(int x=0; x<cacheMaxAssoc;x++) {
                    if(temporalAddrTick[x]<oldestTick) {idx = x; oldestTick = temporalAddrTick[x]; }
                }  			
                assert(idx>=0);
                temporalAddrs[idx]=addr;
                temporalAddrTick[idx]=scache->current_cycle;
            }  
            return ret;		
        }
    };
    SizeDuel sizeDuels[256];
    static SizeDuel* sizeDuelPtr;

    struct Hawkeye
    {
        int iteration;
        uint64_t set;
        uint64_t setMask; // address_map_rounded_entries/ maxElems - 1
        uint64_t logaddrs[64];
        uint64_t logpcs[64];
        int logsize[64];
        int maxElems = 8;
      
        Hawkeye(uint64_t mask, bool history) : iteration(0), set(0), setMask(mask)
        {
           reset();
        }
        
        Hawkeye() : iteration(0), set(0)
        {       }
      
        void reset() {
            iteration=0;
            for(int x=0;x<64;x++) {
                logsize[x]=0;
                logaddrs[x]=0;
                logpcs[x]=0;
            }
            set = std::uniform_int_distribution<uint64_t>(0,setMask-1)(gen);
        }
      
        void decrementOnLRU(uint64_t addr,AssociativeSet<TrainingUnitEntry>* trainer) {
            if((addr % setMask) != set) return;
            for(int y=iteration;y!=((iteration+1)&63);y=(y-1)&63) {
                if(addr==logaddrs[y]) {
                    uint64_t pc = logpcs[y];
                    TrainingUnitEntry *entry = trainer->findEntry(pc); //TODO: is secure
                    if(entry!=nullptr) {
                        if(entry->hawkConfidence>=8) {
                            entry->hawkConfidence--;
                            //debug_printf("%s evicted, pc %s, temporality %d\n",addr, pc,entry->temporal);
                        }
                        
                    }
                    return;
                }
            }            
        }
      
      void add(uint64_t addr,  uint64_t pc,AssociativeSet<TrainingUnitEntry>* trainer) {
        if((addr % setMask) != set) return;
        logaddrs[iteration]=addr;
        logpcs[iteration]=pc;
        logsize[iteration]=0;

        TrainingUnitEntry *entry = trainer->findEntry(pc); //TODO: is secure
        if(entry!=nullptr) {
            for(int y=(iteration-1)&63;y!=iteration;y=(y-1)&63) {
                if(logsize[y] == maxElems) {
                    //no match
                    //debug_printf("%s above max elems, pc %s, temporality %d\n",addr, pc,entry->temporal-1);
                    entry->hawkConfidence--;
                    break;
                }
                if(addr==logaddrs[y]) {
                    //found a match
                    //debug_printf("%s fits, pc %s, temporality %d\n",addr, pc,entry->temporal+1);
                    entry->hawkConfidence++;
                    for(int z=y;z!=iteration;z=(z+1)&63){
                        logsize[z]++;
                    }
                    break;
                }
            }            
        }
        iteration++;
        iteration = iteration % 64;
      }
    };

    Hawkeye hawksets[64];
    bool useHawkeye;

    /** Address Mapping entry, holds an address and a confidence counter */
    struct MarkovMapping : public TaggedEntry
    {
      	uint64_t index; //Just for maintaining HawkEye easily. Not real.
        uint64_t address;
        int lookupIndex; //Only one of lookupIndex/Address are real.
        bool confident;
        uint64_t cycle_issued; // only for prefetched cache and only in simulation
        MarkovMapping() : index(0), address(0), confident(false), cycle_issued(0)
        {}

        void
        invalidate() override
        {
            TaggedEntry::invalidate();
            address = 0;
            index = 0;
            confident = false;
            cycle_issued=uint64_t(0);
        }
    };
    

    /** Sample unit entry, tagged by data address, stores PC, timestamp, next element **/
    struct SampleEntry : public TaggedEntry
    {
    	TrainingUnitEntry* entry;
    	bool reused;
    	uint64_t local_timestamp;
    	uint64_t next;
    	bool confident;

    	SampleEntry() : entry(nullptr), reused(false), local_timestamp(0), next(0)
        {}

        void
        invalidate() override
        {
            TaggedEntry::invalidate();
        }

        void clear() {
            entry=nullptr;
            reused = false;
            local_timestamp=0;
            next = 0;
            confident=false;
        }
    };
    SetAssociative index_for_history_sampler;
    LRU replacement_for_history_sampler;
    AssociativeSet<SampleEntry> historySampler;

    /** Test pf entry, tagged by data address**/
    struct SecondChanceEntry: public TaggedEntry
    {
    	uint64_t pc;
    	uint64_t global_timestamp;
    	bool used;
    };
    SetAssociative index_for_second_chance;
    FIFO replacement_for_second_chance;
    AssociativeSet<SecondChanceEntry> secondChanceUnit;

    /** History mappings table */
    TriangelHashedSetAssociative index_for_markov_table;
    BRRIP replacement_for_markov_table;
    AssociativeSet<MarkovMapping> markovTable;
    static AssociativeSet<MarkovMapping>* markovTablePtr;
    

    SetAssociative index_for_metadata_reuse_buffer;
    FIFO replacement_for_metadata_reuse_buffer;
    AssociativeSet<MarkovMapping> metadataReuseBuffer;
    bool lastAccessFromPFCache;

    MarkovMapping* getHistoryEntry(uint64_t index, bool replace, bool readonly, bool clearing, bool hawk);

    public:
        Triangel(uint32_t cpu);
        ~Triangel() = default;

    void calculatePrefetch(bool load, uint64_t pc, uint64_t addr,
                           std::vector<AddrPriority> &addresses);
                               
    uint64_t metadataAccesses;
    uint64_t lookupCorrect;
    uint64_t lookupWrong;
    uint64_t lookupCancelled;

    unsigned get_degree() const { return degree; }
    uint32_t get_cpu_id() const { return cpu; }

    uint64_t pq_size;
    std::deque<PrefetchQueueEntry> PrefetchQueue;

    uint64_t metadata_access = 0;
    uint64_t metadata_new_write = 0;
    uint64_t metadata_update_same = 0;
    uint64_t metadata_update_diff = 0;
    uint64_t total_assoc = 0;
    uint64_t total_cycle = 0;
    uint64_t conf_dec_recursive = 0, conf_dec_data = 0, conf_dec_other = 0;
    uint64_t metadata_rq_issue = 0, metadata_wq_issue = 0;

    std::unordered_set<uint64_t>trigger_addr_recursive;
    std::unordered_set<uint64_t>trigger_addr_data;
    std::unordered_set<uint64_t>trigger_addr_other;
    std::unordered_set<uint64_t>trigger_addr;
};


#endif // __MEM_CACHE_PREFETCH_TRIANGEL_HH__
