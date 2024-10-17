#include "ooo_cpu.h"
#include "cache.h"
#include "prefetch.h"
#include "dbp.h"
#include "memory_data.h"


extern DBPConfig dbp_config[NUM_CPUS];
extern DBPLoadRet dbp_load_ret[NUM_CPUS];
extern DBPLoadIdentity dbp_load_identity[NUM_CPUS];
extern DBPPrefQ dbp_prefq[NUM_CPUS];

/****** LoadRet ******/ 
void DBPLoadRet::init(DBPConfig *config) {
    cpu = config->cpu;
    SIZE = config->dbp_load_ret_size;
    assert(entries.size() == 0);
}

bool DBPLoadRet::is_full(){
    return entries.size() == SIZE;
}

void DBPLoadRet::insert(uint64_t ip, uint64_t ret_val, uint8_t size) {
    if (is_full()) {
        entries.pop_front();
    }

    DBPLoadRetEntry new_entry;
    new_entry.ret_val = ret_val;
    new_entry.ip = ip;
    new_entry.size = size;
    entries.push_back(new_entry);

    assert(entries.size() <= SIZE);
}

void DBPLoadRet::lookup(uint64_t ip, uint64_t base_addr, uint64_t offset) {
    /** Reverse Search */
    for (auto it = entries.rbegin(); it != entries.rend(); it++) {
        if (it->ret_val == base_addr && it->size == SIZE_DWORD) {
            dbp_load_identity[cpu].insert(it->ip, ip, offset);
        }
    }
}

/****** LoadIdentity ******/ 
void DBPLoadIdentity::init(DBPConfig *config) {
    cpu = config->cpu;
    SIZE = config->dbp_load_identity_size;

    for (uint64_t i = 0; i < SIZE; i++) {
        DBPLoadIdentityEntry new_entry;
        new_entry.producer_ip = 0;
        new_entry.consumer_ip = 0;
        new_entry.offset = 0;
        new_entry.conf = 0;
        new_entry.rrip = 0;
        new_entry.lru = i;
        entries.push_back(new_entry);
    }

    assert(entries.size() == SIZE);
}

void DBPLoadIdentity::insert(uint64_t producer_ip, uint64_t consumer_ip, uint64_t offset) {
    auto hit_it = entries.begin();
    bool hit = false;
    for(auto it = entries.begin(); it != entries.end(); it++) {
        if (it->consumer_ip == consumer_ip && it->producer_ip == producer_ip){
            assert(!hit);
            if (it->offset == offset){
                it->conf = (it->conf == 3) ? 3 : (it->conf  + 1);
            }
            else {
                it->offset = offset;
                it->conf = 1;
            }
            hit_it = it;
            hit = true;
        }
    }

    if(!hit) {
        auto evict_it = entries.begin();
        if(repl == "LRU") {
            bool found = false;
            uint64_t dbg = 0x0;
            for(auto lru_it = entries.begin(); lru_it != entries.end(); lru_it++) {
                dbg += ((uint64_t)0x1 << lru_it->lru);
                if(lru_it->lru == 0) {
                    assert(!found);
                    evict_it = lru_it;
                    found = true;
                }
                lru_it->lru--;
            }

            assert(found);
            if(SIZE == 64){
                assert(dbg == ((uint64_t)-1));
            } else {
                assert(dbg == (((uint64_t)0x1 << SIZE) - 1));
            }
        } else {
            uint64_t max_rrip = 0;
            for(auto rrip_it = entries.begin(); rrip_it != entries.end(); rrip_it++) {
                if(rrip_it->rrip > max_rrip) {
                    max_rrip = rrip_it->rrip;
                    evict_it = rrip_it;
                }
            }
        }

        DBPLoadIdentityEntry item;
        item.lru = SIZE-1;
        item.rrip = 0;
        item.producer_ip = producer_ip;
        item.consumer_ip = consumer_ip;
        item.offset = offset;
        item.conf = 1;

        entries.erase(evict_it);
        entries.push_back(item);
        assert(entries.size() <= SIZE);
    }
    else {
        if (repl == "LRU") {
            for(auto it = entries.begin(); it != entries.end(); it++) {
                if(it->lru > hit_it->lru) {
                    it->lru--;
                }
            }
            hit_it->lru = SIZE - 1;
            return;
        } else if (repl == "Rrip") {
            bool find = false;
            for(auto it = entries.begin(); it != entries.end(); it++) {
                if(it->consumer_ip == consumer_ip) {
                    find = true;
                    it->rrip = (it->rrip > 1) ? (it->rrip - 2) : 0;
                } else {
                    it->rrip = (it->rrip < rrip_max) ? (it->rrip + 1) : rrip_max;
                }
            }
            assert(find);
            return ;
        } else {
            assert(0);
        }
    }
}

uint64_t DBPLoadIdentity::check(uint64_t ip, uint64_t* offset){
    uint64_t cnt = 0;
    for(auto it = entries.begin(); it != entries.end(); it++) {
        if (it->producer_ip == ip){
            offset[cnt] = it->offset;
            cnt++;
        }
    }
    return cnt;
}

void DBPLoadIdentity::print(){
    for(auto it = entries.begin(); it != entries.end(); it++) {
        cout << hex << "producer_ip: " << it->producer_ip << " consumer_ip: " << it->consumer_ip << " offset: " << it->offset << " conf: " << (uint64_t)it->conf <<dec << endl;
    }
}

void DBPPrefQ::init(DBPConfig *config) {
    SIZE = config->dbp_prefq_size;
    assert(entries.size() == 0);
}

bool DBPPrefQ::is_full(){
    return entries.size() == SIZE;
}

void DBPPrefQ::add(uint64_t base, uint64_t offset) {
    if (is_full()) {
        entries.pop_front();
    }

    entries.push_back(base+offset);

    assert(entries.size() <= SIZE);
}

uint64_t DBPPrefQ::pref(){
    if (entries.empty()){
        return 0;
    }
    else {
        auto addr = entries.front();
        entries.pop_front();
        return addr;
    }
}