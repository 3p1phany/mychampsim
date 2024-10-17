#include "ooo_cpu.h"
#include "cache.h"
#include "prefetch.h"
#include "cmc.h"
#include "memory_data.h"

#define nPRINT_LOADRET_INSERT
#define nPRINT_LOADRET_POP
#define nPRINT_IDENTITY_INSERT
#define nPRINT_IDENTITY_PREDICT
#define nPRINT_IDENTITY_EVICT
#define nPRINT_IDENTITY_UPDATE_REPL

#define nPRINT_METADATA_READ
#define PRINT_METADATA_INSERT
#define PRINT_METADATA_WRITE
#define nPRINT_METADAT_CYCLE

#define nPRINT_CMC_AGQ_INSERT

// uint64_t print_ip[14] = {0x120001640, 0x120001628, 0x12000162c, 0x120001630, 0x120001634, 0x120001638,
//                          0x120001670, 0x120001658 ,0x12000165c ,0x120001660 ,0x120001664 ,0x120001668,
//                          0x120001684,
//                          0x120001690};

// uint64_t print_rec_ip[4] = {0x120001640, 0x120001670, 0x120001684, 0x120001690};
uint64_t print_rec_ip[4] = {0x120001640, 0x120001670, 0x120001684, 0x120001658};
// uint64_t print_rec_ip[4] = {0x120000b84};
// uint64_t print_rec_ip[4] = {0x1200010c0};
uint64_t start_print_cycle = 0; //61405291

std::unordered_map<uint64_t, vector<uint64_t>> ip_addr_history;

extern CMCConfig cmc_config[NUM_CPUS];
extern LoadRet load_ret[NUM_CPUS];
extern CMC_AGQ cmc_agq[NUM_CPUS];
extern LoadIdentity load_identity[NUM_CPUS];
extern MetaData_OnChip metadata_onchip[NUM_CPUS];

extern MEMORY_DATA mem_data[NUM_CPUS];

/****** LoadRet ******/ 
void LoadRet::init(CMCConfig *config) {
    cpu = config->cpu;
    SIZE = config->load_ret_size;
    assert(entries.size() == 0);
}

bool LoadRet::is_full(){
    return entries.size() == SIZE;
}

void LoadRet::insert(uint64_t ip, uint64_t ret_val, bool is_consumer, uint64_t instr_id, uint64_t addr, uint8_t size, uint64_t cycle) {
#ifdef PRINT_LOADRET_INSERT
    cout << "[LoadRet Insert] " 
         << "ip: " << hex << ip << dec
         << ", is_consumer: " << is_consumer
         << ", instr_id: " << instr_id
         << ", ret_val: " << hex << ret_val << dec
         << endl;
#endif

    if (is_full()) {
        auto pop_it = entries.front();

        bool is_rec = load_identity[cpu].is_recursive(pop_it.ip);
        if (pop_it.is_consumer && !(is_rec && pop_it.ret_val == 0)) { // Only consumer can be inserted into the load_identity.
            load_identity[cpu].set_identity(pop_it.ip, pop_it.is_producer, pop_it.is_real_producer);
        }

    #ifdef PRINT_LOADRET_POP
        auto print_it = find(begin(print_ip), end(print_ip), pop_it.ip);
        if(print_it != end(print_ip)){
            cout << "\n[LoadRet Pop] " 
                 << "ip: " << hex << pop_it.ip << dec
                 << ", is_producer: " << +pop_it.is_producer
                 << ", is_consumer: " << +pop_it.is_consumer
                 << ", is_real_producer: " << +pop_it.is_real_producer
                 << ", instr_id: " << pop_it.instr_id
                 << ", ret_val: " << hex << pop_it.ret_val << dec
                 << endl;

            cout << "[Identity] ip: " << hex << pop_it.ip << dec << ", ";
                load_identity[cpu].print(pop_it.ip);
                cout << endl;
        }
    #endif

        entries.pop_front();
    }

    LoadRetEntry new_entry;
    new_entry.ret_val = ret_val;
    new_entry.ip = ip;
    new_entry.is_consumer = is_consumer;
    new_entry.instr_id = instr_id;
    new_entry.addr = addr;
    new_entry.size = size;
    new_entry.enq_cycle = cycle;
    entries.push_back(new_entry);

    assert(entries.size() <= SIZE);
}

uint64_t LoadRet::lookup(uint64_t ip, uint64_t base_addr, bool is_data) {
    /** Reverse Search */
    for (auto it = entries.rbegin(); it != entries.rend(); it++) {
        if (it->ret_val == base_addr && it->size == SIZE_DWORD) {
            it->is_producer = true;
        #ifdef ENABLE_JPP
            it->is_real_producer = (it->ip == ip); 
        #else
            it->is_real_producer = it->is_real_producer || !is_data;
        #endif
            return it->ip;
        }
    }
    return 0;
}

void LoadRet::print() {
    cout << "[LoadRet] " << endl;
    for (auto it = entries.begin(); it != entries.end(); it++) {
        cout << "[" << it - entries.begin() << "]"
             << " ip: " << hex << it->ip << dec
             << ", ret_val: " << hex << it->ret_val << dec
             << ", is_consumer: " << it->is_consumer
             << ", is_producer: " << it->is_producer
             << endl;
    }
}

/****** LoadIdentity ******/ 
void LoadIdentity::init(CMCConfig *config) {
    cpu = config->cpu;
    SIZE = config->load_identity_size;

    for (uint64_t i = 0; i < SIZE; i++) {
        LoadIdentityEntry new_entry;
        new_entry.lru = i;
        entries[i] = new_entry;
    }

    assert(entries.size() == SIZE);
}

void LoadIdentity::insert(uint64_t ip, LoadIdentityEntry& item) {
#ifdef PRINT_IDENTITY_INSERT
if(ip == 0x1200012fc)
    cout << "[Identity Insert]" 
         << " cycle: " << ooo_cpu[cpu]->current_cycle
         << ", ip: " << hex << ip << dec
         << endl;
#endif

    assert(!contain(ip));
    auto evict_it = entries.begin();
    if(repl == "LRU") {
        bool found = false;
        uint64_t dbg = 0x0;
        for(auto lru_it = entries.begin(); lru_it != entries.end(); lru_it++) {
            dbg |= ((uint64_t)0x1 << lru_it->second.lru);
            if(lru_it->second.lru == 0) {
                assert(!found);
                evict_it = lru_it;
                found = true;
            }
            lru_it->second.lru--;
        }

        assert(found);
        if(SIZE >= 64){
            assert(dbg == ((uint64_t)-1));
        } else {
            assert(dbg == (((uint64_t)0x1 << SIZE) - 1));
        }
    } else {
        uint64_t max_rrip = 0;
        for(auto rrip_it = entries.begin(); rrip_it != entries.end(); rrip_it++) {
            assert(ip != rrip_it->first);
            if(rrip_it->second.rrip > max_rrip) {
                max_rrip = rrip_it->second.rrip;
                evict_it = rrip_it;
            }
        }
    }

#ifdef PRINT_IDENTITY_EVICT
    if(evict_it->first == 0x1200012fc)
        cout << "[Identity Evict] " 
             << "cycle: " << ooo_cpu[cpu]->current_cycle
             << ", ip: " << hex << evict_it->first << dec
             << endl;
#endif
    item.lru = SIZE-1;
    item.recursive_conf = rec_conf_init;
    item.rec_id = cur_rec_id++;

    entries.erase(evict_it);
    entries[ip] = item;
    assert(entries.size() <= SIZE);
}

void LoadIdentity::update_repl(uint64_t ip) {
#ifdef PRINT_IDENTITY_UPDATE_REPL
    if(ooo_cpu[cpu]->current_cycle >= 3920375 && ooo_cpu[cpu]->current_cycle <= 3921350)
        cout << "[Update Repl] ip: " << hex << ip << dec << endl;
#endif

    assert(contain(ip));
    if (repl == "LRU") {
        for(auto it = entries.begin(); it != entries.end(); it++) {
            if(it->second.lru > entries[ip].lru) {
                it->second.lru--;
            }
        }
        entries[ip].lru = SIZE - 1;
        return;
    } else if (repl == "Rrip") {
        bool find = false;
        for(auto it = entries.begin(); it != entries.end(); it++) {
            if(it->first == ip) {
                find = true;
                it->second.rrip = (it->second.rrip > 1) ? (it->second.rrip - 2) : 0;
            } else {
                it->second.rrip = (it->second.rrip < rrip_max) ? (it->second.rrip + 1) : rrip_max;
            }
        }
        assert(find);
        return ;
    } else {
        assert(0);
    }
}

void LoadIdentity::set_info(uint64_t ip, uint64_t producer_ip, uint64_t offset, uint8_t size) {
    /** Insert new entry*/
    if (!contain(ip)) {
        LoadIdentityEntry new_entry;
        new_entry.rrip = 5; // update rrip will set rrip = 3
        insert(ip, new_entry);
    }

    assert(contain(ip));

    assert((entries[ip].producer_conf <= 3) && entries[ip].producer_conf >= 1);
    if(entries[ip].producer_conf == 1){
        entries[ip].producer_ip = producer_ip;
    } else if (entries[ip].producer_ip == producer_ip) {
        entries[ip].producer_conf = (entries[ip].producer_conf == 3) ? 3 : entries[ip].producer_conf + 1;
    } else {
        entries[ip].producer_conf = (entries[ip].producer_conf == 0) ? 0 : entries[ip].producer_conf - 1;
    }

    // entries[ip].producer_ip = producer_ip;
    entries[ip].offset = offset;
    entries[ip].is_offset = is_recursive(producer_ip);
    entries[ip].size = size;

    if (entries.find(producer_ip) != entries.end()){
        entries[ip].rec_id = entries[producer_ip].rec_id;
        entries[ip].is_producer = true;
    } else {
        entries[ip].rec_id = cur_rec_id++;
    }

    // Update rrip
    update_repl(ip);
    assert(entries.size() == SIZE);
}

void LoadIdentity::set_identity(uint64_t ip, bool is_producer, bool is_real_producer) {
    // Assert
    if(is_real_producer){
        assert(is_producer);
    }

    if(!contain(ip)){
        SET_ID_NOT_FOUND_CNT++;
        return ;
    }

    // Update conf
#ifdef ENABLE_JPP
    if(!is_real_producer){
        entries[ip].data_conf = max_conf;
        entries[ip].recursive_conf = 0;
    } else {
        entries[ip].data_conf = 0;
        entries[ip].recursive_conf = max_conf;
    }
#else
    if(!is_real_producer){
        entries[ip].data_conf = (entries[ip].data_conf == max_conf) ? max_conf : entries[ip].data_conf + 1;
        entries[ip].recursive_conf = (entries[ip].recursive_conf == 0) ? 0 : entries[ip].recursive_conf - 1;
    } else {
        entries[ip].data_conf = (entries[ip].data_conf == 0) ? 0 : entries[ip].data_conf - 1;
        entries[ip].recursive_conf = (entries[ip].recursive_conf == max_conf) ? max_conf : entries[ip].recursive_conf + 1;
    }
#endif

    // Update replace policy
    update_repl(ip);
}

void LoadIdentity::predict(uint64_t ip, uint64_t addr, bool cache_miss, uint64_t cycle){
#ifndef CMC_RECORD_ALL
    bool is_rec = is_recursive(ip);
#else
    bool is_rec = true;
#endif
    if(is_rec) {
    #ifdef CMC_RESPECTIVE_ADDR
        uint64_t base_addr = addr;
    #else
        uint64_t base_addr = (is_recursive(ip))? (addr - get_offset(ip)) : addr;
    #endif

    #ifndef CMC_PC_LOCALIZATION
        #define LOOKUP_HIS_LENGTH 16
        auto it = find(history_buffer.end()-LOOKUP_HIS_LENGTH, history_buffer.end(), base_addr);
        if(it != history_buffer.end()){
            if(cache_miss) {METADATA_CYCLE_NUM++;}
            return ;
        }
    #endif

    #ifdef PRINT_IDENTITY_PREDICT
        cout << "[Predict] ip: " << hex << ip << dec 
             << ", base_addr: " << hex << base_addr << dec
             << ", cache_miss: " << cache_miss
             << ", his_buffer.size: " << history_buffer.size()
             << endl;
    #endif

        metadata_onchip[cpu].access(ip, base_addr, cycle); 

        if(cache_miss){            
        #ifdef CMC_PC_LOCALIZATION
            if(ip_addr_history[ip].size() >= PREFETCH_DISTANCE){
                uint64_t trigger_addr = *(ip_addr_history[ip].end() - PREFETCH_DISTANCE);
                metadata_onchip[cpu].insert(ip, trigger_addr, base_addr, cycle);
                METADATA_INSERT_NUM++;
            }
        #else
            if(history_buffer.size() >= PREFETCH_DISTANCE){
                uint64_t trigger_addr = *(history_buffer.end()-PREFETCH_DISTANCE);
                metadata_onchip[cpu].insert(ip, trigger_addr, base_addr, cycle);
                METADATA_INSERT_NUM++;
            }
        #endif
        }

    #ifdef CMC_PC_LOCALIZATION
        if(ip_addr_history[ip].size() == 16){
            ip_addr_history[ip].erase(ip_addr_history[ip].begin());
            ip_addr_history[ip].push_back(base_addr);
        } else {
            ip_addr_history[ip].push_back(base_addr);
        }
    #endif

        if(history_buffer.size() == 16){
            history_buffer.erase(history_buffer.begin());
            history_buffer.push_back(base_addr);
        } else {
            history_buffer.push_back(base_addr);
        }

        assert(history_buffer.size() <= 16);
    }
}

bool LoadIdentity::has_successor(uint64_t ip){
    for (auto it = entries.begin(); it != entries.end(); it++) {
        if(it->second.producer_ip == ip){
            return true;
        }
    }

    return false;
}

uint64_t LoadIdentity::get_offset(uint64_t ip) {
    assert(contain(ip));

    return entries[ip].offset;
}

// Return the number of offset
pair<uint64_t, uint64_t> LoadIdentity::get_offset_array(uint64_t ip, uint64_t base_addr, uint64_t rec_addr, uint64_t* offsets, uint64_t* dlinks, uint64_t* ips) {
    assert(is_recursive(ip));

    uint64_t offset_cnt = 0;
    uint64_t dlink_cnt = 0;
    vector<uint64_t> block_addr = {rec_addr >> 6};
    uint64_t rec_id = entries[ip].rec_id;

    for (auto it = entries.begin(); it != entries.end(); it++) {
        if(rec_id == it->second.rec_id && is_data(it->first) && is_recursive(it->second.producer_ip)){
            uint64_t pref_addr = (base_addr + it->second.offset) >> 6;

            // if(find(begin(print_rec_ip), end(print_rec_ip), ip) != end(print_rec_ip)){
            //     if(base_addr == 0x4000b0b7f8){
            //         cout << "[get_offset_array find d-link] "
            //              << "ip: " << hex << it->first << dec
            //              << ", rec_id: " << it->second.rec_id
            //              << ", offset: " << it->second.offset
            //              << ", pref_addr: " << hex << pref_addr << dec
            //              << endl;
                
            //         cmc_agq[cpu].entries[0].print();
            //     }
            // }

            if(find(block_addr.begin(), block_addr.end(), pref_addr) == block_addr.end()){
                block_addr.push_back(pref_addr);
                offsets[offset_cnt] = it->second.offset;
                offset_cnt++;
            }

            // search data load that depends on the d-link
            for (auto it2 = entries.begin(); it2 != entries.end(); it2++) {
                if(is_data(it2->first) && it2->second.rec_id == it->second.rec_id && it->first == it2->second.producer_ip){ // has consumer
                    uint64_t dlink_addr = base_addr + it->second.offset;
                    if(find(dlinks, dlinks+dlink_cnt, dlink_addr) == dlinks+dlink_cnt){
                        dlinks[dlink_cnt] = dlink_addr;
                        ips[dlink_cnt] = it->first;
                        dlink_cnt++;
                    // if(find(begin(print_rec_ip), end(print_rec_ip), ip) != end(print_rec_ip)){
                    //     if(base_addr == 0x4000b0b7f8){
                    //         cout << "[get_offset_array find d-load] "
                    //              << "ip: " << hex << it->first << dec
                    //              << ", rec_id: " << it->second.rec_id
                    //              << ", offset: " << it->second.offset
                    //              << ", dlink_addr: " << hex << dlink_addr << dec
                    //              << ", pref_addr: " << hex << pref_addr << dec
                    //              << ", dlinks size: " << dlinks.size()
                    //              << endl;
                    //     }
                    // }

                        assert(it->second.size==SIZE_DWORD);
                        break;
                    }
                }
            }
        }
    }

    //if(find(begin(print_rec_ip), end(print_rec_ip), ip) != end(print_rec_ip)){
    //    if(base_addr == 0x4000bed180){
    //        cout << "cnt: " << cnt << endl;
    //        cout << "rec_id: " << rec_id << endl;
    //        cout << "addr: " << hex << base_addr << dec << endl;
    //        print_final_stats();
    //    }
    //}

    // if(find(begin(print_rec_ip), end(print_rec_ip), ip) != end(print_rec_ip)){
    //     cout << "[plink]" 
    //          << " plink_cnt: " << plink_cnt
    //          << ", ip: " << hex << ip << dec
    //          << ", base_addr: " << hex << base_addr << dec;
    //     for(uint64_t i = 0; i < plink_cnt; i++){
    //         cout << ", " << hex << plinks[i] << dec;
    //     }
    //     cout << endl;
    //     print_final_stats();
    //     cout << endl << endl;
    // }

    return make_pair(offset_cnt, dlink_cnt);
}

void LoadIdentity::print_final_stats() {
    cout << "-------------------------" << endl;
    cout << "[Load Identity Stats] " << endl;
    cout << "Recursive Load: " << endl;
    vector<uint64_t> rec_ip;
    for (auto it = entries.begin(); it != entries.end(); it++) {
        if (it->second.recursive_conf >= threshold) {
            rec_ip.push_back(it->first);
        }
    }
    sort(rec_ip.begin(), rec_ip.end());
    for(auto it = rec_ip.begin(); it != rec_ip.end(); it++){    
        print(*it);
    }


    cout << "Data Load: " << endl;
    vector<uint64_t> data_ip;
    for (auto it = entries.begin(); it != entries.end(); it++) {
        if (it->second.data_conf >= threshold) {
            data_ip.push_back(it->first);
        }
    }
    sort(data_ip.begin(), data_ip.end());
    for(auto it = data_ip.begin(); it != data_ip.end(); it++){
        print(*it);
    }

    cout << "All: " << endl;
    for (auto it = entries.begin(); it != entries.end(); it++) {
        print(it->first);
    }

    cout << "SET_ID_NOT_FOUND_CNT: " << SET_ID_NOT_FOUND_CNT << endl;
    cout << "METADATA_INSERT_NUM: " << METADATA_INSERT_NUM 
         << ", METADATA_CYCLE_NUM: " << METADATA_CYCLE_NUM
         << endl;
    cout << "-------------------------" << endl;
}


/****** AGQ ******/ 
void CMC_AGQ::init(CMCConfig *config) {
    cpu = config->cpu;
    assert(entries.size() == 0);
}

bool CMC_AGQ::insert(CMC_AGQ_ITEM& item){
    CMC_AGQ_INSERT_CNT++;
    item.enq_cycle = ooo_cpu[cpu]->current_cycle;

#ifdef PRINT_CMC_AGQ_INSERT
if(item.ret_value == 0x1200f7790){
    cout << "\t[CMC AGQ insert] "  << ", ";
    item.print();
}
#endif

    if(entries.size() == SIZE){
        CMC_AGQ_FULL_CNT++;

        if(pop_when_full){
            entries.erase(entries.begin());
        } else {
            return false;
        }
    }

    entries.push_back(item);
    return true;
}

vector<CMC_AGQ_ITEM>::iterator CMC_AGQ::search_addr(uint64_t addr){
    for(auto it = entries.begin(); it != entries.end(); it++){
        if(it->issued && (addr >> 6) == (it->ret_value >> 6)){
            return it;
        }
    }
    return entries.end();
}

void CMC_AGQ::load_return(uint64_t addr){
    auto it = search_addr(addr);
    while(it != entries.end()){
        uint64_t vaddr = (addr & 0xffffffffffffffc0) | (it->ret_value & 0x3f);
        uint8_t size = it->size;
        // assert(size == SIZE_DWORD);
        uint64_t ret_val = mem_data[cpu].read(vaddr, size, false);

        bool has_successor = cmc_agq[cpu].update_src(it, ret_val);
        if(!has_successor){
            check_cycle(it);
            entries.erase(it);
        }

        if(vaddr == 0){
            cout << "vaddr: " << hex << vaddr << dec  << ", size: " << +size
                 << ", ret_val: " << hex << ret_val << dec << endl;
        }

        it = search_addr(addr);
    }
}

void CMC_AGQ::remove_expired(){
    for(auto it = entries.begin(); it != entries.end(); ){
        if(ooo_cpu[cpu]->current_cycle - it->enq_cycle > 1000){
            entries.erase(it);
            it = entries.begin();
        } else {
            it++;
        }
    }
}

void CMC_AGQ::check_cycle(vector<CMC_AGQ_ITEM>::iterator ret_item){
    if(ooo_cpu[cpu]->current_cycle - ret_item->enq_cycle > 100000){
        cout << "A item resides in CMC_AGQ for more than 100000 cycles. "
             << "current_cycle: " << ooo_cpu[cpu]->current_cycle << endl;
        ret_item->print();
        exit(1);
    }
}

bool CMC_AGQ::update_src(vector<CMC_AGQ_ITEM>::iterator ret_item, uint64_t ret_val){
    bool has_found = false;
    uint64_t found_num = 0;

    for(auto it = load_identity[cpu].entries.begin(); it != load_identity[cpu].entries.end(); it++){
        bool need_handle = it->second.need_handle();

        if(it->second.producer_ip == ret_item->ip && need_handle){
            assert(it->second.recursive_conf <= 2); //Only handle the data load, otherwise there will be a loop.
            if(!has_found){
                check_cycle(ret_item);
                ret_item->issued = false;
                ret_item->ret_value = ret_val;
                ret_item->ip = it->first;
                ret_item->size = it->second.size;
                ret_item->cycle = ooo_cpu[cpu]->current_cycle;
            } else {
                CMC_AGQ_ITEM new_item;
                new_item.issued = false;
                new_item.ret_value = ret_val;
                new_item.ip = it->first;
                new_item.size = it->second.size;
                new_item.cycle = ooo_cpu[cpu]->current_cycle;
                new_item.enq_cycle = ret_item->enq_cycle;
                insert(new_item);
            }
        
            has_found = true;
            found_num++;
        }
    }

    return has_found;
}

vector<CMC_AGQ_ITEM>::iterator CMC_AGQ::first_ready_item(){
    for(auto it = entries.begin(); it != entries.end(); it++){
        if(!it->issued){
            return it;
        }
    }

    return entries.end();
}

void CMC_AGQ::print_final_stats(){
    cout << "-------------------------" << endl;
    cout << "[CMC AGQ Stats] " << endl;
    cout << "CMC_AGQ_INSERT_CNT: " << CMC_AGQ_INSERT_CNT << endl;
    cout << "CMC_AGQ_FULL_CNT: " << CMC_AGQ_FULL_CNT << endl;
    cout << "CMC_AGQ_IP_EXPIRED: " << CMC_AGQ_IP_EXPIRED << endl;
    cout << "CMC_AGQ_BEYOND_NUM: " << CMC_AGQ_BEYOND_NUM << endl;
    cout << "-------------------------" << endl;
}

/****** MetaData_OnChip ******/ 
void MetaData_OnChip::init(CMCConfig *config) {
    cpu = config->cpu;
    assoc = config->assoc;
    num_sets = config->num_sets;
    //line_size = config->line_size;
    use_dynamic_assoc = config->use_dynamic_assoc;
    entries.resize(config->num_sets);

    RQ_SIZE = config->readq_size;
    WQ_SIZE = config->writeq_size;
    PQ_SIZE = config->prefq_size;
    metadata_delay = config->metadata_delay;
    assert(readQ.size() == 0);
    assert(writeQ.size() == 0);
    assert(prefQ.size() == 0);

    assert((num_sets & (num_sets - 1)) == 0); // Ensure that num_sets is a power of 2
    index_mask = num_sets - 1;
    index_length = lg2(num_sets);
}

bool MetaData_OnChip::RQ_is_full() {
    return readQ.size() == RQ_SIZE;
}

bool MetaData_OnChip::WQ_is_full() {
    return writeQ.size() == WQ_SIZE;
}

bool MetaData_OnChip::PQ_is_full() {
    return prefQ.size() == PQ_SIZE;
}

uint64_t MetaData_OnChip::get_set_id(uint64_t addr) {
#ifdef CMC_DIRECT_INDEX
    uint64_t shift6 = addr >> 6;
    uint64_t set_id = (shift6) & index_mask;
#else
    uint64_t shift5 = addr >> 5;
    uint64_t shift6 = addr >> 6;
    uint64_t set_id = (shift5 ^ shift6) & index_mask;
#endif

    assert(set_id < num_sets);
    return set_id;
} 

uint64_t MetaData_OnChip::get_tag(uint64_t addr) {
#ifdef CMC_DIRECT_INDEX
    uint64_t tag = addr >> (6 + index_length);
#else
    uint64_t tag = addr >> (5 + index_length);
#endif
    return tag;
}

void MetaData_OnChip::add_rq(uint64_t ip, uint64_t addr, uint64_t cycle){
    if (RQ_is_full()) {
        RQ_FULL_CNT++;
        readQ.pop_front();
    }
    metadata_rq_add++;
    read_entry new_entry;
    new_entry.ip = ip;
    new_entry.addr = addr;
    new_entry.cycle = cycle;
    readQ.push_back(new_entry);
}

void MetaData_OnChip::add_wq(MetaDataEntry* entry, bool first_write, uint64_t index, uint64_t way, uint64_t addr2, uint64_t conf, uint64_t ip, uint64_t addr1, uint64_t cycle) {
    if (WQ_is_full()) {
        WQ_FULL_CNT++;
        writeQ.pop_front();
    }
    metadata_wq_add++;
    write_entry new_entry;
    new_entry.entry = entry;
    new_entry.first_write = first_write;
    new_entry.index = index;
    new_entry.way = way;
    new_entry.addr2 = addr2;
    new_entry.conf = conf;
    new_entry.ip = ip;
    new_entry.addr1 = addr1;
    new_entry.cycle = cycle;
    writeQ.push_back(new_entry);
}

void MetaData_OnChip::add_pq(uint64_t ip, uint64_t addr, uint64_t cycle) {
    if (PQ_is_full()) {
        PQ_FULL_CNT++;
        // return ;
        prefQ.pop_front();
    }
    prefetch_count++;
    prefetch_entry new_entry;
    new_entry.ip = ip;
    new_entry.addr = addr;
    new_entry.cycle = cycle;
    prefQ.push_back(new_entry);
}

void MetaData_OnChip::read(uint64_t num, uint64_t current_cycle) {
    for (uint64_t i = 0; i < num; i++) {
        if (readQ.size() == 0) {
            return;
        }
        auto read = readQ.front();
        uint64_t read_ip = read.ip;
        uint64_t read_addr = read.addr;
        uint64_t cycle = read.cycle;
        if (current_cycle >= cycle + metadata_delay){
            readQ.pop_front();

            metadata_rq_issue++;
            uint64_t prefetch_addr = read_addr;

            uint64_t set_id = get_set_id(prefetch_addr);
            uint64_t tag = get_tag(prefetch_addr);

            // uint64_t block_addr = addr >> LOG2_BLOCK_SIZE;
            map<uint64_t, MetaDataEntry>& entry_map = entries[set_id];
            map<uint64_t, MetaDataEntry>::iterator it = entry_map.find(tag);

        #ifdef PRINT_METADATA_READ
            if(cycle == 6696541){
                cout << "[Metadata Read] pref_addr: " << hex << prefetch_addr << dec
                    << ", prefetch_addr: " << hex << it->second.next_addr << dec
                    << ", tag: " << hex << tag << dec
                    << ", set_id: " << hex << set_id << dec
                    << endl;
            }
        #endif

            if (it != entry_map.end()) {
                prefetch_addr = it->second.next_addr;
                metadata_rq_hit++;
            } else {
                break;
            }

            if(prefetch_addr != 0) {
                #ifdef METADATA_DEBUG
                printf("addr: %lx triger prefetch addr: %lx\n", read_addr, prefetch_addr);
                #endif
                add_pq(read_ip, prefetch_addr, current_cycle);
            }
        }
    }
}

void MetaData_OnChip::write(uint64_t num, uint64_t current_cycle) {
    for (uint64_t i = 0; i < num; i++) {
        if (writeQ.size() == 0) {
            return;
        }
        write_entry write = writeQ.front();
        if (current_cycle >= write.cycle + metadata_delay && !write.first_write) {
    #ifdef nPRINT_METADATA_WRITE
        // if(write.addr2 == 0x4000a1c240 || write.addr1 == 0x4000a1c240)
            cout << "[Metadata Write] " 
                 << "ip: " << hex << write.ip << dec
                 << ", addr1: " << hex << write.addr1 << dec
                 << ", addr2: " << hex << write.addr2 << dec
                 << ", conf: " << write.conf
                 << ", index: " << write.index
                 << ", way: " << write.way
                 << endl;
    #endif
            write.entry->next_addr = write.addr2;
            write.entry->conf = write.conf;
            writeQ.pop_front();
            metadata_wq_issue++;
        }
    }
}

prefetch_entry MetaData_OnChip::pref() {
    if(prefQ.size() == 0) {
        return {0,0};
    }
    else {
        auto ret = prefQ.front();
        prefQ.pop_front();
        return ret;
    }
}

void MetaData_OnChip::insert(uint64_t ip, uint64_t addr1, uint64_t addr2, uint64_t cycle) {

#ifdef nPRINT_METADATA_INSERT
    cout << "[Metadata Insert] " 
         << "ip: " << hex << ip << dec
         << ", assoc: " << hex << assoc << dec
         << ", addr1: " << hex << addr1 << dec
         << ", addr2: " << hex << addr2 << dec
         << endl;
#endif

    // Set LUT
    uint64_t addr1_lut_idx = (addr1 >> (11+5)) & ((1 << 11)-1);
    lut_entries[addr1_lut_idx].tag = addr1 >> (11+5);

    uint64_t addr2_lut_idx = (addr2 >> (11+5)) & ((1 << 11)-1);
    lut_entries[addr2_lut_idx].tag = addr2 >> (11+5);

    cmc_unique_addr.insert(addr1);
    if ((++access_cnt) % METADATA_INTERVAL == 0){
        if (use_dynamic_assoc){
            uint32_t new_assoc = cmc_unique_addr.size() / (num_sets*CMC_LINE_SIZE) + 1;
            assoc = new_assoc > MAX_ASSOC ? MAX_ASSOC : new_assoc;
        } else {
            assoc = 6;
        }

        cmc_unique_addr.clear();
    }

    if (assoc == 0) {
        return;
    }

    trigger_addr_recursive.insert(addr1 >> 5);

    unique_addr0.insert(addr2 >> 16);
    unique_addr1.insert((addr2 >> 16) & ((1 << 11)-1));

    uint64_t set_id = get_set_id(addr1);
    uint64_t tag = get_tag(addr1);
    map<uint64_t, MetaDataEntry>& entry_map = entries[set_id];
    map<uint64_t, MetaDataEntry>::iterator it = entry_map.find(tag);

    // uint64_t block_addr1 = addr1 >> LOG2_BLOCK_SIZE;
    uint64_t block_addr2 = addr2; // TODO
    uint64_t new_addr2 = 0;
    uint64_t new_conf = 0;

    metadata_update++;
    if(find(begin(print_rec_ip), end(print_rec_ip), ip) != end(print_rec_ip)){
        ip_metadata_update++;
    }
    if (it != entry_map.end()) {
        bool need_write = true;
        if ((block_addr2 >> 3) == (it->second.next_addr >> 3)) {
            new_addr2 = it->second.next_addr;
            new_conf = (it->second.conf == 3) ? 3 : it->second.conf+1;  // TODO:
            metadata_inc_conf++;
            if(find(begin(print_rec_ip), end(print_rec_ip), ip) != end(print_rec_ip)){
                ip_metadata_inc_conf++;
            }
            #ifdef METADATA_DEBUG
            printf("confirm addr1: %lx, addr2: %lx\n", addr1, addr2);
            #endif
            if(it->second.conf == 3) {
                need_write = false;
            }
        } else {
            metadata_dec_conf++;
            if(find(begin(print_rec_ip), end(print_rec_ip), ip) != end(print_rec_ip)){
                ip_metadata_dec_conf++;
            }
            
            if (it->second.conf == 1) {
                metadata_dec_conf_update++;
                if(find(begin(print_rec_ip), end(print_rec_ip), ip) != end(print_rec_ip)){
                    ip_metadata_dec_conf_update++;
                }
                new_addr2 = block_addr2;
                new_conf = 2; // TODO:
                #ifdef METADATA_DEBUG
                printf("update addr1: %lx, addr2: %lx\n", addr1, addr2);
                #endif
            }
            else {
                new_addr2 = it->second.next_addr;
                new_conf = it->second.conf-1;
                #ifdef METADATA_DEBUG
                printf("exist addr1: %lx, addr2: %lx\n", addr1, it->second.next_addr);
                #endif
            }
        }
        if(need_write){
            add_wq(&(it->second), false, 0, 0, new_addr2, new_conf, ip, addr1, cycle);
        }
    } else {
        new_addr++;
        bool new_alloc = false;
        uint32_t way_num = 0;

        if (entry_map.size() < (assoc*CMC_LINE_SIZE)){
            new_alloc = true;
            way_num = entry_map.size() / CMC_LINE_SIZE;
        }
        while(entry_map.size() >= (assoc*CMC_LINE_SIZE)) {
            uint64_t victim = (rand() % (assoc*CMC_LINE_SIZE));  // TODO: evict
            auto it = entry_map.begin();
            std::advance(it, victim);
            #ifdef METADATA_DEBUG
            printf("victim addr2: %lx\n", it->second.next_addr);
            #endif
            entry_map.erase(it);
        }

        MetaDataEntry entry;
        entry.next_addr = 0;
        entry.conf = 0;
        entry_map[tag] = entry;
        #ifdef METADATA_DEBUG
        printf("ip %lx write addr1: %lx, addr2: %lx\n", ip, addr1, addr2);
        #endif
        add_wq(&(entry_map[tag]), new_alloc, set_id, way_num, block_addr2, 2, ip, addr1, cycle);

        assert(entry_map.size() <= (MAX_ASSOC*CMC_LINE_SIZE));
    }
}

void MetaData_OnChip::access(uint64_t ip, uint64_t addr, uint64_t cycle) {
    if (use_dynamic_assoc && (access_cnt < METADATA_INTERVAL)){
        uint32_t new_assoc = cmc_unique_addr.size() / (num_sets*CMC_LINE_SIZE) + 1;
        assoc = new_assoc > MAX_ASSOC ? MAX_ASSOC : new_assoc;
    } else {
        assoc = 6;
    }
    trigger_count++;
    total_assoc += assoc;
    add_rq(ip, addr, cycle);
}

uint64_t MetaData_OnChip::get_assoc() {
    return assoc;
}

void MetaData_OnChip::check_state(){
    // Check the order of cycle in readQ
    uint64_t last_cycle = 0;
    for(auto it = readQ.begin(); it != readQ.end(); it++){
        assert(it->cycle >= last_cycle);
        last_cycle = it->cycle;
    }
}

void MetaData_OnChip::print_stats() {
    cout << "[Metadata OnChip Stats] " << endl;
    cout << "ACCESS_CNT: " << ACCESS_CNT
         << ", RQ_FULL_CNT: " << RQ_FULL_CNT
         << ", WQ_FULL_CNT: " << WQ_FULL_CNT
         << ", PQ_FULL_CNT: " << PQ_FULL_CNT
         << endl;


    cout << "metadata_read_num: " << metadata_rq_issue << endl;
    cout << "metadata_write_num: " << metadata_wq_issue << endl;

    cout << dec << "metadata_trigger_read: " << trigger_count <<endl;
    cout << "metadata_rq_add: " << metadata_rq_add << endl;
    cout << "metadata_rq_issue: " << metadata_rq_issue << endl;
    cout << "metadata_rq_hit: " << metadata_rq_hit << endl;
    cout << "metadata_predict_count: " << prefetch_count <<endl;
    cout << "metadata_update: " << metadata_update << endl;
    cout << "metadata_wq_add: " << metadata_wq_add << endl;
    cout << "metadata_wq_issue: " << metadata_wq_issue << endl;
    // cout << "metadata_pq_add: " << metadata_pq_add << endl;
    // cout << "metadata_pq_issue: " << metadata_pq_issue << endl;
    cout << "meatdata_write_new: " << new_addr <<endl;
    cout << "metadata_conf_change: " << metadata_dec_conf <<endl;
    cout << "metadata_conf_change_modify: " << metadata_dec_conf_update <<endl;
    cout << "metadata_conf_same: " << metadata_inc_conf <<endl;
    if (trigger_count) cout << "avg_assoc: " << (float)total_assoc / trigger_count <<endl;
    cout << "recursive entry: " << trigger_addr_recursive.size() << endl;
    cout << "unique_addr0: " << unique_addr0.size() << endl;
    cout << "unique_addr1: " << unique_addr1.size() << endl;

    cout << "ip_metadata update: " << ip_metadata_update <<endl;
    cout << "metadata_conf_change_ip: " << ip_metadata_dec_conf <<endl;
    cout << "metadata_conf_same_ip: " << ip_metadata_inc_conf <<endl;
    cout << "ip_metadata dec conf update: " << ip_metadata_dec_conf_update << endl;

}
