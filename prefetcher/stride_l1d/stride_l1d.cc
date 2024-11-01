#include "cache.h"
#include "prefetch.h"

#define IPT_NUM 48
#define L1_STRIDE_DISTANCE 8
#define SP_CONF_MAX 3

class IPT_L1 {
    public:
        uint64_t ip = 0;
        uint64_t last_addr = 0;
        int64_t stride = 0;
        uint8_t conf = 0;

        uint8_t rplc_bits; // 3 means most recently used, 0 means least recently used
};

IPT_L1 ipt[NUM_CPUS][IPT_NUM];

uint8_t update_conf(int64_t stride, int64_t last_stride, uint8_t conf){
    uint8_t conf_ret;
    if (stride == 0){
        conf_ret = conf;
    } else if(stride == last_stride){
        conf_ret = (conf < SP_CONF_MAX)? (conf+1) : conf;
    } else{
        conf_ret = (conf > 0          )? (conf-1) : 0;
    }

    return conf_ret;
}

uint64_t encode_metadata(int stride, PREF_TYPE pref_type){
	uint64_t metadata = 0;

	if(stride > 0)
    	metadata = stride & 0x7fffffff;
	else
    	metadata = (((-1*stride) & 0x7fffffff) | 0x80000000);

	metadata = metadata | ((uint64_t)pref_type << 32);
	return metadata;
}

pair<uint64_t, uint64_t> stride_cache_operate(uint64_t cpu, uint64_t addr, uint64_t ip){
    uint64_t pf_address = 0, pf_metadata = 0;
    uint32_t hit_idx = IPT_NUM;

    for(uint32_t i = 0; i < IPT_NUM; i++){
        if(ipt[cpu][i].conf != 0 && ipt[cpu][i].ip == ip){
            hit_idx = i;
        }
    }

    if(hit_idx != IPT_NUM){
        IPT_L1 ipt_hit_item = ipt[cpu][hit_idx];
        int64_t new_stride = addr - ipt_hit_item.last_addr;
        bool ignore = new_stride==0;

        bool trigger_prefetch = ipt_hit_item.conf >= 2 && ipt_hit_item.stride != 0 &&
                            ((ipt_hit_item.stride == new_stride) ||
                             (ipt_hit_item.conf >= 3 && !ignore));

        if(trigger_prefetch){
            int64_t stride = ipt_hit_item.stride;
            uint64_t distance = L1_STRIDE_DISTANCE;
            int64_t delta = stride * distance;

            pf_address = addr + delta;
            pf_metadata = encode_metadata((int)(stride * distance), PREF_STRIDE);
        }

        if(!ignore){
            ipt[cpu][hit_idx].last_addr = addr;
            ipt[cpu][hit_idx].conf = update_conf(new_stride, ipt_hit_item.stride, ipt_hit_item.conf);

            if(ipt_hit_item.conf <= 1){
                ipt[cpu][hit_idx].stride = new_stride;
            }
        }

        for(uint32_t j = 0; j < IPT_NUM; j++){
            if(ipt[cpu][j].rplc_bits > ipt_hit_item.rplc_bits){
                ipt[cpu][j].rplc_bits --;
            }
        }
        ipt[cpu][hit_idx].rplc_bits = IPT_NUM-1;
    } else {
        uint8_t ip_idx = IPT_NUM;
        uint8_t rplc0_idx = IPT_NUM;
        uint8_t conf0_idx  = IPT_NUM;
        uint8_t conf0_rplc = IPT_NUM;

        for(uint32_t i = 0; i < IPT_NUM; i++){
            if(ipt[cpu][i].conf < 2 && ipt[cpu][i].rplc_bits < conf0_rplc){
                conf0_idx = i;
                conf0_rplc = ipt[cpu][i].rplc_bits;
            }

            if(ipt[cpu][i].ip == ip){
                ip_idx = i;
            }

            if(ipt[cpu][i].rplc_bits == 0){
                rplc0_idx = i;
            }
        }

        uint8_t rplc_idx = (ip_idx < IPT_NUM)? ip_idx : (conf0_idx < IPT_NUM) ? conf0_idx : rplc0_idx;

        ipt[cpu][rplc_idx].ip        = ip;
        ipt[cpu][rplc_idx].last_addr = addr;
        ipt[cpu][rplc_idx].conf      = 1;

        for(uint32_t j = 0; j < IPT_NUM; j++){
            if(ipt[cpu][j].rplc_bits > ipt[cpu][rplc_idx].rplc_bits){
                ipt[cpu][j].rplc_bits --;
            }
        }
        ipt[cpu][rplc_idx].rplc_bits = IPT_NUM-1;
    }

    return make_pair(pf_address, pf_metadata);
}

void CACHE::prefetcher_initialize() {
    std::cout << "L1D [Stride] prefetcher" << std::endl;

    for(uint32_t i = 0; i < IPT_NUM; i++){
        ipt[cpu][i].conf = 0;
        ipt[cpu][i].rplc_bits = i;
    }
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
    /** Stride Prefetcher */
    pair<uint64_t, uint64_t> stride = stride_cache_operate(cpu, addr, ip);
    if(stride.first != 0){
        int stride_succ = prefetch_line(stride.first, true, stride.second);
    }

    return metadata_in;
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {
    return ;
}

void CACHE::prefetcher_final_stats() {
    return ;
}
