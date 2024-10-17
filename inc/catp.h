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
#include <random>
#include <algorithm>

using namespace std;

#define ENABLE_CATP
#define CATP_MAX_ASSOC 16
#define CATP_METADATA_INTERVAL 30000000

struct CATPConfig {
    uint32_t cpu;

    uint32_t criticality_mode; // 0 for disable; 1 for use cycle and mshr occurpancy; 2 for instruction issue count;
    bool pattern_train_enable;
    bool use_dynamic_threshold;
    bool use_dynamic_pattern;
    bool use_dynamic_assoc;

    uint32_t critical_conf_threshold;
    uint32_t critical_conf_max;
    uint32_t criticality_threshold;
    uint64_t assoc;     

    uint32_t training_table_size;
    uint32_t missing_status_size;
    uint32_t missing_status_ip_size;
    uint32_t set_dueller_size;

    uint64_t cache_sets;
    uint64_t cache_ways;
    uint64_t meta_sub_ways;
    uint64_t num_sets;  // TODO

    uint32_t lookahead;
    uint64_t readq_size;
    uint64_t writeq_size;
    uint64_t prefq_size;
    uint64_t metadata_delay;
};

class CATP {
    private:
        uint32_t cpu;

        uint32_t criticality_mode;
        bool pattern_train_enable;
        bool use_dynamic_threshold;
        bool use_dynamic_pattern;
        bool use_dynamic_assoc;

        uint32_t critical_conf_threshold;
        uint32_t critical_conf_max;
        uint32_t criticality_threshold;

        uint32_t training_table_size;
        uint32_t missing_status_size;
        uint32_t missing_status_ip_size;
        uint32_t set_dueller_size;

        uint64_t cache_sets;
        uint64_t cache_ways;
        uint64_t meta_sub_ways;

    struct MissingStatusEntry {
        uint64_t addr;
        vector<uint64_t> ip;
        uint64_t criticality;
    };
    vector<MissingStatusEntry> missing_status;

    struct TrainingEntry {
        uint64_t ip;
        uint64_t last_addr;
        uint64_t occur_cnt;     // for simulation
        uint64_t critical_sum;  // for simulation
        uint64_t critical_conf;
        uint64_t addr1;
        uint64_t addr2;
        uint64_t addr_cnt;
        uint64_t wait_cnt;
        uint64_t reuse_distance;
        uint64_t pattern_conf;
        uint64_t replace_score;
    };
    vector<TrainingEntry> training_table;
    bool pattern_train_dynamic_enable;

    struct SetDuellerEntry {
        uint64_t sample_index;
        uint64_t sample_sub_index;
        vector<uint64_t> cache_addrs;
        vector<uint64_t> cache_scores;
        vector<uint64_t> meta_addrs;
        vector<uint64_t> meta_scores;
    };
    vector<SetDuellerEntry> set_dueller;
    
    deque<uint64_t> prefetch_addrs;
    uint64_t FILTER_SIZE;

    uint32_t target_assoc;
    vector<uint64_t> meta_cnt;
    vector<uint64_t> cache_cnt;
    std::vector<int> random_set;
    std::mt19937 gen;

    uint64_t cache_access_cnt = 0;
    uint64_t total_threshold = 0;
    uint64_t total_pattern_enable = 0;

    public:
    void init(CATPConfig* config);
    void catp_cache_miss(uint64_t ip, uint64_t addr);
    void catp_cache_fill(uint64_t addr, uint64_t current_cycle, uint8_t prefetch);
    void catp_cycle_op();
    void catp_issue_op();
    void training_table_update_on_refill(uint64_t ip, uint64_t addr, uint64_t criticality, uint64_t current_cycle);
    void training_table_update_on_access(uint64_t ip, uint64_t addr, bool cache_hit, uint64_t current_cycle, uint64_t current_inst);
    void print_stats();
    void set_dueller_check(uint64_t addr, bool meta, bool meta_write, uint64_t current_cycle);
    void set_dueller_adjust(uint64_t inst_num);
    uint32_t get_target_assoc();
    bool recent_prefetch(uint64_t addr);
};

struct CATPMetaDataEntry{
    uint64_t next_addr;
    uint8_t conf;
};

struct catp_read_entry{
    uint64_t ip;
    uint64_t addr;
    uint64_t cycle;
    uint64_t depth;
};

struct catp_write_entry{
    CATPMetaDataEntry* entry;
    bool first_write;
    uint64_t index;
    uint64_t way;
    uint64_t addr2;
    uint64_t conf;
    uint64_t cycle;
};

struct catp_prefetch_entry{
    uint64_t ip;
    uint64_t addr;
    uint64_t cycle;
};

class CATP_MetaData_OnChip {
    private:
        uint32_t cpu;
        std::vector<std::map<uint64_t, CATPMetaDataEntry>> entries;

        uint32_t assoc;    // Numbers of Cache ways that are used to store metadata
        uint64_t num_sets; // Number of Cache Sets 

        uint64_t index_mask;
        uint64_t index_length;
        
        uint64_t WQ_SIZE;
        uint64_t RQ_SIZE;
        uint64_t PQ_SIZE;
        uint64_t metadata_delay;
        
        uint32_t lookahead;

    public:
        // statistics
        uint64_t RQ_FULL_CNT = 0, WQ_FULL_CNT = 0, PQ_FULL_CNT = 0; 
        std::unordered_set<uint64_t> trigger_addr;
        uint64_t ACCESS_CNT = 0, ASSOC_SUM = 0;

    public:
        deque<catp_write_entry> writeQ;
        deque<catp_read_entry> readQ;
        deque<catp_prefetch_entry> prefQ;

    public:
        void init(CATPConfig* config);
        bool RQ_is_full();
        bool WQ_is_full();
        bool PQ_is_full();
        void add_rq(uint64_t ip, uint64_t addr, uint64_t cycle, uint64_t depth);
        void add_pq(uint64_t ip, uint64_t addr, uint64_t cycle);
        void add_wq(CATPMetaDataEntry* entry, bool first_write, uint64_t index, uint64_t way, uint64_t addr1, uint64_t addr2, uint64_t conf, uint64_t cycle);
        void read(uint64_t num, uint64_t cycle);
        void write(uint64_t num, uint64_t cycle);
        catp_prefetch_entry pref();
        void insert(uint64_t ip, uint64_t addr1, uint64_t addr2, uint64_t cycle);
        void access(uint64_t ip, uint64_t addr, uint64_t cycle);
        void adjust_assoc(uint32_t assoc);
        uint32_t get_assoc();
        uint64_t get_set_id(uint64_t addr);
        uint64_t get_tag(uint64_t addr);
        void print_stats();
};
