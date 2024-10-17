
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

#define nENABLE_DBP

struct DBPConfig {
    uint32_t cpu;

    // LoadRet
    uint64_t dbp_load_ret_size;
    // LoadIdentity
    uint64_t dbp_load_identity_size;
    uint64_t dbp_prefq_size;
};

struct DBPLoadRetEntry {
    uint64_t ret_val;
    uint64_t ip;
    uint8_t size;
};

class DBPLoadRet {
    private:
        uint32_t cpu;
        deque<DBPLoadRetEntry> entries;
        uint64_t SIZE;
        
    public:
        void init(DBPConfig *config);
        void insert(uint64_t ip, uint64_t ret_val, uint8_t size);
        void lookup(uint64_t ip, uint64_t base_addr, uint64_t offset);
        bool is_full();
};

struct DBPLoadIdentityEntry
{
    uint64_t producer_ip; // should be reaplaced with index
    uint64_t consumer_ip; // should be reaplaced with index
    uint64_t offset;
    uint64_t lru;
    uint8_t rrip = 0;
    uint8_t conf;
};

class DBPLoadIdentity {
    private:
        uint32_t cpu;

        string repl = "LRU";
        uint8_t rrip_max = 7;
    public:
        uint64_t SIZE;
        vector<DBPLoadIdentityEntry> entries;

        void init(DBPConfig *config);
        void insert(uint64_t producer_ip, uint64_t consumer_ip, uint64_t offset);
        uint64_t check(uint64_t ip, uint64_t* offset);
        void print();
};

class DBPPrefQ {
    public:
        uint64_t SIZE;
        deque<uint64_t> entries;

        void init(DBPConfig *config);
        bool is_full();
        void add(uint64_t base, uint64_t offset);
        uint64_t pref();
};
