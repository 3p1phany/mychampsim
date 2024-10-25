/*************************************************************************************************************************
Authors: 
Samuel Pakalapati - samuelpakalapati@gmail.com
Biswabandan Panda - biswap@cse.iitk.ac.in
Nilay Shah - nilays@iitk.ac.in
Neelu Shivprakash kalani - neeluk@cse.iitk.ac.in 
**************************************************************************************************************************/

/*************************************************************************************************************************
Source code of "Bouquet of Instruction Pointers: Instruction Pointer Classifier-based Spatial Hardware Prefetching" 
appeared (to appear) in ISCA 2020: https://www.iscaconf.org/isca2020/program/. The paper is available at 
https://www.cse.iitk.ac.in/users/biswap/IPCP_ISCA20.pdf. The source code can be used with the ChampSim simulator 
https://github.com/ChampSim . Note that the authors have used a modified ChampSim that supports detailed virtual 
memory sub-system. Performance numbers may increase/decrease marginally
based on the virtual memory-subsystem support. Also for PIPT L1-D caches, this code may demand 1 to 1.5KB additional 
storage for various hardware tables.     
**************************************************************************************************************************/

#include "ooo_cpu.h"
#include "cache.h"
#include <vector>

/**************************************************************************************************************************
Note that variables uint64_t pref_useful[NUM_CPUS][6], pref_filled[NUM_CPUS][6], pref_late[NUM_CPUS][6]; are to be declared 
as members of the CACHE class in inc/cache.h and modified in src/cache.cc where the second level index denotes the IPCP prefetch 
class type for each variable which can be extracted through pf_metadata. A prefetch is considered in pref_useful when a cache 
blocks gets a hit and its prefetch bit is set. Whenever a cache block is filled (in handle_fill) and its type is prefetch, 
pref_fill is incremented. The pref_late variable is modified whenever a demand request merges with a prefetch request or 
vice versa in the cache's MSHR as, if the prefetch would've been on time, the demand request would've hit in the cache.
****************************************************************************************************************************/ 

#define NUM_IP_INDEX_BITS 6
#define NUM_IP_TABLE_L1_ENTRIES 64
#define NUM_IP_TAG_BITS 9 	
#define NUM_PAGE_TAG_BITS 2

#define NUM_SIG_BITS 7                          // num of bits in signature        
#define NUM_CSPT_ENTRIES 128                    // = 2^NUM_SIG_BITS                                                               

#define MAX_POS_NEG_COUNT 64				    // 6-bit saturating counter
#define NUM_OF_LINES_IN_REGION 32			    // 32 cache lines in 2KB region
#define REGION_OFFSET_MASK 0x1F				    // 5-bit offset for 2KB region
#define NUM_RST_ENTRIES 8 

#define NUM_OF_RR_ENTRIES 32
#define RR_TAG_MASK 0xFFF				        // 12 bits of prefetch line address are stored in recent request filter

#define S_TYPE 1                                            // stream
#define CS_TYPE 2                                           // constant stride
#define CPLX_TYPE 3                                         // complex stride
#define NL_TYPE 4                                           // next line

class IP_TABLE_L1 {
    public:
    uint64_t ip_tag;					    
    uint64_t last_vpage;                    // last page seen by IP 
    uint64_t last_line_offset;              // last cl offset in the 4KB page 
    int64_t last_stride;                    // last stride observed 
    uint16_t ip_valid;		                // valid bit
    int conf;                               // CS confidence 
    uint16_t signature;                     // CPLX signature 
    uint16_t str_dir;                       // stream direction 
    uint16_t str_valid;                     // stream valid 
    uint16_t pref_type;                     // pref type or class for book-keeping purposes.

    IP_TABLE_L1 () {
        ip_tag = 0;
        last_vpage = 0;
        last_line_offset = 0;
        last_stride = 0;
        ip_valid = 0;
        signature = 0;
        conf = 0;
        str_dir = 0;
        str_valid = 0;
        pref_type = 0;
    };
};

/*	IP TABLE STORAGE OVERHEAD: 288 Bytes

	Single Entry:

	FIELD					STORAGE (bits)
	
	IP tag					9 
	last page				2
	last line offset	    6
	last stride				7 	(6 bits stride + 1 sign bit)
	IP valid				1
	confidence				2
	signature				7
	stream direction	    1
	stream valid		    1
	
	Total 					36

	Full Table Storage Overhead: 

	64 entries * 36 bits = 2304 bits = 288 Bytes

	NOTE: The field prefetch class is used for book-keeping purposes. 
*/

class CONST_STRIDE_PRED_TABLE {
public:
    int stride;
    int conf;

    CONST_STRIDE_PRED_TABLE () {
        stride = 0;
        conf = 0;
    };        
};

/*	CONSTANT STRIDE STORAGE OVERHEAD: 144 Bytes

	Single Entry:

	FIELD					STORAGE (bits)
	
	stride					7	(6 bits stride + 1 sign bit)
	confidence 				2

	Total					9

	Full Table Storage Overhead:
	
	128 entries * 9 bits = 1152 bits = 144 Bytes

*/

class REGION_STREAM_TABLE {
    public:
        uint64_t region_id;		
        uint64_t tentative_dense;			// tentative dense bit
        uint64_t trained_dense;				// trained dense bit
        uint64_t pos_neg_count;				// positive/negative stream counter
        uint64_t dir;					    // direction of stream - 1 for +ve and 0 for -ve
        uint64_t lru;					    // lru for replacement
        uint8_t line_access[NUM_OF_LINES_IN_REGION];	// bit vector to store which lines in the 2KB region have been accessed
        REGION_STREAM_TABLE () {
            region_id=0;
            tentative_dense=0;
            trained_dense=0;
            pos_neg_count=MAX_POS_NEG_COUNT/2;
            dir=0;
            lru=0;
            for(int i=0; i < NUM_OF_LINES_IN_REGION; i++)
                line_access[i]=0;
        };
};

/*	REGION STREAM TABLE STORAGE OVERHEAD:

	Single Entry: 

	FIELD					STORAGE (bits)

	region id				    3
	tentative dense				1
	trained dense				1
	positive/negative count		6
	direction				    1
	lru 					    3
	bit vector line access		32	(for 2KB region)

	Total					    47

	Full Table Storage Overhead:

	8 entries * 47 bits = 376 bits = 47 Bytes

*/

IP_TABLE_L1 trackers_l1[NUM_CPUS][NUM_IP_TABLE_L1_ENTRIES];
CONST_STRIDE_PRED_TABLE CSPT_l1[NUM_CPUS][NUM_CSPT_ENTRIES];
REGION_STREAM_TABLE rstable [NUM_CPUS][NUM_RST_ENTRIES];
vector<uint64_t> recent_request_filter;		// to filter redundant prefetch requests 

/* 	RECENT REQUEST FILTER STORAGE OVERHEAD: 48 Bytes

	FIELD					STORAGE (bits)
	
	Tag					12

	Total Storage Overhead:

	32 entries * 12 bits = 384 bits = 48 Bytes

*/

int acc_filled[NUM_CPUS][5];
int acc_useful[NUM_CPUS][5];
int acc[NUM_CPUS][5];
int prefetch_degree[NUM_CPUS][5];

uint64_t num_misses[NUM_CPUS];
uint64_t num_access[NUM_CPUS];
float mpki[NUM_CPUS] = {0};
int spec_nl[NUM_CPUS] = {0}, flag_nl[NUM_CPUS] = {0};

/* update_sig_l1: 7 bit signature is updated by performing a left-shift of 1 bit on the old signature and xoring the outcome with the delta*/
uint16_t update_sig_l1(uint16_t old_sig, int delta){
    uint16_t new_sig = 0;
    int sig_delta = 0;

    // 7-bit sign magnitude form, since we need to track deltas from +63 to -63
    sig_delta = (delta < 0) ? (((-1) * delta) + (1 << 6)) : delta;
    new_sig = ((old_sig << 1) ^ sig_delta) & ((1 << NUM_SIG_BITS)-1);                     

    return new_sig;
}

/* encode_metadata: The stride, prefetch class type and speculative nl fields are encoded in the metadata. */
uint32_t ipcp_encode_metadata(int stride, uint16_t type, int spec_nl){
	uint32_t metadata = 0;

	// first encode stride in the last 8 bits of the metadata
	if(stride > 0)
        metadata = stride;
	else
        metadata = ((-1*stride) | 0b1000000);

	// encode the type of IP in the next 4 bits 			 
	metadata = metadata | (type << 8);

	// encode the speculative NL bit in the next 1 bit
	metadata = metadata | (spec_nl << 12);

	return metadata;
}

/*If the actual stride and predicted stride are equal, then the confidence counter is incremented. */
int ipcp_update_conf(int stride, int pred_stride, int conf){
    if(stride == pred_stride){             // use 2-bit saturating counter for confidence
        conf++;
        if(conf > 3)
            conf = 3;
    } else {
        conf--;
        if(conf < 0)
            conf = 0;
    }

    return conf;
}

uint64_t hash_page(uint64_t addr){
    uint64_t hash = 0;
    while(addr != 0){
        hash = hash ^ addr;
        addr = addr >> 6;
    }

    return hash & ((1 << NUM_PAGE_TAG_BITS)-1);
}

void CACHE::prefetcher_initialize() 
{
    cout << "[L1D IPCP Prefetcher]" << endl;
	cout << "IP Table Entries: " << NUM_IP_TABLE_L1_ENTRIES << endl;
	cout << "CSPT Entries: " << NUM_CSPT_ENTRIES << endl; 
	cout << "RST_ENTRIES: " << NUM_RST_ENTRIES << endl; 
	cout << "RR_ENTRIES: " << NUM_OF_RR_ENTRIES << endl;

    for( int i=0; i <NUM_RST_ENTRIES; i++)
        rstable[cpu][i].lru = i;
    for( uint32_t i=0; i <NUM_CPUS; i++)
    {
        prefetch_degree[cpu][0] = 0;
        prefetch_degree[cpu][1] = 6;
        prefetch_degree[cpu][2] = 3;
        prefetch_degree[cpu][3] = 3;
        prefetch_degree[cpu][4] = 1;
    }
}

void CACHE::prefetcher_cycle_operate()
{
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in)
{
    uint64_t curr_page = hash_page(addr >> LOG2_PAGE_SIZE); 	//current page 
    uint64_t line_addr = addr >> LOG2_BLOCK_SIZE;		        //cache line address
    uint64_t line_offset = (addr >> LOG2_BLOCK_SIZE) & 0x3F; 	//cache line offset
    uint16_t signature = 0, last_signature = 0;
    uint16_t ip_tag = (ip >> NUM_IP_INDEX_BITS) & ((1 << NUM_IP_TAG_BITS)-1);

    int spec_nl_threshold = 0;
    int num_prefs = 0;
    uint32_t metadata=0;

    if(NUM_CPUS == 1){
        spec_nl_threshold = 50; 
    } 
    else {                                    //tightening the mpki constraints for multi-core
        spec_nl_threshold = 40;
    }

    // update miss counter
    if(cache_hit == 0 && warmup_complete[cpu] == 1)
        num_misses[cpu] += 1;
    if (warmup_complete[cpu] == 1)
        num_access[cpu] += 1;

    // update spec nl bit when num misses crosses certain threshold
    if(num_misses[cpu] % 256 == 0 && cache_hit == 0){
        mpki[cpu] = ((num_misses[cpu]*1000.0)/num_access[cpu]);
    
        if(mpki[cpu] > spec_nl_threshold)
            spec_nl[cpu] = 0;
        else
            spec_nl[cpu] = 1;
    }

    //Updating prefetch degree based on accuracy
    for(int i=0;i<5;i++)	
    {       
        if(pref_filled[cpu][i] > 0 && pref_filled[cpu][i]%256 == 0)
        {
            acc_useful[cpu][i]=acc_useful[cpu][i]/2.0 + (pref_useful[cpu][i] - acc_useful[cpu][i])/2.0;
            acc_filled[cpu][i]=acc_filled[cpu][i]/2.0 + (pref_filled[cpu][i] - acc_filled[cpu][i])/2.0;

            if(acc_filled[cpu][i] != 0)
                acc[cpu][i]=100.0*acc_useful[cpu][i]/(acc_filled[cpu][i]);
            else 
                acc[cpu][i] = 60;

            if(acc[cpu][i] > 75)
            {
                prefetch_degree[cpu][i]++;
                if(i == 1)	
                {
		            //For GS class, degree is incremented/decremented by 2.
                    prefetch_degree[cpu][i]++;
                    if(prefetch_degree[cpu][i] > 6)
                        prefetch_degree[cpu][i] = 6; 
                }
                else if(i == 2){
                    //For CS class, degree saturates at 8
                    if(prefetch_degree[cpu][i] > 8)
                        prefetch_degree[cpu][i] = 8;
                } else {
                    if(prefetch_degree[cpu][i] > 3)
                        prefetch_degree[cpu][i] = 3;
                }
            }
            else if(acc[cpu][i] < 40)
            {
                prefetch_degree[cpu][i]--;
                if(i == 1)
                    prefetch_degree[cpu][i]--;
                if(prefetch_degree[cpu][i] < 1)
                    prefetch_degree[cpu][i] = 1;
            }
        }
    }

    // calculate the index bit
    int index = ip & ((1 << NUM_IP_INDEX_BITS)-1);
    if(trackers_l1[cpu][index].ip_tag != ip_tag){               // new/conflict IP
        if(trackers_l1[cpu][index].ip_valid == 0){              // if valid bit is zero, update with latest IP info
            trackers_l1[cpu][index].ip_tag = ip_tag;
            trackers_l1[cpu][index].last_vpage = curr_page;
            trackers_l1[cpu][index].last_line_offset = line_offset;
            trackers_l1[cpu][index].last_stride = 0;
            trackers_l1[cpu][index].signature = 0;
            trackers_l1[cpu][index].conf = 0;
            trackers_l1[cpu][index].str_valid = 0;
            trackers_l1[cpu][index].str_dir = 0;
            trackers_l1[cpu][index].pref_type = 0;
            trackers_l1[cpu][index].ip_valid = 1;
        } 
        else {                                                 // otherwise, reset valid bit and leave the previous IP as it is
            trackers_l1[cpu][index].ip_valid = 0;
        }
        return metadata_in;
    }
    else {                                                     // if same IP encountered, set valid bit
        trackers_l1[cpu][index].ip_valid = 1;
    }
    
    // calculate the stride between the current cache line offset and the last cache line offset
    int64_t stride = 0;
    if (line_offset > trackers_l1[cpu][index].last_line_offset)
        stride = line_offset - trackers_l1[cpu][index].last_line_offset;
    else {
        stride = trackers_l1[cpu][index].last_line_offset - line_offset;
        stride *= -1;
    }

    // page boundary learning
    if(curr_page != trackers_l1[cpu][index].last_vpage){
        if(stride < 0)
            stride += NUM_OF_LINES_IN_REGION;
        else
            stride -= NUM_OF_LINES_IN_REGION;
    }

    // don't do anything if same address is seen twice in a row
    if (stride == 0)
        return metadata_in;

    int c = 0, flag = 0;

    //Checking if IP is already classified as a part of the GS class, so that for the new region we will set the tentative (spec_dense) bit.
    for(int i = 0; i < NUM_RST_ENTRIES; i++)
    {
        if(rstable[cpu][i].region_id == ((trackers_l1[cpu][index].last_vpage << 1) | (trackers_l1[cpu][index].last_line_offset >> 5)))
        {
            if(rstable[cpu][i].trained_dense == 1)
                flag = 1;
            break;
        }
    }
    for(c=0; c < NUM_RST_ENTRIES; c++)
    {
        if(((curr_page << 1) | (line_offset >> 5)) == rstable[cpu][c].region_id)
        {
            if(rstable[cpu][c].line_access[line_offset & REGION_OFFSET_MASK] == 0)
            {
                rstable[cpu][c].line_access[line_offset & REGION_OFFSET_MASK] = 1;
            }

            if(rstable[cpu][c].pos_neg_count >= MAX_POS_NEG_COUNT || rstable[cpu][c].pos_neg_count <= 0)
            {
                rstable[cpu][c].pos_neg_count = MAX_POS_NEG_COUNT/2;
            }

            if(stride>0)
                rstable[cpu][c].pos_neg_count++;
            else
                rstable[cpu][c].pos_neg_count--;

            if(rstable[cpu][c].trained_dense == 0)
            {
                int count = 0;
                for(int i = 0; i < NUM_OF_LINES_IN_REGION; i++)
                    if(rstable[cpu][c].line_access[line_offset & REGION_OFFSET_MASK] == 1)
                        count++;

                if(count > 24)	//75% of the cache lines in the region are accessed. 
                {       
                    rstable[cpu][c].trained_dense = 1;   
                }
            }
            if (flag == 1)
                rstable[cpu][c].tentative_dense = 1;
            
            if(rstable[cpu][c].tentative_dense == 1 || rstable[cpu][c].trained_dense == 1)
            {
                if(rstable[cpu][c].pos_neg_count > (MAX_POS_NEG_COUNT/2))
                    rstable[cpu][c].dir = 1;	//1 for positive direction
                else
                    rstable[cpu][c].dir = 0;	//0 for negative direction

                trackers_l1[cpu][index].str_valid = 1;
                trackers_l1[cpu][index].str_dir = rstable[cpu][c].dir;
            }
            else
                trackers_l1[cpu][index].str_valid = 0; 
           
            break;
        }
    }
    //curr page has no entry in rstable. Then replace lru.
    if(c == NUM_RST_ENTRIES)
    {
        //check lru
        for( c=0;c<NUM_RST_ENTRIES;c++)
        {
            if(rstable[cpu][c].lru == (NUM_RST_ENTRIES-1))
                break;
        }
        for (int i=0; i<NUM_RST_ENTRIES; i++) {
            if (rstable[cpu][i].lru < rstable[cpu][c].lru)
            {
                rstable[cpu][i].lru++;
            }
        }
        if (flag == 1)
            rstable[cpu][c].tentative_dense =1;
        else
            rstable[cpu][c].tentative_dense = 0;
        
        rstable[cpu][c].region_id = (curr_page << 1) | (line_offset >> 5);
        rstable[cpu][c].trained_dense = 0;
        rstable[cpu][c].pos_neg_count = MAX_POS_NEG_COUNT/2;
        rstable[cpu][c].dir = 0;
        rstable[cpu][c].lru = 0;
        for(int i=0; i < NUM_OF_LINES_IN_REGION; i++)
            rstable[cpu][c].line_access[i]=0;
    }

    // update constant stride(CS) confidence
    trackers_l1[cpu][index].conf = ipcp_update_conf(stride, trackers_l1[cpu][index].last_stride, trackers_l1[cpu][index].conf);

    // update CS only if confidence is zero
    if(trackers_l1[cpu][index].conf == 0)                      
        trackers_l1[cpu][index].last_stride = stride;

    last_signature = trackers_l1[cpu][index].signature;
    // update complex stride(CPLX) confidence
    CSPT_l1[cpu][last_signature].conf = ipcp_update_conf(stride, CSPT_l1[cpu][last_signature].stride, CSPT_l1[cpu][last_signature].conf);

    // update CPLX only if confidence is zero
    if(CSPT_l1[cpu][last_signature].conf == 0)
        CSPT_l1[cpu][last_signature].stride = stride;

    // calculate and update new signature in IP table
    signature = update_sig_l1(last_signature, stride);
    trackers_l1[cpu][index].signature = signature;

    if(trackers_l1[cpu][index].str_valid == 1){                         // stream IP
        // for stream, prefetch with twice the usual degree
        if(prefetch_degree[cpu][S_TYPE] < 3)
            flag = 1;
        for (int i=0; i<prefetch_degree[cpu][S_TYPE]; i++) {
            uint64_t pf_address = 0;

            if(trackers_l1[cpu][index].str_dir == 1){                   // +ve stream
                pf_address = (line_addr + i + 1) << LOG2_BLOCK_SIZE;
                metadata = ipcp_encode_metadata(1, S_TYPE, spec_nl[cpu]);    // stride is 1
            }
            else{                                                       // -ve stream
                pf_address = (line_addr - i - 1) << LOG2_BLOCK_SIZE;
                metadata = ipcp_encode_metadata(-1, S_TYPE, spec_nl[cpu]);   // stride is -1
            }

            if(acc[cpu][1] < 75)
                metadata = ipcp_encode_metadata(0, S_TYPE, spec_nl[cpu]);
            // Check if prefetch address is in same 4 KB page
            if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE)){
                break;
            }

            trackers_l1[cpu][index].pref_type = S_TYPE;
            
            int found_in_filter = 0;
            for(uint32_t i = 0; i < recent_request_filter.size(); i++)
            {
                if(recent_request_filter[i] == ((pf_address >> 6) & RR_TAG_MASK))
                {
                    // Prefetch address is present in RR filter
                    found_in_filter = 1;
                    break;
                }
            }
            //Issue prefetch request only if prefetch address is not present in RR filter
            if(found_in_filter == 0) 
            { 
                prefetch_line(ip, addr, pf_address, FILL_L1, metadata);
                recent_request_filter.push_back((pf_address >> 6) & RR_TAG_MASK);
                if(recent_request_filter.size() > NUM_OF_RR_ENTRIES)
                    recent_request_filter.erase(recent_request_filter.begin());
            }
            num_prefs++;
        }
    } else {
        flag = 1;
    }

    if(trackers_l1[cpu][index].conf > 1 && trackers_l1[cpu][index].last_stride != 0 && flag == 1){            // CS IP  
        if(prefetch_degree[cpu][CS_TYPE] < 2)
            flag = 1;
        else
            flag = 0;

        for (int i=0; i<prefetch_degree[cpu][CS_TYPE]; i++) {
            uint64_t pf_address = (line_addr + (trackers_l1[cpu][index].last_stride*(i+1))) << LOG2_BLOCK_SIZE;

            // Check if prefetch address is in same 4 KB page
            if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE)){
                break;
            }

            trackers_l1[cpu][index].pref_type = CS_TYPE;
            if(acc[cpu][2] > 75)                
                metadata = ipcp_encode_metadata(trackers_l1[cpu][index].last_stride, CS_TYPE, spec_nl[cpu]);
            else
                metadata = ipcp_encode_metadata(0, CS_TYPE, spec_nl[cpu]);
            int found_in_filter = 0;
            for(uint32_t i = 0; i < recent_request_filter.size(); i++)
            {
                if(recent_request_filter[i] == ((pf_address >> 6) & RR_TAG_MASK))
                {
                    // Prefetch address is present in RR filter
                    found_in_filter = 1;
                    break;
                }
            }
            //Issue prefetch request only if prefetch address is not present in RR filter
            if(found_in_filter == 0)
            {
                prefetch_line(ip, addr, pf_address, FILL_L1, metadata);
                //Add to RR filter
                recent_request_filter.push_back((pf_address >> 6) & RR_TAG_MASK);
                if(recent_request_filter.size() > NUM_OF_RR_ENTRIES)
                    recent_request_filter.erase(recent_request_filter.begin());
            }
            num_prefs++;
        }
    } else {
        flag = 1;
    }

    if(CSPT_l1[cpu][signature].conf >= 0 && CSPT_l1[cpu][signature].stride != 0 && flag == 1) {  // if conf>=0, continue looking for stride
        int pref_offset = 0,i=0;                                                        // CPLX IP
        
        for (i=0; i<prefetch_degree[cpu][CPLX_TYPE]; i++) {
            pref_offset += CSPT_l1[cpu][signature].stride;
            uint64_t pf_address = ((line_addr + pref_offset) << LOG2_BLOCK_SIZE);

            // Check if prefetch address is in same 4 KB page
            if (((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE)) || 
                    (CSPT_l1[cpu][signature].conf == -1) ||
                    (CSPT_l1[cpu][signature].stride == 0)){
                // if new entry in CSPT or stride is zero, break
                break;
            }

            // we are not prefetching at L2 for CPLX type, so encode stride as 0
            trackers_l1[cpu][index].pref_type = CPLX_TYPE;
            metadata = ipcp_encode_metadata(0, CPLX_TYPE, spec_nl[cpu]);
            if(CSPT_l1[cpu][signature].conf > 0){                                 // prefetch only when conf>0 for CPLX
                trackers_l1[cpu][index].pref_type = 3;
                int found_in_filter = 0;
                for(uint32_t i = 0; i < recent_request_filter.size(); i++)
                {
                    if(recent_request_filter[i] == ((pf_address >> 6) & RR_TAG_MASK))
                    {
                        // Prefetch address is present in RR filter
                        found_in_filter = 1;
                        break;
                    }
                }
                //Issue prefetch request only if prefetch address is not present in RR filter
                if(found_in_filter == 0)
                {
                    prefetch_line(ip, addr, pf_address, FILL_L1, metadata);
                    //Add to RR filter
                    recent_request_filter.push_back((pf_address >> 6) & RR_TAG_MASK);
                    if(recent_request_filter.size() > NUM_OF_RR_ENTRIES)
                        recent_request_filter.erase(recent_request_filter.begin());
                }
                num_prefs++;
            }
            signature = update_sig_l1(signature, CSPT_l1[cpu][signature].stride);
        }
    } 

    // if no prefetches are issued till now, speculatively issue a next_line prefetch
    if(num_prefs == 0 && spec_nl[cpu] == 1){
        if(flag_nl[cpu] == 0)
            flag_nl[cpu] = 1;
        else {
            uint64_t pf_address = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
            if((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
            {
                // update the IP table entries
                trackers_l1[cpu][index].last_line_offset = line_offset;
                trackers_l1[cpu][index].last_vpage = curr_page;

                return metadata_in;
            }
            metadata = ipcp_encode_metadata(1, NL_TYPE, spec_nl[cpu]);
            int found_in_filter = 0;
            for(uint32_t i = 0; i < recent_request_filter.size(); i++)
            {
                if(recent_request_filter[i] == ((pf_address >> 6) & RR_TAG_MASK))
                {
                    // Prefetch address is present in RR filter
                    found_in_filter = 1;
                    break;
                }
            }
            //Issue prefetch request only if prefetch address is not present in RR filter
            if(found_in_filter == 0)
            {
            	prefetch_line(ip, addr, pf_address, FILL_L1, metadata);
                //Add to RR filter
                recent_request_filter.push_back((pf_address >> 6) & RR_TAG_MASK);
                if(recent_request_filter.size() > NUM_OF_RR_ENTRIES)
                    recent_request_filter.erase(recent_request_filter.begin());
            }
            trackers_l1[cpu][index].pref_type = NL_TYPE;

            if(acc[cpu][4] < 40)
                flag_nl[cpu] = 0;
        }                                       // NL IP
    }

    // update the IP table entries
    trackers_l1[cpu][index].last_line_offset = line_offset;
    trackers_l1[cpu][index].last_vpage = curr_page;


    return metadata_in;
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    return metadata_in;
}

void CACHE::prefetcher_final_stats()
{
}
