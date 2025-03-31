#include "ooo_cpu.h"
#include "cache.h"
#include "prefetch.h"
#include "memory_data.h"
#include "adatp.h"

extern MEMORY_DATA mem_data[NUM_CPUS];

extern AdaTP adatp[NUM_CPUS];
extern AdaTP_MetaData_OnChip adatp_metadata_onchip[NUM_CPUS];
extern AdaTPConfig adatp_config[NUM_CPUS];

void AdaTP::init(AdaTPConfig* config) {
    cpu = config->cpu;

    criticality_mode = config->criticality_mode;
    pattern_train_enable = config->pattern_train_enable;
    pattern_train_dynamic_enable = config->use_dynamic_pattern ? false : pattern_train_enable;
    use_dynamic_threshold = config->use_dynamic_threshold;
    use_dynamic_pattern = config->use_dynamic_pattern;
    use_dynamic_assoc = config->use_dynamic_assoc;

    critical_conf_threshold = config->critical_conf_threshold;
    critical_conf_max = config->critical_conf_max;
    criticality_threshold = config->criticality_threshold;

    training_table_size = config->training_table_size;
    missing_status_size = config->missing_status_size;
    missing_status_ip_size = config->missing_status_ip_size;
    set_dueller_size = config->set_dueller_size;

    cache_sets = config->cache_sets;
    cache_ways = config->cache_ways;
    meta_sub_ways = config->meta_sub_ways;

    FILTER_SIZE = 16;

    target_assoc = config->assoc;
    gen = std::mt19937(0xDEEDBEEF);
    for (int i = 0; i <= cache_sets; ++i) {
        random_set.push_back(i);
    }
    std::shuffle(random_set.begin(), random_set.end(), gen);

    set_dueller.resize(set_dueller_size);
    for (int i = 0; i < set_dueller_size; i++){
        set_dueller[i].sample_index = random_set[i];
        set_dueller[i].sample_sub_index = std::uniform_int_distribution<uint64_t>(0, meta_sub_ways-1)(gen);
        set_dueller[i].cache_addrs.resize(cache_ways,0);
        set_dueller[i].cache_scores.resize(cache_ways,0);
        set_dueller[i].meta_addrs.resize(cache_ways,0);
        set_dueller[i].meta_scores.resize(cache_ways,0);
    }
    cache_cnt.resize(cache_ways);
    meta_cnt.resize(cache_ways);
}

void AdaTP::adatp_cache_miss(uint64_t ip, uint64_t addr) {
    addr = addr >> 6;
    assert(missing_status.size() <= missing_status_size);
    for (auto& entry : missing_status) {
        if (entry.addr == addr) {
            if (entry.ip.size() < missing_status_ip_size) {
                entry.ip.push_back(ip);
            }
            return;
        }
    }
    missing_status.push_back({addr, {ip}, 0});
}

void AdaTP::adatp_cache_fill(uint64_t addr, uint64_t current_cycle, uint8_t prefetch) {
    addr = addr >> 6;
    auto it = std::find_if(missing_status.begin(), missing_status.end(), [addr](const MissingStatusEntry& entry) { return entry.addr == addr; });
    if (it != missing_status.end()) {
        for (auto& ip : it->ip) {
            if (!prefetch) {
                training_table_update_on_refill(ip, addr, it->criticality, current_cycle);
            }
        }
        missing_status.erase(it);
    }
}

void AdaTP::adatp_cycle_op() {
    for (auto& entry : missing_status) {
        if (criticality_mode == 1) {
            entry.criticality += (missing_status_size / missing_status.size());
        }
    }
}

void AdaTP::adatp_issue_op() {
    for (auto& entry : missing_status) {
        if (criticality_mode == 2) {
            entry.criticality += 1;
        }
    }
}

void AdaTP::training_table_update_on_refill(uint64_t ip, uint64_t addr, uint64_t criticality, uint64_t current_cycle) {
    auto it = std::find_if(training_table.begin(), training_table.end(), [ip](const TrainingEntry& entry) { return entry.ip == ip; });
    if (it != training_table.end()) {
        if (criticality_mode == 0){
            it->critical_conf = critical_conf_max;
        }
        else if (criticality > criticality_threshold) {
            it->critical_conf =  it->critical_conf + criticality / (criticality_threshold + 100) + 1;
            if (it->critical_conf > critical_conf_max) {
                it->critical_conf = critical_conf_max;
            }
        }
        else if (it->critical_conf > 0) {
            it->critical_conf--;
        }
        it->occur_cnt++;
        it->critical_sum += criticality;
    } 
    else {
        if (training_table.size() >= training_table_size) {
            uint64_t min_replace_score = current_cycle;
            auto min_replace_it = training_table.begin();
            for (auto it = training_table.begin(); it != training_table.end(); it++) {
                if (it->replace_score < min_replace_score) {
                    min_replace_score = it->replace_score;
                    min_replace_it = it;
                }
            }
            training_table.erase(min_replace_it);
        }
        auto new_entry = new TrainingEntry;
        new_entry->ip = ip;
        new_entry->last_addr = addr;
        new_entry->occur_cnt = 1;
        new_entry->critical_sum = criticality;
        new_entry->critical_conf = 0;
        new_entry->addr1 = addr;
        new_entry->addr2 = 0;
        new_entry->addr_cnt = 0;
        new_entry->wait_cnt = 0;
        new_entry->reuse_distance = 0;
        new_entry->pattern_conf = 0;
        new_entry->replace_score = current_cycle;
        training_table.push_back(*new_entry);
    }
}

void AdaTP::training_table_update_on_access(uint64_t ip, uint64_t addr, bool cache_hit, uint64_t current_cycle, uint64_t current_inst) {
    addr = addr >> 6;
    if(!cache_hit) {
        set_dueller_check(addr, false, false, current_cycle);
    }
    set_dueller_adjust(current_inst);

    total_threshold += criticality_threshold;
    total_pattern_enable += pattern_train_dynamic_enable;

    auto it = std::find_if(training_table.begin(), training_table.end(), [ip](const TrainingEntry& entry) { return entry.ip == ip; });
    if (it != training_table.end()) {
        it->replace_score = current_cycle;
        if (addr == it->last_addr) {
            return;
        }

        if (it->pattern_conf >= 1 || !pattern_train_dynamic_enable || !pattern_train_enable) {
            if (!cache_hit && it->critical_conf >= critical_conf_threshold){
                adatp_metadata_onchip[cpu].insert(ip, it->last_addr, addr, current_cycle);
                adatp_metadata_onchip[cpu].access(ip, addr, current_cycle);
                set_dueller_check(it->last_addr, true, true, current_cycle);
            }
            else if (cache_hit && it->critical_conf >= critical_conf_threshold){
                adatp_metadata_onchip[cpu].access(ip, addr, current_cycle);
                set_dueller_check(it->last_addr, true, true, current_cycle);
            }
            else {
                adatp_metadata_onchip[cpu].access(ip, addr, current_cycle);
                set_dueller_check(it->last_addr, true, false, current_cycle);
            }
        }

        it->last_addr = addr;

        if (it->addr2 == 0){
            it->addr2 = addr;
        }
        else if (addr == it->addr1) {
            it->wait_cnt = 1;
        }
        else if (it->wait_cnt > 0 && addr == it->addr2) {
            it->reuse_distance = it->addr_cnt + it->wait_cnt;
            if (it->reuse_distance > 1024) it->pattern_conf = it->pattern_conf < 32 ? it->pattern_conf + 1 : 32;
            it->addr1 = addr;
            it->addr2 = 0;
            it->addr_cnt = 0;
            it->wait_cnt = 0;
        }
        else if (it->wait_cnt > 0) {
            it->wait_cnt++;
        }
        else if (it->wait_cnt >= 256) {
            if (it->pattern_conf>0) it->pattern_conf--;
            it->addr1 = addr;
            it->addr2 = 0;
            it->addr_cnt = 0;
            it->wait_cnt = 0;
        }
        else if (it->addr_cnt >= 262144){
            if (it->pattern_conf>0) it->pattern_conf--;
            it->addr1 = addr;
            it->addr2 = 0;
            it->addr_cnt = 0;
            it->wait_cnt = 0;
        }
        else {
            it->addr_cnt++;
        }
    }
}

void AdaTP::set_dueller_check(uint64_t addr, bool meta, bool meta_write, uint64_t current_cycle){
    if (meta) {
        for (uint64_t i = 0; i < set_dueller_size; i++){
            auto set_dueller_entry = &set_dueller[i];
            if (addr % cache_sets != set_dueller_entry->sample_index){
                continue;
            }
            if ((addr / cache_sets) % meta_sub_ways != set_dueller_entry->sample_sub_index){
                continue;
            }
            bool hit = false;
            uint64_t hit_way;
            uint64_t hit_score;
            for (uint64_t j = 0; j < cache_ways; j++) {
                if (addr == set_dueller_entry->meta_addrs[j]){
                    hit = true;
                    hit_way = j;
                    hit_score = set_dueller_entry->meta_scores[j];
                    break;
                }
            }
            if (hit) {
                uint64_t newer_way = 0;
                for (uint64_t j = 0; j < cache_ways; j++){
                    if (set_dueller_entry->meta_scores[j] > hit_score) {
                        newer_way++;
                    }
                }
                for (uint64_t j = 0; j <= newer_way; j++){
                    meta_cnt[j] += 1;
                }
                set_dueller_entry->meta_scores[hit_way] = current_cycle;
            }
            else if (meta_write) {
                uint64_t replace_way = 0;
                uint64_t replace_cycle = set_dueller_entry->meta_scores[0];
                for (uint64_t j = 1; j < cache_ways; j++) {
                    if (set_dueller_entry->meta_scores[j] < replace_cycle) {
                        replace_way = j;
                        replace_cycle = set_dueller_entry->meta_scores[j];
                    }
                }
                set_dueller_entry->meta_addrs[replace_way] = addr;
                set_dueller_entry->meta_scores[replace_way] = current_cycle;
            }
        }
    }
    else {
        cache_access_cnt++;
        for (uint64_t i = 0; i < set_dueller_size; i++){
            auto set_dueller_entry = &set_dueller[i];
            if (addr % cache_sets != set_dueller_entry->sample_index){
                continue;
            }
            bool hit = false;
            uint64_t hit_way;
            uint64_t hit_score;
            for (uint64_t j = 0; j < cache_ways; j++) {
                if (addr == set_dueller_entry->cache_addrs[j]){
                    hit = true;
                    hit_way = j;
                    hit_score = set_dueller_entry->cache_scores[j];
                    break;
                }
            }
            if (hit) {
                uint64_t newer_way = 0;
                for (uint64_t j = 0; j < cache_ways; j++){
                    if (set_dueller_entry->cache_scores[j] > hit_score) {
                        newer_way++;
                    }
                }
                for (uint64_t j = 0; j <= newer_way; j++){
                    cache_cnt[j] += 1;
                }
                set_dueller_entry->cache_scores[hit_way] = current_cycle;
            }
            else {
                uint64_t replace_way = 0;
                uint64_t replace_cycle = set_dueller_entry->cache_scores[0];
                for (uint64_t j = 1; j < cache_ways; j++) {
                    if (set_dueller_entry->cache_scores[j] < replace_cycle) {
                        replace_way = j;
                        replace_cycle = set_dueller_entry->cache_scores[j];
                    }
                }
                set_dueller_entry->cache_addrs[replace_way] = addr;
                set_dueller_entry->cache_scores[replace_way] = current_cycle;
            }
        }
    }
}

void AdaTP::set_dueller_adjust(uint64_t inst_num) {
    static uint64_t trigger_inst = AdaTP_METADATA_INTERVAL;
    if (inst_num > 0 && inst_num >= trigger_inst) {
        trigger_inst += AdaTP_METADATA_INTERVAL;
        bool critical_metadata = false;
        uint64_t cache_threshold = std::accumulate(cache_cnt.begin(), cache_cnt.end(), 0) * 0.75;
        uint64_t meta_threshold = meta_cnt[0] / 100;
        uint64_t cache_alloc_ways = 0;
        uint64_t meta_alloc_ways = 0;

        int cache_sum = 0;
        for (uint64_t i = 0; i < cache_ways; i++){
            cache_sum += cache_cnt[i];
            if (cache_sum < cache_threshold) {
                cache_alloc_ways++;
            }
            if (meta_cnt[i] > meta_threshold) {
                meta_alloc_ways++;
            }
        }

        uint64_t delta = 0;
        bool increase_delta = false;
        bool decrease_delta = false;
        if (cache_alloc_ways + meta_alloc_ways > cache_ways) {
            delta = cache_alloc_ways + meta_alloc_ways - cache_ways;
            increase_delta = true;
        }
        else if (cache_alloc_ways + meta_alloc_ways < cache_ways) {
            delta = cache_ways - cache_alloc_ways - meta_alloc_ways;
            decrease_delta = true;
        }

        if (use_dynamic_assoc){
            if (cache_alloc_ways + meta_alloc_ways < cache_ways) {
                if (meta_alloc_ways > target_assoc)
                    target_assoc = meta_alloc_ways;
                else if (trigger_inst == 2*AdaTP_METADATA_INTERVAL) {
                    target_assoc = meta_alloc_ways;
                }
                else if (target_assoc + cache_alloc_ways > cache_ways)
                    target_assoc = cache_ways - cache_alloc_ways;
            }
            else {
                target_assoc = cache_ways - cache_alloc_ways;
            }
	    if (target_assoc >= 12) {
		    target_assoc = 12;
	    }
        }
        if (use_dynamic_threshold){
            if (increase_delta && !critical_metadata) {
                criticality_threshold += (delta * 400);       
                if (use_dynamic_pattern) {
                    pattern_train_dynamic_enable = true;
                }
            }
            if (decrease_delta) {
                if (criticality_threshold > (delta * 200)) {
                    criticality_threshold -= (delta * 200);
                }
                else {
                    criticality_threshold = 0;
                    if (use_dynamic_pattern) { 
                        pattern_train_dynamic_enable = false;
                    }
                }
            }
        }
        printf("cache_alloc_ways: %lu, meta_alloc_ways: %lu, target assoc: %ld\n", cache_alloc_ways, meta_alloc_ways, target_assoc);
        printf("cache_cnt: ");
        for (uint64_t i = 0; i < cache_ways; i++){
            printf("%lu ", cache_cnt[i]);
        }
        printf("\n");
        printf("meta_cnt: ");
        for (uint64_t i = 0; i < cache_ways; i++){
            printf("%lu ", meta_cnt[i]);
        }
        printf("\n");
        printf("delta %d, dynamic threshold: %d, dynamic pattern: %d\n", delta, criticality_threshold, pattern_train_dynamic_enable);
        std::shuffle(random_set.begin(), random_set.end(), gen);
        for (int i = 0; i < set_dueller_size; i++){
            set_dueller[i].sample_index = random_set[i];
            set_dueller[i].sample_sub_index = std::uniform_int_distribution<uint64_t>(0, meta_sub_ways-1)(gen);
        }
        for (uint64_t i = 0; i < cache_ways; i++){
            cache_cnt[i] = 0;
            meta_cnt[i] = 0;
        }
    }
}

bool AdaTP::recent_prefetch(uint64_t addr) {
    bool flag = false;
    for (auto& prefetch_addr : prefetch_addrs) {
        if (addr == prefetch_addr) {
            flag = true;
        }
    }
    if (prefetch_addrs.size() >= FILTER_SIZE) {
        prefetch_addrs.pop_front();
    }
    prefetch_addrs.push_back(addr);
    return flag;
}

uint32_t AdaTP::get_target_assoc() {
    return target_assoc;
}

void AdaTP::print_stats() {
    printf("----training table----\n");
    for (auto& entry : training_table) {
        if (entry.occur_cnt > 0)
            printf("ip: %lx, average: %lu, criticality conf: %ld, reuse: %lu, pattern_conf: %lu\n", entry.ip, entry.critical_sum/entry.occur_cnt, entry.critical_conf, entry.reuse_distance, entry.pattern_conf);
    }
    printf("----training stats----\n");
    printf("average threshold: %lf, average pattern enable: %lf\n", 1.0*total_threshold / cache_access_cnt, 1.0*total_pattern_enable / cache_access_cnt);
}

/****** MetaData_OnChip ******/ 
void AdaTP_MetaData_OnChip::init(AdaTPConfig* config) {
    cpu = config->cpu;
    entries.resize(config->num_sets);

    assoc = config->assoc;
    num_sets = config->num_sets;

    RQ_SIZE = config->readq_size;
    WQ_SIZE = config->writeq_size;
    PQ_SIZE = config->prefq_size;
    metadata_delay = config->metadata_delay;

    lookahead = config->lookahead;

    assert(readQ.size() == 0);
    assert(writeQ.size() == 0);
    assert(prefQ.size() == 0);

    assert((num_sets & (num_sets - 1)) == 0); // Ensure that num_sets is a power of 2
    index_mask = num_sets - 1;
    index_length = lg2(num_sets);
}

bool AdaTP_MetaData_OnChip::RQ_is_full() {
    return readQ.size() == RQ_SIZE;
}

bool AdaTP_MetaData_OnChip::WQ_is_full() {
    return writeQ.size() == WQ_SIZE;
}

bool AdaTP_MetaData_OnChip::PQ_is_full() {
    return prefQ.size() == PQ_SIZE;
}

void AdaTP_MetaData_OnChip::adjust_assoc(uint32_t new_assoc) {
    assoc = new_assoc;
}

uint32_t AdaTP_MetaData_OnChip::get_assoc() {
    return assoc;
}

uint64_t AdaTP_MetaData_OnChip::get_set_id(uint64_t addr) {
    uint64_t set_id = addr & index_mask;
    assert(set_id < num_sets);
    return set_id;
} 

uint64_t AdaTP_MetaData_OnChip::get_tag(uint64_t addr) {
    uint64_t tag = addr >> index_length;
    return tag;
}

void AdaTP_MetaData_OnChip::add_rq(uint64_t ip, uint64_t addr, uint64_t cycle, uint64_t depth){
    if (RQ_is_full()) {
        RQ_FULL_CNT++;
        readQ.pop_front();
    }
    adatp_read_entry new_entry;
    new_entry.ip = ip;
    new_entry.addr = addr;
    new_entry.cycle = cycle;
    new_entry.depth = depth;
    readQ.push_back(new_entry);
}

void AdaTP_MetaData_OnChip::add_wq(AdaTPMetaDataEntry* entry, bool first_write, uint64_t index, uint64_t way, uint64_t addr1, uint64_t addr2, uint64_t conf, uint64_t cycle) {
    if (WQ_is_full()) {
        WQ_FULL_CNT++;
        writeQ.pop_front();
    }
    trigger_addr.insert(addr1);
    adatp_write_entry new_entry;
    new_entry.entry = entry;
    new_entry.first_write = first_write;
    new_entry.index = index;
    new_entry.way = way;
    new_entry.addr2 = addr2;
    new_entry.conf = conf;
    new_entry.cycle = cycle;
    writeQ.push_back(new_entry);
}

void AdaTP_MetaData_OnChip::add_pq(uint64_t ip, uint64_t addr, uint64_t cycle) {
    if (PQ_is_full()) {
        PQ_FULL_CNT++;
        prefQ.pop_front();
    }
    adatp_prefetch_entry new_entry;
    new_entry.ip = ip;
    new_entry.addr = addr;
    new_entry.cycle = cycle;
    prefQ.push_back(new_entry);
}

void AdaTP_MetaData_OnChip::read(uint64_t num, uint64_t current_cycle) {
    for (uint64_t i = 0; i < num; i++) {
        if (readQ.size() == 0) {
            return;
        }
        auto read = readQ.front();
        uint64_t read_ip = read.ip;
        uint64_t read_addr = read.addr;
        uint64_t cycle = read.cycle;
        uint64_t depth = read.depth;
        if (current_cycle >= cycle + metadata_delay){
            readQ.pop_front();

            uint64_t prefetch_addr = 0;
            uint64_t set_id = get_set_id(read_addr);
            uint64_t tag = get_tag(read_addr);

            map<uint64_t, AdaTPMetaDataEntry>& entry_map = entries[set_id];
            map<uint64_t, AdaTPMetaDataEntry>::iterator it = entry_map.find(tag);

            if (it != entry_map.end()) {
                prefetch_addr = it->second.next_addr;
            } else {
                continue;
            }

            if(prefetch_addr != 0) {
                add_pq(read_ip, prefetch_addr, current_cycle);
                if (depth < lookahead) {
                    add_rq(read_ip, prefetch_addr, current_cycle, depth+1);
                }
            }
        }
    }
}

void AdaTP_MetaData_OnChip::write(uint64_t num, uint64_t current_cycle) {
    for (uint64_t i = 0; i < num; i++) {
        if (writeQ.size() == 0) {
            return;
        }
        adatp_write_entry write = writeQ.front();
        if (current_cycle >= write.cycle + metadata_delay && !write.first_write) {
            write.entry->next_addr = write.addr2;
            write.entry->conf = write.conf;
            writeQ.pop_front();
        }
    }
}

adatp_prefetch_entry AdaTP_MetaData_OnChip::pref() {
    if(prefQ.size() == 0) {
        return {0,0};
    }
    else {
        adatp_prefetch_entry ret = prefQ.front();
        prefQ.pop_front();
        return ret;
    }
}

void AdaTP_MetaData_OnChip::insert(uint64_t ip, uint64_t addr1, uint64_t addr2, uint64_t cycle) {
    if (assoc == 0) {
        return;
    }

    uint64_t set_id = get_set_id(addr1);
    uint64_t tag    = get_tag(addr1);
    map<uint64_t, AdaTPMetaDataEntry>& entry_map = entries[set_id];
    map<uint64_t, AdaTPMetaDataEntry>::iterator it = entry_map.find(tag);

    if (it != entry_map.end()) {
        bool need_write = true;
        uint64_t new_addr2 = 0;
        uint64_t new_conf = 0;

        if (addr2 == it->second.next_addr) {
            new_addr2 = it->second.next_addr;
            new_conf = (it->second.conf == 3) ? 3 : it->second.conf+1;
            if(it->second.conf == 3) {
                need_write = false;
            }
        } else {
            if (it->second.conf == 0) {
                new_addr2 = addr2;
                new_conf = 2; 
            }
            else {
                new_addr2 = it->second.next_addr;
                new_conf = it->second.conf-1;
            }
        }
        if(need_write){
            // no need to assign index & way bcuz dirty data is alr written back
            add_wq(&(it->second), false, 0, 0, addr1, new_addr2, new_conf, cycle);
        }
    } else {
        bool new_alloc = false;
        uint32_t way_num = 0;

        if (entry_map.size() < assoc){
            new_alloc = true;
            way_num = entry_map.size();
        }
        while(entry_map.size() >= assoc) {
            uint64_t way_num = (rand() % assoc);
            auto it = entry_map.begin();
            std::advance(it, way_num);
            auto write_it = find_if(writeQ.begin(), writeQ.end(), [it](const adatp_write_entry& entry) { return entry.entry == &(it->second); });
            if (write_it != writeQ.end()) {
                writeQ.erase(write_it);
            }
            entry_map.erase(it);
        }

        AdaTPMetaDataEntry entry;
        entry.next_addr = addr2;
        entry.conf = 2;
        entry_map[tag] = entry;

        add_wq(&(entry_map[tag]), new_alloc, set_id / 16, way_num, addr1, addr2, 2, cycle);

        assert(entry_map.size() <= AdaTP_MAX_ASSOC);
    }
}

void AdaTP_MetaData_OnChip::access(uint64_t ip, uint64_t addr, uint64_t cycle) {
    ACCESS_CNT++; ASSOC_SUM+=assoc;
    add_rq(ip, addr, cycle, 1);
}

void AdaTP_MetaData_OnChip::print_stats() {
    printf("----metadata stats----\n");
    printf("RQ_FULL_CNT: %lu\n", RQ_FULL_CNT);
    printf("WQ_FULL_CNT: %lu\n", WQ_FULL_CNT);
    printf("PQ_FULL_CNT: %lu\n", PQ_FULL_CNT);
    printf("trigger addr: %lu\n", trigger_addr.size());
    printf("average assoc: %lf\n", 1.0*ASSOC_SUM/ACCESS_CNT);
}
