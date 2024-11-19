#ifndef _BERTI_H_
#define _BERTI_H_

#include "cache.h"
#include "champsim_constants.h"

#include <algorithm>
#include <vector>
#include <tuple>
#include <queue>
#include <cmath>
#include <map>

#include <stdlib.h>
#include <time.h>

// notify_prefetch() is called by the cache to notify the prefetcher of issuing prefetch
#define nENABLE_BERTI

// Berti defines
# define LATENCY_TABLE_SIZE           (L1D_MSHR_SIZE + 16 + 64)
# define HISTORY_TABLE_SET            (8)
# define HISTORY_TABLE_WAY            (16)
# define TABLE_SET_MASK               (0x7)

# define DELTA_TABLE_SIZE             (16)
# define DELTA_TABLE_STRIDE_SIZE      (16)

// Mask
# define MAX_HISTORY_IP               (8)
# define MAX_PF                       (16)
# define MAX_PF_LAUNCH                (12)
# define STRIDE_MASK                  (12)
# define IP_MASK                      (0x3FF)
# define TIME_MASK                    (0xFFFF)
# define LAT_MASK                     (0xFFF)
# define ADDR_MASK                    (0xFFFFFF)

// Confidence
# define CONFIDENCE_MAX               (16) // 6 bits
# define CONFIDENCE_INC               (1)  // 6 bits
# define CONFIDENCE_INIT              (1)  // 6 bits
# define CONFIDENCE_L1                (65) // 6 bits
# define CONFIDENCE_L2                (50) // 6 bits
# define CONFIDENCE_L3                (35) // 6 bits
# define CONFIDENCE_MIN               (8)  // 6 bits
# define MSHR_LIMIT                   (70)

// Stride rpl
// L1, L2, L3
# define R                            (0x0)
# define L1                           (0x1)
# define L2                           (0x2)
# define L3                           (0x3)

// Structs define
typedef struct latency_table {
    uint64_t addr; // Addr
    uint64_t ip;   // Ip
    uint64_t time; // Time where the miss is issued
} latency_table_t; // This struct is the latency table

typedef struct shadow_cache {
    uint64_t addr; // Address
    uint64_t lat;  // Latency
    uint8_t  pf;   // Is this accesed
} shadow_cache_t; // This struct is the shadow cache

typedef struct history_table {
    uint64_t ip;   // IP Tag
    uint64_t addr; // IP @ accessed
    uint64_t time; // Time where the line is accessed
} history_table_t; // This struct is the history table

// Confidence tuple
typedef struct Stride {
    uint64_t conf;
    int64_t stride;
    uint8_t rpl;
    float   per;
    Stride(): conf(0), stride(0), rpl(0), per(0) {};
} stride_t; 

typedef struct delta_table {
    stride_t *stride;
    uint64_t conf;
    uint64_t total_used;
} delta_table_t; // This struct is the delta table

// Auxiliary latency table functions
void latency_table_init(uint32_t cpu);
uint8_t latency_table_add(uint64_t line_addr, uint64_t tag, uint32_t cpu);
uint8_t latency_table_add(uint64_t line_addr, uint64_t tag, uint32_t cpu, uint64_t cycle);
uint64_t latency_table_del(uint64_t line_addr, uint32_t cpu);
uint64_t latency_table_get_ip(uint64_t line_addr, uint32_t cpu);

// Shadow cache
void shadow_cache_init(uint32_t cpu);
uint8_t shadow_cache_add(uint32_t cpu, uint32_t set, uint32_t way, uint64_t line_addr, uint8_t pf, uint64_t latency);
uint8_t shadow_cache_get(uint32_t cpu, uint64_t line_addr);
uint8_t shadow_cache_reset_pf(uint32_t cpu, uint64_t line_addr);
uint8_t shadow_cache_is_pf(uint32_t cpu, uint64_t line_addr);

// Auxiliar history table functions
void history_table_init(uint32_t cpu);
void history_table_add(uint64_t ip, uint32_t cpu, uint64_t addr);
uint16_t history_table_get(uint32_t cpu, uint32_t latency, uint64_t cycle, uint64_t ip, uint64_t act_addr, uint64_t addr[HISTORY_TABLE_WAY]);

// Auxiliar delta table functions
void delta_table_add(uint64_t ip, uint32_t cpu, int64_t stride);
uint8_t delta_table_get(uint64_t ip, uint32_t cpu, stride_t res[MAX_PF]);
void delta_table_increase_conf_ip(uint64_t ip, uint32_t cpu);

void find_and_update(uint32_t cpu, uint64_t latency, uint64_t ip, uint64_t cycle, uint64_t line_addr);

#endif
