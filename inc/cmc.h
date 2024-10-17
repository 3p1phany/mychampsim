
#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>
#include <deque>
#include <vector>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>

using namespace std;

#define DYNAMIC_CMC

#define CMC_LINE_SIZE 14
#define METADATA_INTERVAL 30000000
#define MAX_ASSOC 7

#define nENABLE_CMC
#define CMC_INFO
#define nCMC_DEBUG
#define nMETADATA_DEBUG

#define nENABLE_JPP

#define nCMC_RECORD_ALL
#define nCMC_RESPECTIVE_ADDR
#define nCMC_PC_LOCALIZATION
#define nCMC_DIRECT_INDEX
#define nCMC_PREF_ONLY_DIRECT

struct CMCConfig {
    uint32_t cpu;

    // LoadRet
    uint64_t load_ret_size;

    // LoadIdentity
    uint64_t load_identity_size;

    // Metadata
    uint64_t assoc; // Numbers of Cache ways that are used to store metadata
    uint64_t num_sets; // Number of Cache Sets 
    uint64_t line_size; // Number of entries in each Cache line
    uint64_t readq_size;
    uint64_t writeq_size;
    uint64_t prefq_size;
    uint64_t metadata_delay;

    // int lookahead;
    // int degree;
    // int training_unit_size;
    bool use_dynamic_assoc;
    // TriageReplType repl;
};

#define LoadReturn_SIZE 32

struct LoadRetEntry {
    uint64_t ret_val;
    uint64_t ip;
    bool is_real_producer = false;
    bool is_producer = false;
    bool is_consumer;
    uint8_t size;

    /** Debug */
    uint64_t instr_id; 
    uint64_t addr;
    uint64_t enq_cycle;
};

class LoadRet {
    private:
        uint32_t cpu;
        deque<LoadRetEntry> entries;
        uint64_t SIZE;
        
    public:
        void init(CMCConfig *config);
        void insert(uint64_t ip, uint64_t ret_val, bool is_consumer, uint64_t instr_id, uint64_t addr, uint8_t size, uint64_t cycle);
        uint64_t lookup(uint64_t ip, uint64_t base_addr, bool is_recursive);        
        bool is_full();
        void print();
};

struct LoadIdentityEntry
{
    uint64_t producer_ip; // should be reaplaced with index
    uint8_t producer_conf = 1;

    uint64_t offset;
    uint64_t lru;
    uint8_t rrip = 0;
    uint8_t recursive_conf = 0;
    uint8_t data_conf = 0;
    uint64_t rec_id;

    uint8_t size; // For debug. Because rec load & p-link are definately both double word load.

    bool is_producer = false;
    bool is_offset = false;

    bool need_handle(){
        return data_conf >= 2; // TODO:
    }
};

#define LoadIdentity_SIZE 16
class LoadIdentity {
    private:
        uint32_t cpu;

        const uint8_t max_conf = 3;
        const uint8_t threshold = 2;
        const uint8_t rec_conf_init = 1;
        string repl = "LRU";
        uint8_t rrip_max = 7;
        uint64_t cur_rec_id = 0;

        vector<uint64_t> history_buffer= {0};
        uint64_t METADATA_INSERT_NUM = 0;
        uint64_t METADATA_CYCLE_NUM = 0;
        uint64_t SET_ID_NOT_FOUND_CNT = 0;

    public:
        uint64_t SIZE;
        unordered_map<uint64_t, LoadIdentityEntry> entries;

        void init(CMCConfig *config);
        bool is_full();
        void set_info(uint64_t ip, uint64_t producer_ip, uint64_t offset, uint8_t size);
        void set_identity(uint64_t ip, bool is_producer, bool is_real_producer);
        void insert(uint64_t ip, LoadIdentityEntry& item);
        void update_repl(uint64_t ip);
        void predict(uint64_t ip, uint64_t addr, bool cache_miss, uint64_t cycle);
        uint64_t get_offset(uint64_t ip);
        pair<uint64_t, uint64_t> get_offset_array(uint64_t ip, uint64_t base_addr, uint64_t rec_addr, uint64_t* offsets,
                                                  uint64_t* dlinks, uint64_t* ips);
        bool has_successor(uint64_t ip);

        void print_final_stats();
    
        bool contain(uint64_t ip) {return entries.find(ip) != entries.end();};

        bool is_recursive(uint64_t ip) {
            if(entries.find(ip) == entries.end()) {return false;}
            else {return entries[ip].recursive_conf >= 2;}
        };

        bool is_data(uint64_t ip) {
            if(entries.find(ip) == entries.end()) {return false;}
            else {return entries[ip].data_conf >= threshold;}
        };

        bool is_link_data(uint64_t ip) {
            if(entries.find(ip) == entries.end()) {return false;}
            else {
                bool conf_true = entries[ip].data_conf >= 2;
                uint64_t rec_id = entries[ip].rec_id;

                // Find if there is a recursive load that has the same rec_id
                for (auto it = entries.begin(); it != entries.end(); it++) {
                    if (it->second.rec_id == rec_id && it->first != ip && is_recursive(it->first)) {
                        return conf_true;
                    }
                }
            }

            return false;
        };

        void print_entries(){
            cout << "[LoadIdentity Entries] :" << endl;
            for(auto it = entries.begin(); it != entries.end(); it++){
                print(it->first);
            }
        }

        void print(uint64_t ip) {
            assert(contain(ip));
            cout << "ip: " << hex << ip << dec
                 << ", rec_conf: " << (int)entries[ip].recursive_conf
                 << ", data_conf: " << (int)entries[ip].data_conf 
                 << ", rec_id: " << entries[ip].rec_id 
                 << ", is_offset: " << entries[ip].is_offset 
                 << ", is_producer: " << entries[ip].is_producer
                 << ", offset: " << entries[ip].offset
                 << ", producer_ip: " << hex << entries[ip].producer_ip << dec
                 << ", lru: " << (int)entries[ip].lru 
                 << endl;
        }
};


struct CMC_AGQ_ITEM{
    // bool valid = true;
    bool issued = false;
    uint64_t ret_value;
    uint64_t ip;

    // DEBUG
    uint64_t size;
    uint64_t cycle;
    uint64_t enq_cycle;

    void print(){
        cout << "ip: " << hex << ip << dec
             << ", ret_value: " << hex << ret_value << dec
             << ", issued: " << +issued
             << ", cycle: " << cycle
             << ", enq_cycle: " << enq_cycle
             << endl;
    }
};

#define CMC_AGQ_SIZE 0
class CMC_AGQ {
public:
    vector<CMC_AGQ_ITEM> entries;
    uint32_t SIZE = CMC_AGQ_SIZE;
    uint32_t cpu;
    bool pop_when_full = false;

    uint64_t CMC_AGQ_INSERT_CNT = 0;
    uint64_t CMC_AGQ_FULL_CNT = 0;
    uint64_t CMC_AGQ_BEYOND_NUM = 0;
    uint64_t CMC_AGQ_IP_EXPIRED = 0;

    void init(CMCConfig *config);
    bool insert(CMC_AGQ_ITEM& item);
    void load_return(uint64_t addr);
    vector<CMC_AGQ_ITEM>::iterator search_addr(uint64_t addr);
    bool update_src(vector<CMC_AGQ_ITEM>::iterator ret_item, uint64_t ret_val);
    vector<CMC_AGQ_ITEM>::iterator first_ready_item();
    void print_final_stats();
    void check_cycle(vector<CMC_AGQ_ITEM>::iterator ret_item);
    void remove_expired();

    void clear_agq();
#ifdef PREFETCH_DEBUG
    string name;
    uint64_t full_cycles = 0;
    void check_state();
    void print_buffer();
#endif
};

struct MetaDataEntry{
    uint64_t next_addr;
    uint8_t conf;
};

struct LUTEntery{
    uint64_t tag;
};

struct read_entry{
    uint64_t ip;
    uint64_t addr;

    // DEBUG
    uint64_t cycle; // cycle that the demand access is handled
};

struct write_entry{
    MetaDataEntry* entry;
    bool first_write;
    uint64_t index;
    uint64_t way;
    uint64_t addr2;
    uint64_t conf;

    // DEBUG
    uint64_t ip;
    uint64_t addr1;
    uint64_t cycle;
};

struct prefetch_entry{
    uint64_t ip;
    uint64_t addr;

    // DEBUG
    uint64_t cycle; // cycle that the demand access is handled
};

class MetaData_OnChip {
    private:
        uint32_t cpu;
        std::vector<std::map<uint64_t, MetaDataEntry>> entries;

        uint64_t access_cnt;
        std::set<uint64_t>cmc_unique_addr;

        bool use_dynamic_assoc;
        uint64_t assoc; // Numbers of Cache ways that are used to store metadata
        uint64_t num_sets; // Number of Cache Sets 
        //uint64_t line_size; // Number of metadata entries in each Cache line

        uint64_t index_mask;
        uint64_t index_length;
        
        uint64_t WQ_SIZE;
        uint64_t RQ_SIZE;
        uint64_t PQ_SIZE;
        uint64_t metadata_delay;

        std::unordered_set<uint64_t> trigger_addr_recursive;
        std::unordered_set<uint64_t> unique_addr0;
        std::unordered_set<uint64_t> unique_addr1;
        uint64_t ACCESS_CNT = 0, RQ_FULL_CNT = 0, WQ_FULL_CNT = 0, PQ_FULL_CNT = 0;
        uint64_t trigger_count, total_assoc, new_addr, prefetch_count;
        uint64_t metadata_rq_add, metadata_rq_issue, metadata_rq_hit;
        uint64_t metadata_wq_add, metadata_wq_issue;
        uint64_t metadata_update = 0, metadata_inc_conf = 0, metadata_dec_conf = 0, metadata_dec_conf_update = 0;

        uint64_t ip_metadata_update = 0, ip_metadata_inc_conf = 0, ip_metadata_dec_conf = 0, ip_metadata_dec_conf_update = 0;

    public:
        std::map<uint64_t, LUTEntery> lut_entries;
        deque<write_entry> writeQ;
        deque<read_entry> readQ;
        deque<prefetch_entry> prefQ;

    public:
        void init(CMCConfig *config);
        bool RQ_is_full();
        bool WQ_is_full();
        bool PQ_is_full();
        void add_rq(uint64_t ip, uint64_t addr, uint64_t cycle);
        void add_pq(uint64_t ip, uint64_t addr, uint64_t cycle);
        void add_wq(MetaDataEntry* entry, bool first_write, uint64_t index, uint64_t way,uint64_t addr2, uint64_t conf, uint64_t ip, uint64_t addr1, uint64_t cycle);
        void read(uint64_t num, uint64_t curent_cycle);
        void write(uint64_t num, uint64_t curent_cycle);
        prefetch_entry pref();
        void insert(uint64_t ip, uint64_t addr1, uint64_t addr2, uint64_t cycle);
        void access(uint64_t ip, uint64_t addr, uint64_t cycle);
        uint64_t get_assoc();
        uint64_t get_set_id(uint64_t addr);
        uint64_t get_tag(uint64_t addr);
        void print_stats();
        void check_state();
};

typedef struct LoadCounter{
    uint64_t total_num;
    uint64_t rec_num;
} LoadCounter_t;