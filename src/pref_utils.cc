#include "prefetch.h"
#include "cache.h"

#include "champsim_constants.h"
#include "memory_data.h"
#include "instruction.h"

#define PRINT_CYCLE 32696

#define ISQ_WRITE_PORT 4

#define nPRINT_ICT_INSERT
#define PRINT_STRIDE_INSERT XF_DEBUG
#define PRINT_STRIDE_UPDATE XF_DEBUG
#define nCHECK_MEM_READ

#define nPRINT_OPERATE_TRACE
#define nPRINT_PERF_TRACE
#define nPRINT_MEM_TRACE

extern map<uint64_t, uint64_t> consumer_map;
extern uint64_t isq_write_num;

using namespace std;

extern bool start_print;
extern DCT dct[NUM_CPUS];
extern AGQ agq[NUM_CPUS];
extern pt_format_t pt[NUM_CPUS][64];
extern MEMORY_DATA mem_data[NUM_CPUS];

extern uint8_t warmup_complete[NUM_CPUS];

uint64_t l1_encode_metadata(uint8_t dest_reg, uint8_t size, bool unsign_ext){
	uint64_t metadata = 0;
    metadata = dest_reg;
    metadata = metadata | (size << 8);
    metadata = metadata | (unsign_ext << 16);
    return metadata;
}

uint64_t successor_num[25] = {};

uint8_t l1_decode_size(uint64_t metadata){
    uint8_t size = (metadata >> 8) & 0xff;
    return size;
}

bool l1_decode_unsign_ext(uint64_t metadata){
    bool unsign_ext = (metadata >> 16) & 0b1;
    return unsign_ext;
}

uint8_t l1_decode_dest_reg(uint64_t metadata){
    uint8_t dest_reg = metadata & 0xff;
    return dest_reg;
}
