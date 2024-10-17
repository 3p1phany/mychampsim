#include <stdio.h>

#include "cache.h"
#include "prefetch.h"
#include "cdp.h"
#include "memory_data.h"

#define nCDP_DEBUG

extern MEMORY_DATA mem_data[NUM_CPUS];

struct CDPConfig conf[NUM_CPUS];

bool stride_enable = true;
extern IPT_L1 ipt[NUM_CPUS][IPT_NUM];

void CACHE::prefetcher_initialize() {
  conf[cpu].depth = 3;
  conf[cpu].cmp_bits = 16;
  conf[cpu].filter_bits = 4;
  conf[cpu].align_bits = 4;
  conf[cpu].cmp_mask = -1LL << (40 - conf[cpu].cmp_bits);
  conf[cpu].filter_mask = (-1LL << (40 - conf[cpu].cmp_bits - conf[cpu].filter_bits)) ^ conf[cpu].cmp_mask;
  assert(conf[cpu].align_bits == 4);

  if(stride_enable){
      for(uint32_t i = 0; i < IPT_NUM; i++){
          ipt[cpu][i].conf = 0;
          ipt[cpu][i].rplc_bits = i;
      }
  }

#ifndef ENABLE_CDP
  cout << "CDP is not enabled" << endl;
  exit(1);
#endif

  std::cout << NAME << " CDP prefetcher" << std::endl;
  printf("filter mask %016lx, cmp mask %016lx\n", conf[cpu].filter_mask, conf[cpu].cmp_mask);
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) { 
  /** Stride Prefetcher */
  if(stride_enable){
      pair<uint64_t, uint64_t> stride = stride_cache_operate(cpu, addr, ip);
      if(stride.first != 0){
          int stride_succ = prefetch_line(stride.first, true, stride.second);
      }
  }

  if(hit_pref){
    #ifdef CDP_DEBUG
    printf("demand hit pref arrive addr %016lx\n", addr);
    #endif
    for (uint64_t i = 0; i < 8; i++){
      addr = addr & 0xffffffffffLL;
      uint64_t data = mem_data[cpu].read((addr & ~0x3fLL) + i * 8, SIZE_DWORD, true);
      bool cmp_bits_same =  (data & conf[cpu].cmp_mask) == (addr & conf[cpu].cmp_mask);
      bool bits_in_range = ((data & conf[cpu].cmp_mask) == 0                  && ((data & conf[cpu].filter_mask) != 0)) || 
                           ((data & conf[cpu].cmp_mask) == (conf[cpu].cmp_mask & 0xffffffffffLL) && ((data & conf[cpu].filter_mask) != conf[cpu].filter_mask));
      #ifdef CDP_DEBUG
      printf("  > data %016lx ", data);
      #endif
      if(cmp_bits_same || bits_in_range){
        uint64_t md_out = 0;
        #ifdef CDP_DEBUG
        printf("  prefetching %lx, with md %ld\n", data, md_out);
        #endif
        prefetch_line(data, true, md_out);
      }
      else{
        #ifdef CDP_DEBUG
        printf("\n");
        #endif
      }
    }
  }
  return metadata_in; 
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
  #ifdef CDP_DEBUG
  printf("refill arrive addr %016lx, prefetch %d, metadata_in %ld\n", addr, prefetch, metadata_in);
  #endif
  if(!prefetch || (prefetch && metadata_in <= conf[cpu].depth)){
    for (uint64_t i = 0; i < 8; i++){
      addr = addr & 0xffffffffffLL;
      uint64_t data = mem_data[cpu].read((addr & ~0x3fLL) + i * 8, SIZE_DWORD, true);
      bool cmp_bits_same =  (data & conf[cpu].cmp_mask) == (addr & conf[cpu].cmp_mask);
      bool bits_in_range = ((data & conf[cpu].cmp_mask) == 0                  && ((data & conf[cpu].filter_mask) != 0)) || 
                           ((data & conf[cpu].cmp_mask) == (conf[cpu].cmp_mask & 0xffffffffff) && ((data & conf[cpu].filter_mask) != conf[cpu].filter_mask));
      #ifdef CDP_DEBUG
      printf("  > data %016lx ", data);
      #endif
      if(cmp_bits_same || bits_in_range){
        uint64_t md_out = prefetch ? metadata_in + 1 : 0;
        #ifdef CDP_DEBUG
        printf("  prefetching %lx, with md %ld\n", data, md_out);
        #endif
        prefetch_line(data, true, md_out);
      }
      else{
        #ifdef CDP_DEBUG
        printf("\n");
        #endif
      }
    }
  }
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}

void cdp_handle_prefetch(uint32_t cpu, CACHE* cache, uint64_t ip, uint64_t addr, uint64_t md){
  if (md <= conf[cpu].depth){
    #ifdef CDP_DEBUG
    printf("pref hit cache arrive addr %016lx\n", addr);
    #endif
    for (uint64_t i = 0; i < 8; i++){
      addr = addr & 0xffffffffffLL;
      uint64_t data = mem_data[cpu].read((addr & ~0x3fLL) + i * 8, SIZE_DWORD, true);
      bool cmp_bits_same =  (data & conf[cpu].cmp_mask) == (addr & conf[cpu].cmp_mask);
      bool bits_in_range = ((data & conf[cpu].cmp_mask) == 0                  && ((data & conf[cpu].filter_mask) != 0)) || 
                           ((data & conf[cpu].cmp_mask) == (conf[cpu].cmp_mask & 0xffffffffffLL) && ((data & conf[cpu].filter_mask) != conf[cpu].filter_mask));
      #ifdef CDP_DEBUG
      printf("  > data %016lx ", data);
      #endif
      if(cmp_bits_same || bits_in_range){
        uint64_t md_out = md + 1;
        #ifdef CDP_DEBUG
        printf("  prefetching %lx, with md %ld\n", data, md_out);
        #endif
        cache->prefetch_line(data, true, md_out);
      }
      else{
        #ifdef CDP_DEBUG
        printf("\n");
        #endif
      }
    }
  }
}