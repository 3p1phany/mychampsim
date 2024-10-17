
#include <assert.h>
#include <iostream>

#include "triage.h"

using namespace std;

extern uint64_t print_rec_ip[4];
// #define DEBUG

#ifdef DEBUG
#define debug_cout cerr << "[TABLEISB] "
#else
#define debug_cout if (0) cerr
#endif

#include "cmc.h"
extern LoadIdentity load_identity[NUM_CPUS];

void Triage::test() {
    // train(0, 0, 0);
    // train(0, 1, 0);
    // train(0, 4, 0);

    // Metadata c1 = on_chip_data.get_next_entry(0, 0, false);
    //assert(c1.spatial);
    //assert(c1.next_spatial.predict(0).size() == 1);
    //assert(c1.next_spatial.predict(0)[0] == 1);
}

Triage::Triage() {
    trigger_count = 0;
    predict_count = 0;
    same_addr = 0;
    new_addr = 0;
    no_next_addr = 0;
    conf_dec_retain = 0;
    conf_dec_update = 0;
    conf_dec_recursive = 0;
    conf_dec_data = 0;
    conf_dec_other = 0;
    conf_inc = 0;
    new_stream = 0;
    total_assoc = 0;
    spatial = 0;
    temporal = 0;

    ip_conf_dec_retain = 0;
    ip_conf_inc = 0;
}

void Triage::set_conf(TriageConfig *config) {
    lookahead = config->lookahead;
    degree = config->degree;
    WQ_SIZE = 16;
    RQ_SIZE = 16;
    PQ_SIZE = 64;
    metadata_delay = config->metadata_delay;

    training_unit.set_conf(config);
    on_chip_data.set_conf(config);
}

bool Triage::RQ_is_full() {
    return readQ.size() == RQ_SIZE;
}

bool Triage::WQ_is_full() {
    return writeQ.size() == WQ_SIZE;
}

bool Triage::PQ_is_full() {
    return prefQ.size() == PQ_SIZE;
}

bool Triage::add_rq(uint64_t ip, uint64_t addr, uint64_t cur_degree, bool insert_front, uint64_t cycle) {
    assert(!(cur_degree == 0 && insert_front));
    if (RQ_is_full()) {
        RQ_FULL_CNT++;
        if(insert_front){
            cout << "Error: RQ is full and insert front" << endl;
            exit(1);
        }
        readQ.pop_front();
        // return false;
    }
    if (cur_degree > degree) {
        return false;
    }
    metadata_rq_add++;
    triage_read_entry entry;
    entry.pc = ip;
    entry.addr = addr;
    entry.cur_degree = cur_degree + 1;
    entry.cycle = cycle;
    if (insert_front) {
        readQ.push_front(entry);
    } else {
        readQ.push_back(entry);
    }
    return true;
}
bool Triage::add_wq(uint64_t pc, uint64_t trigger_addr, Metadata new_entry, uint64_t cycle) {
    bool in_recursive = trigger_addr_recursive.find(trigger_addr) != trigger_addr_recursive.end();
    bool in_data = trigger_addr_data.find(trigger_addr) != trigger_addr_data.end();
    if (load_identity[cpu].is_recursive(pc)) {
        trigger_addr_recursive.insert(trigger_addr);
        trigger_addr_data.erase(trigger_addr);
        trigger_addr_other.erase(trigger_addr);
        trigger_cnt_recursive++;
    } else if (load_identity[cpu].is_link_data(pc)) {
        if(!in_recursive){
            trigger_addr_data.insert(trigger_addr);
        }
        trigger_addr_other.erase(trigger_addr);
        trigger_cnt_data++;
    } else {
        if(!in_recursive && !in_data){
            trigger_addr_other.insert(trigger_addr);
        }
        trigger_cnt_other++;
    }
    if (warmup_complete[cpu]) trigger_addr_all.insert(trigger_addr);

    // if (load_identity[cpu].is_recursive(pc)) {
    //     trigger_addr_recursive.insert(trigger_addr);
    //     trigger_addr_data.erase(trigger_addr);
    //     trigger_addr_other.erase(trigger_addr);
    // } else if (load_identity[cpu].is_link_data(pc)) {
    //     trigger_addr_data.insert(trigger_addr);
    //     // trigger_addr_recursive.erase(trigger_addr);
    //     trigger_addr_other.erase(trigger_addr);
    // } else {
    //     trigger_addr_other.insert(trigger_addr);
    //     // trigger_addr_recursive.erase(trigger_addr);
    //     // trigger_addr_data.erase(trigger_addr);
    // }

    Metadata next_entry = on_chip_data.get_next_entry(trigger_addr, pc, false);
    if(next_entry == new_entry) {
        conf_inc++;
        #ifdef CONFLICT_DEBUG
        cout << "confirm: " << hex << pc << " trigger " << trigger_addr << " to " << new_entry.addr << dec << "\n";
        #endif
        if(find(begin(print_rec_ip), end(print_rec_ip), pc) != end(print_rec_ip)){
            ip_conf_inc++;
        }
    } else if(next_entry.valid) {
        conf_dec_retain++;
        if (load_identity[cpu].is_recursive(pc)) {
            #ifdef CONFLICT_DEBUG
            cout << "recursive: " << hex << pc << " trigger " << trigger_addr << " to " << new_entry.addr << dec << "\n";
            #endif
            conf_dec_recursive++;
        } 
        else if (load_identity[cpu].is_link_data(pc)) {
            #ifdef CONFLICT_DEBUG
            cout << "data: " << hex << pc << " trigger " << trigger_addr << " to " << new_entry.addr << dec << "\n";
            #endif
            conf_dec_data++;
        }
        else {
            #ifdef CONFLICT_DEBUG
            cout << "other: " << hex << pc << " trigger " << trigger_addr << " to " << new_entry.addr << dec << "\n";
            #endif
            conf_dec_other++;
        }
        if(find(begin(print_rec_ip), end(print_rec_ip), pc) != end(print_rec_ip)){
            ip_conf_dec_retain++;
        }
    }

    if (WQ_is_full()) {
        WQ_FULL_CNT++;
        writeQ.pop_front();
        // return false;
    }
    metadata_wq_add++;

    triage_write_entry entry;
    entry.dec_conf = false;
    entry.inc_conf = false;
    entry.update      = false;
    entry.new_entry = new_entry;
    entry.trigger_addr = trigger_addr;
    entry.pc = pc;
    entry.cycle = cycle;

    if (!next_entry.valid) {
        // no valid correlation for trigger_addr yet
        entry.update = true;
    } else if (next_entry != new_entry) {
        // existing correlation doesn't match the new one
        entry.dec_conf = true;
    } else {
        // existing correlation matches this one
        entry.inc_conf = true;
    }

    writeQ.push_back(entry);
    return true;
}

bool Triage::add_pq(uint64_t pc, uint64_t addr){
    if (PQ_is_full()) {
        PQ_FULL_CNT++;
        return false;
    }

    // TODO: Check is the addr is already in the queue.
    metadata_pq_add++;
    triage_prefetch_entry entry;
    entry.pc = pc;
    entry.addr = addr;
    prefQ.push_back(entry);
    return true;
}

triage_prefetch_entry Triage::pref(){
    if(prefQ.size() == 0){
        return {0, 0};
    } else {
        auto ret = prefQ.front();
        prefQ.pop_front();
        return ret;
    }
}

void Triage::read(uint64_t cycle) {
    if (readQ.empty()) {
        return;
    }
    triage_read_entry entry = readQ.front();
    if (cycle >= metadata_delay + entry.cycle){
        readQ.pop_front();
        metadata_rq_issue++;
        Metadata next_entry = on_chip_data.get_next_entry(entry.addr, entry.pc, false);
        if (next_entry.valid) {
            metadata_rq_hit++;
            if (next_entry.spatial) {
                for (uint64_t pred : next_entry.next_spatial.predict(entry.addr)) {
                    debug_cout << hex << "Predict: " << entry.addr << " " << pred << dec << endl;
                    predict_count++;
                    add_pq(entry.pc, pred);
                    // next_addr_list.push_back(pred);
                    assert(pred != entry.addr);
                }
            } else {
                assert(next_entry.addr != entry.addr);
                if (entry.cur_degree >= degree){
                    debug_cout << hex << "Predict: " <<  next_entry.addr << dec << endl;
                    predict_count++;
                    add_pq(entry.pc, next_entry.addr);
                    // next_addr_list.push_back(next_entry.addr);
                }
                else {
		    add_pq(entry.pc, next_entry.addr);
                    add_rq(entry.pc, next_entry.addr, entry.cur_degree, false, cycle);
                }
            }
        }
        else {
            if (entry.cur_degree > 0){
                debug_cout << hex << "Predict: " <<  entry.addr << dec << endl;
                predict_count++;
                //add_pq(entry.pc, entry.addr);
                // next_addr_list.push_back(entry.addr);
            }
        }
    }
}
void Triage::write(uint64_t cycle) {
    if (writeQ.empty()) {
        return;
    }
    triage_write_entry entry = writeQ.front();
    Metadata md_entry = on_chip_data.get_next_entry(entry.trigger_addr, entry.pc, false);

    if (cycle >= metadata_delay + entry.cycle){
        writeQ.pop_front();
        metadata_wq_issue++;
        if (entry.update || !md_entry.valid) {
            on_chip_data.update(entry.trigger_addr, entry.new_entry, entry.pc, true);
            no_next_addr++;
        }
        if (entry.dec_conf && md_entry.valid) {
            int conf = on_chip_data.decrease_confidence(entry.trigger_addr);
            // conf_dec_retain++;
            if (conf == 0) {
                conf_dec_update++;
                on_chip_data.update(entry.trigger_addr, entry.new_entry, entry.pc, false);
            }
        }
        if (entry.inc_conf && md_entry.valid) {
            on_chip_data.increase_confidence(entry.trigger_addr);
            // conf_inc++;
        }
    }
}

void Triage::train(uint64_t pc, uint64_t addr, bool cache_hit, uint64_t cycle) {
    if (cache_hit) {
        training_unit.set_addr(pc, addr);
        return;
    }

    on_chip_data.triage_unique_addr.insert(addr);
    if (++on_chip_data.triage_access_cnt == METADATA_INTERVAL){
        on_chip_data.triage_unique_addr.clear();
    }

    Metadata new_entry = training_unit.set_addr(pc, addr);
    if (new_entry.valid) {
        bool is_spatial = new_entry.spatial;
        if (!is_spatial && new_entry.addr == addr) {
            // Same Addr
            debug_cout << hex << "Same Addr: " << new_entry.addr << ", " << addr <<endl;
            same_addr++;
        } else {
            // New Addr
            new_addr++;

            uint64_t trigger_addr;
            if (is_spatial) {
                spatial += new_entry.next_spatial.size();
                // if spatial, new_entry contains the real trigger addr
                trigger_addr = new_entry.addr;
            } else {
                temporal++;
                // if temporal, correlate old address with new one
                trigger_addr = new_entry.addr;
                new_entry.addr = addr;
            }

            add_wq(pc, trigger_addr, new_entry, cycle);

            if (new_entry.spatial) {
                // create a link to the next address, if necessary
                trigger_addr = new_entry.next_spatial.last_addr;
                Metadata link_entry;
                link_entry.set_addr(addr);

                add_wq(pc, trigger_addr, link_entry, cycle);
            }
        }
    } else {
        // New Stream
        debug_cout << hex << "StreamHead: " << addr <<endl;
        new_stream++;
    }
}

void Triage::predict(uint64_t pc, uint64_t addr, bool cache_hit, uint64_t degree, uint64_t cycle) {
    add_rq(pc, addr, 0, false, cycle);
}

void Triage::calculatePrefetch(uint64_t pc, uint64_t addr, bool cache_hit, uint64_t *prefetch_list, uint64_t max_degree, uint64_t cpu, uint64_t cycle) {
    // XXX Only allow lookahead = 1 and degree=1 for now
    assert(lookahead == 1);
    // assert(degree == 1);

    assert(degree <= max_degree);
    
    if (pc == 0) return; //TODO: think on how to handle prefetches from lower level

    debug_cout << hex << "Trigger: pc: " << pc << ", addr: " << addr << dec << " " << cache_hit << endl;

    trigger_count++;
    total_assoc += get_target_assoc();

    // Predict
    predict(pc, addr, cache_hit, degree, cycle);

    // Train
    train(pc, addr, cache_hit, cycle);

    // for (size_t i = 0; i < next_addr_list.size() && i < (size_t)degree; i++){
    //     prefetch_list[i] = next_addr_list[i];
    // }
    // next_addr_list.clear();
}

uint32_t Triage::get_assoc() {
    return on_chip_data.get_assoc();
}
uint32_t Triage::get_target_assoc() {
    return on_chip_data.get_target_assoc();
}

void Triage::print_stats() {
    cout << "metadata_read_num: " << metadata_rq_issue << endl;
    cout << "metadata_write_num: " << metadata_wq_issue << endl;

    cout << dec << "metadata_trigger_read: " << trigger_count <<endl;
    cout << "ACCESS_CNT: " << ACCESS_CNT
         << ", RQ_FULL_CNT: " << RQ_FULL_CNT
         << ", WQ_FULL_CNT: " << WQ_FULL_CNT
         << ", PQ_FULL_CNT: " << PQ_FULL_CNT
         << endl;

    cout << "metadata_rq_add: " << metadata_rq_add << endl;
    cout << "metadata_rq_issue: " << metadata_rq_issue << endl;
    cout << "metadata_rq_hit: " << metadata_rq_hit << endl;
    cout << "metadata_predict_count: " << predict_count <<endl;
    cout << "metadata_wq_add: " << metadata_wq_add << endl;
    cout << "metadata_wq_issue: " << metadata_wq_issue << endl;
    cout << "meatdata_write_new: " << no_next_addr <<endl;
    // cout << "metadata_pq_add: " << metadata_pq_add << endl;
    // cout << "metadata_pq_issue: " << metadata_pq_issue << endl;
    cout << "metadata_conf_change: " << conf_dec_retain <<endl;
    cout << "metadata_conf_change_modify: " << conf_dec_update <<endl;
    cout << "metadata_conf_same: " << conf_inc <<endl;
    cout << "metadata_recursive_conf: " << conf_dec_recursive << endl;
    cout << "metadata_data_conf: " << conf_dec_data << endl;
    cout << "metadata_other_conf: " << conf_dec_other << endl;
    if (trigger_count) cout << "avg_assoc: " << (float)total_assoc / trigger_count <<endl;

    cout << "[Collect Entry Number]: " << endl;
    cout << "recursive entry: " << trigger_addr_recursive.size() << endl;
    cout << "data entry: " << trigger_addr_data.size() << endl;
    cout << "other entry: " << trigger_addr_other.size() << endl;

    cout << "trigger addr: " << trigger_addr_all.size() << endl;

    cout << "[Collect Trigger Counter]: " << endl;
    cout << "trigger recursive count: " << trigger_cnt_recursive << endl;
    cout << "trigger data count: " << trigger_cnt_data << endl;
    cout << "trigger other count: " << trigger_cnt_other << endl;

    cout << "same_addr: " << same_addr <<endl;
    cout << "new_addr: " << new_addr <<endl;
    cout << "new_stream: " << new_stream <<endl;
    cout << "spatial: " << spatial << endl;
    cout << "temporal: " << temporal << endl;

    cout << "metadata_conf_change_ip: " << ip_conf_dec_retain <<endl;
    cout << "metadata_conf_same_ip: " << ip_conf_inc <<endl;

    on_chip_data.print_stats();
}

