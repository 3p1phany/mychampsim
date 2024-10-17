#ifndef __TRIAGE_H__
#define __TRIAGE_H__

#include <iostream>
#include <map>
#include <vector>
#include <unordered_set>

#include "champsim.h"
#include "triage_training_unit.h"
#include "triage_onchip.h"

#define COMPRESS_METADATA true
#define nENABLE_TRIAGE
#define nCONFLICT_DEBUG

struct TriageConfig {
    int lookahead;
    int degree;
    uint64_t metadata_delay;

    int on_chip_set, on_chip_assoc;
    int training_unit_size;
    bool use_dynamic_assoc;
    uint32_t max_assoc;

    TriageReplType repl;
};

struct triage_write_entry {
    bool dec_conf, inc_conf, update;
    Metadata new_entry;
    uint64_t pc, trigger_addr;
    uint64_t cycle;
};
struct triage_read_entry {
    uint64_t pc, addr, cur_degree;
    uint64_t cycle;
};
struct triage_prefetch_entry {
    uint64_t pc, addr;
};

class Triage {
    TriageTrainingUnit training_unit;

    uint64_t lookahead, degree;

    uint64_t WQ_SIZE;
    uint64_t RQ_SIZE;
    uint64_t PQ_SIZE;
    uint64_t ACCESS_CNT = 0, RQ_FULL_CNT = 0, WQ_FULL_CNT = 0, PQ_FULL_CNT = 0;

    bool RQ_is_full();
    bool WQ_is_full();
    bool PQ_is_full();

    bool add_rq(uint64_t ip, uint64_t addr, uint64_t degree, bool insert_front, uint64_t cycle);
    bool add_wq(uint64_t pc, uint64_t trigger_addr, Metadata new_entry, uint64_t cycle);
    bool add_pq(uint64_t pc, uint64_t addr);

    void train(uint64_t pc, uint64_t addr, bool hit, uint64_t cycle);
    void predict(uint64_t pc, uint64_t addr, bool hit, uint64_t degree, uint64_t cycle);

    public:
    void read(uint64_t cycle);
    void write(uint64_t cycle);

    // Stats
    std::unordered_set<uint64_t>trigger_addr_recursive;
    std::unordered_set<uint64_t>trigger_addr_data;
    std::unordered_set<uint64_t>trigger_addr_other;
    std::unordered_set<uint64_t>trigger_addr_all;

    uint64_t trigger_cnt_recursive = 0;
    uint64_t trigger_cnt_data = 0;
    uint64_t trigger_cnt_other = 0;

    uint32_t cpu;
    uint64_t metadata_delay;
    uint64_t metadata_rq_add, metadata_rq_issue, metadata_rq_hit;
    uint64_t metadata_wq_add, metadata_wq_issue;
    uint64_t metadata_pq_add, metadata_pq_issue;
    uint64_t same_addr, new_addr, new_stream;
    uint64_t no_next_addr, conf_dec_retain, conf_dec_update, conf_inc;
    uint64_t conf_dec_recursive, conf_dec_data, conf_dec_other;
    uint64_t predict_count, trigger_count;
    uint64_t spatial, temporal;
    uint64_t total_assoc;

    uint64_t ip_conf_dec_retain, ip_conf_inc;

    // std::vector<uint64_t> next_addr_list;

    public:
        TriageOnchip on_chip_data;
        Triage();
        void test();
        void set_conf(TriageConfig *config);
        void calculatePrefetch(uint64_t pc, uint64_t addr,
                bool cache_hit, uint64_t *prefetch_list,
                uint64_t max_degree, uint64_t cpu, uint64_t cycle);
        void print_stats();
        uint32_t get_assoc();
        uint32_t get_target_assoc();

        triage_prefetch_entry pref();

        deque<triage_write_entry> writeQ;
        deque<triage_read_entry> readQ;
        deque<triage_prefetch_entry> prefQ;
};

#endif // __TRIAGE_H__
