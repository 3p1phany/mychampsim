#include "berti.h"

// Structs
latency_table_t latencyt[NUM_CPUS][LATENCY_TABLE_SIZE];
// Cache Style
history_table_t historyt[NUM_CPUS][HISTORY_TABLE_SET][HISTORY_TABLE_WAY];
shadow_cache_t scache[NUM_CPUS][L1D_SET][L1D_WAY];
std::map<uint64_t, delta_table_t*> delta_table[NUM_CPUS];
// To Make a FIFO MAP
std::queue<uint64_t> delta_table_queue[NUM_CPUS];
// Auxiliar pointers
history_table_t *history_pointers[NUM_CPUS][HISTORY_TABLE_SET];

bool compare_greater_stride_t(stride_t a, stride_t b)
{
    if (a.rpl == L1 && b.rpl != L1) return 1;
    else if (a.rpl != L1 && b.rpl == L1) return 0;
    else
    {
        if (a.rpl == L2 && b.rpl != L2) return 1;
        else if (a.rpl != L2 && b.rpl == L2) return 0;
        else
        {
            if (a.rpl == L3 && b.rpl != L3) return 1;
            if (a.rpl != L3 && b.rpl == L3) return 0;
            else
            {
                if (std::abs(a.stride) < std::abs(b.stride)) return 1;
                return 0;
            }
        }
    }
}

bool compare_greater_stride_t_per(stride_t a, stride_t b)
{
    if (a.per > b.per) return 1;
    else
    {
        if (std::abs(a.stride) < std::abs(b.stride)) return 1;
        return 0;
    }
}

/******************************************************************************/
/*                      Latency table functions                               */
/******************************************************************************/
void latency_table_init(uint32_t cpu)
{
    /*
     * Init pqmshr (latency) table
     *
     * Parameters:
     *      - cpu: cpu
     */
    for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++)
    {
        latencyt[cpu][i].ip   = 0;
        latencyt[cpu][i].addr = 0;
        latencyt[cpu][i].time = 0;
    }
}

uint64_t latency_table_get_ip(uint64_t line_addr, uint32_t cpu)
{
    /*
     * Return 1 or 0 if the addr is or is not in the pqmshr (latency) table
     *
     * Parameters:
     *  - line_addr: address without cache offset
     *  - cpu: actual cpu
     *
     * Return: 1 if the line is in the latency table, otherwise 0
     */

    for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++)
    {
        // Search if the line_addr already exists
        if (latencyt[cpu][i].addr == line_addr && latencyt[cpu][i].ip) 
            return latencyt[cpu][i].ip;
    }

    return 0;
}

uint8_t latency_table_add(uint64_t line_addr, uint64_t ip, uint32_t cpu)
{
    /*
     * Save if possible the new miss into the pqmshr (latency) table
     *
     * Parameters:
     *  - line_addr: address without cache offset
     *  - cpu: actual cpu
     */
    return latency_table_add(line_addr, ip, cpu, current_core_cycle[cpu] & TIME_MASK);
}

uint8_t latency_table_add(uint64_t line_addr, uint64_t ip, uint32_t cpu, uint64_t cycle)
{
    /*
     * Save if possible the new miss into the pqmshr (latency) table
     *
     * Parameters:
     *  - line_addr: address without cache offset
     *  - cpu: actual cpu
     *  - cycle: time to use in the latency table
     *
     * Return: 1 if the addr already exist, otherwise 0.
     */

    latency_table_t *free;
    free = nullptr;

    for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++)
    {
        // Search if the line_addr already exists. If it exist we does not have
        // to do nothing more
        if (latencyt[cpu][i].addr == line_addr) 
        {
            return 1;
        }

        // We discover a free space into the latency table, save it for later
        if (latencyt[cpu][i].addr == 0) free = &latencyt[cpu][i];
    }

    // No free space!! This cannot be truth
    if (free == nullptr) {
        return 0;
    }

    // We save the new entry into the latency table
    free->addr = line_addr;
    free->time = cycle;
    free->ip   = ip;

    return 1;
}

uint64_t latency_table_del(uint64_t line_addr, uint32_t cpu)
{
    /*
     * Remove the address from the latency table
     *
     * Parameters:
     *  - line_addr: address without cache offset
     *  - cpu: actual cpu
     *
     *  Return: the latency of the address
     */
    
    for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++) {
        if (current_core_cycle[cpu] & TIME_MASK < latencyt[cpu][i].time) {
            latencyt[cpu][i].addr = 0;
            latencyt[cpu][i].ip   = 0;
            latencyt[cpu][i].time = 0;
        }
    }
    for (uint32_t i = 0; i < LATENCY_TABLE_SIZE; i++)
    {
        // Line already in the table
        if (latencyt[cpu][i].addr == line_addr)
        {
            uint64_t latency = (current_core_cycle[cpu] & TIME_MASK) - latencyt[cpu][i].time; // Calculate latency

            latencyt[cpu][i].addr = 0; // Free the entry
            latencyt[cpu][i].ip   = 0; // Free the entry
            latencyt[cpu][i].time = 0; // Free the entry

            // Return the latency
            return latency;
        }
    }

    // We should always track the misses
    return 0;
}

/******************************************************************************/
/*                       Shadow Cache functions                               */
/******************************************************************************/
void shadow_cache_init(uint32_t cpu)
{
    /*
     * Init shadow cache
     *
     * Parameters:
     *      - cpu: cpu
     */
    for (uint8_t i = 0; i < L1D_SET; i++)
    {
        for (uint8_t ii = 0; ii < L1D_WAY; ii++)
        {
            scache[cpu][i][ii].addr = 0;
            scache[cpu][i][ii].lat  = 0;
            scache[cpu][i][ii].pf   = 0;
        }
    }
}

uint8_t shadow_cache_add(uint32_t cpu, uint32_t set, uint32_t way, uint64_t line_addr, uint8_t pf, uint64_t latency)
{
    /*
     * Add block to shadow cache
     *
     * Parameters:
     *      - cpu: cpu
     *      - set: cache set
     *      - way: cache way
     *      - addr: cache block v_addr
     *      - access: the cache is access by a demand
     */
    scache[cpu][set][way].addr = line_addr;
    scache[cpu][set][way].pf   = pf;
    scache[cpu][set][way].lat  = latency;
    return scache[cpu][set][way].pf;
}

uint8_t shadow_cache_get(uint32_t cpu, uint64_t line_addr)
{
    /*
     * Init shadow cache
     *
     * Parameters:
     *      - cpu: cpu
     *      - addr: cache block v_addr
     *
     * Return: 1 if the addr is in the l1d cache, 0 otherwise
     */

    for (uint32_t i = 0; i < L1D_SET; i++)
    {
        for (uint32_t ii = 0; ii < L1D_WAY; ii++)
        {
            if (scache[cpu][i][ii].addr == line_addr) return 1;
        }
    }

    return 0;
}

uint8_t shadow_cache_reset_pf(uint32_t cpu, uint64_t line_addr)
{
    /*
     * Init shadow cache
     *
     * Parameters:
     *      - cpu: cpu
     *      - addr: cache block v_addr
     *
     * Return: 1 if the addr is in the l1d cache, 0 otherwise
     */

    for (uint32_t i = 0; i < L1D_SET; i++)
    {
        for (uint32_t ii = 0; ii < L1D_WAY; ii++)
        {
            if (scache[cpu][i][ii].addr == line_addr) 
            {
                scache[cpu][i][ii].pf = 0;
                return 1;
            }
        }
    }

    return 0;
}

uint8_t shadow_cache_is_pf(uint32_t cpu, uint64_t line_addr)
{
    /*
     * Init shadow cache
     *
     * Parameters:
     *      - cpu: cpu
     *      - addr: cache block v_addr
     *
     * Return: 1 if the addr is in the l1d cache, 0 otherwise
     */

    for (uint32_t i = 0; i < L1D_SET; i++)
    {
        for (uint32_t ii = 0; ii < L1D_WAY; ii++)
        {
            if (scache[cpu][i][ii].addr == line_addr) return scache[cpu][i][ii].pf;
        }
    }

    return 0;
}

uint8_t shadow_cache_latency(uint32_t cpu, uint64_t line_addr)
{
    /*
     * Init shadow cache
     *
     * Parameters:
     *      - cpu: cpu
     *      - addr: cache block v_addr
     *
     * Return: 1 if the addr is in the l1d cache, 0 otherwise
     */

    for (uint32_t i = 0; i < L1D_SET; i++)
    {
        for (uint32_t ii = 0; ii < L1D_WAY; ii++)
        {
            if (scache[cpu][i][ii].addr == line_addr) return scache[cpu][i][ii].lat;
        }
    }
    assert(0);
    return 0;
}

/******************************************************************************/
/*                       History Table functions                               */
/******************************************************************************/
// Auxiliar history table functions
void history_table_init(uint32_t cpu)
{
    /*
     * Initialize history table pointers
     *
     * Parameters:
     *      - cpu: cpu
     */
    for (uint32_t i = 0; i < HISTORY_TABLE_SET; i++) 
    {
        // Pointer to the first element
        history_pointers[cpu][i] = historyt[cpu][i];

        for (uint32_t ii = 0; ii < HISTORY_TABLE_WAY; ii++) 
        {
            historyt[cpu][i][ii].ip   = 0;
            historyt[cpu][i][ii].time = 0;
            historyt[cpu][i][ii].addr = 0;
        }
    }
}

void history_table_add(uint64_t ip, uint32_t cpu, uint64_t addr)
{
    /*
     * Save the new information into the history table
     *
     * Parameters:
     *  - ip: PC tag
     *  - cpu: actual cpu
     *  - addr: ip addr access
     */
    uint16_t set = ip & TABLE_SET_MASK;
    addr &= ADDR_MASK;
    uint64_t cycle = current_core_cycle[cpu] & TIME_MASK;

    // Save new element into the history table
    history_pointers[cpu][set]->ip        = ip;
    history_pointers[cpu][set]->time      = cycle;
    history_pointers[cpu][set]->addr      = addr;

    if (history_pointers[cpu][set] == &historyt[cpu][set][HISTORY_TABLE_WAY - 1]){
        history_pointers[cpu][set] = &historyt[cpu][set][0]; // End the cycle
    } else history_pointers[cpu][set]++; // Pointer to the next (oldest) entry
}

uint16_t history_table_get(uint32_t cpu, uint32_t latency, uint64_t cycle, uint64_t ip, uint64_t act_addr, uint64_t addr[HISTORY_TABLE_WAY])
{
    /*
     * Return an array (by parameter) with all the possible addr that can launch
     * an on-time prefetch
     *
     * Parameters:
     *  - ip: PC tag
     *  - cpu: actual cpu
     *  - latency: latency of the processor
     *  - on_time_addr (out): addr that can launch an on-time prefetch
     *  - num_on_time (out): number of ips that can launch an on-time prefetch
     */

    act_addr &= ADDR_MASK;
    uint16_t num_on_time = 0;
    uint16_t set = ip & TABLE_SET_MASK;

    // The IPs that is launch in this cycle will be able to launch this prefetch
    if (cycle < latency) return num_on_time;
    cycle -= latency; 

    // Pointer to guide
    history_table_t *pointer = history_pointers[cpu][set];

    do
    {
        // Look for the addr that can launch this prefetch
        if (pointer->ip == ip && pointer->time <= cycle)
        {
            // Test that addr is not duplicated
            if (pointer->addr == act_addr) return num_on_time;

            for (int i = 0; i < num_on_time; i++)
            {
                if (pointer->addr == addr[i]) return num_on_time;
            }

            // This IP can launch the prefetch
            addr[num_on_time] = pointer->addr;
            num_on_time++;
        }

        if (pointer == historyt[cpu][set])
        {
            pointer = &historyt[cpu][set][HISTORY_TABLE_WAY - 1];
        } else pointer--;
    } while (pointer != history_pointers[cpu][set]);

    return num_on_time;
}

/******************************************************************************/
/*                        Delta table functions                               */
/******************************************************************************/
// Auxiliar delta table functions
void delta_table_increase_conf_ip(uint64_t ip, uint32_t cpu)
{
    if (delta_table[cpu].find(ip) == delta_table[cpu].end()) return;

    delta_table_t *tmp = delta_table[cpu][ip];
    stride_t *aux = tmp->stride;

    tmp->conf += CONFIDENCE_INC;

    if (tmp->conf == CONFIDENCE_MAX) 
    {
        // Max confidence achieve
        for(int i = 0; i < DELTA_TABLE_STRIDE_SIZE; i++)
        {
            float temp = (float) aux[i].conf / (float) tmp->conf;
            uint64_t aux_conf   = (uint64_t) (temp * 100);

            // Set bits
            if (aux_conf > CONFIDENCE_L1) aux[i].rpl = L1;
            else if (aux_conf > CONFIDENCE_L2) aux[i].rpl = L2;
            else if (aux_conf > CONFIDENCE_L3) aux[i].rpl = L3;
            else aux[i].rpl = R;
            
            aux[i].conf = 0;
        }

        tmp->conf = 0;
    }
}

void delta_table_add(uint64_t ip, uint32_t cpu, int64_t stride)
{
    /*
     * Save the new information into the history table
     *
     * Parameters:
     *  - ip: PC tag
     *  - cpu: actual cpu
     *  - stride: actual cpu
     */
    if (delta_table[cpu].find(ip) == delta_table[cpu].end())
    {
        // FIFO MAP
        if (delta_table_queue[cpu].size() > DELTA_TABLE_SIZE)
        {
            uint64_t key = delta_table_queue[cpu].front();
            delta_table_t *tmp = delta_table[cpu][key];
            delete tmp->stride;
            delete tmp;
            delta_table[cpu].erase(delta_table_queue[cpu].front());
            delta_table_queue[cpu].pop();
        }
        delta_table_queue[cpu].push(ip);

        assert(delta_table[cpu].size() <= DELTA_TABLE_SIZE);

        delta_table_t *tmp = new delta_table_t;
        tmp->stride = new stride_t[DELTA_TABLE_STRIDE_SIZE]();
        
        // Confidence IP
        tmp->conf = CONFIDENCE_INC;

        // Create new stride
        tmp->stride[0].stride = stride;
        tmp->stride[0].conf = CONFIDENCE_INIT;
        tmp->stride[0].rpl = R;

        // Save value
        delta_table[cpu].insert(make_pair(ip, tmp));
        return;
    }

    delta_table_t *tmp = delta_table[cpu][ip];
    stride_t *aux = tmp->stride;

    // Increase IP confidence
    uint8_t max = 0;

    for (int i = 0; i < DELTA_TABLE_STRIDE_SIZE; i++)
    {
        if (aux[i].stride == stride)
        {
            aux[i].conf += CONFIDENCE_INC;
            if (aux[i].conf > CONFIDENCE_MAX) aux[i].conf = CONFIDENCE_MAX;
            return;
        }
    }

    uint8_t dx_conf = 100;
    int dx_remove = -1;
    for (int i = 0; i < DELTA_TABLE_STRIDE_SIZE; i++)
    {
        if (aux[i].rpl == R && aux[i].conf < dx_conf)
        {
            dx_conf = aux[i].conf;
            dx_remove = i;
        }
    }

    if (dx_remove > -1)
    {
        tmp->stride[dx_remove].stride = stride;
        tmp->stride[dx_remove].conf   = CONFIDENCE_INIT;
        tmp->stride[dx_remove].rpl    = R;
        return;
    } else {
        for (int i = 0; i < DELTA_TABLE_STRIDE_SIZE; i++)
        {
            if (aux[i].rpl == L3 && aux[i].conf < dx_conf)
            {
                dx_conf = aux[i].conf;
                dx_remove = i;
            }
        }
        if (dx_remove > -1)
        {
            tmp->stride[dx_remove].stride = stride;
            tmp->stride[dx_remove].conf   = CONFIDENCE_INIT;
            tmp->stride[dx_remove].rpl    = R;
            return;
        }
    }
}

uint8_t delta_table_get(uint64_t ip, uint32_t cpu, stride_t res[MAX_PF])
{
    /*
     * Save the new information into the history table
     *
     * Parameters:
     *  - ip: PC tag
     *  - cpu: actual cpu
     *
     * Return: the stride to prefetch
     */
    if (!delta_table[cpu].count(ip)) return 0;

    delta_table_t *tmp = delta_table[cpu][ip];
    stride_t *aux = tmp->stride;
    uint64_t max_conf = 0;
    uint16_t dx = 0;
    
    for (int i = 0; i < DELTA_TABLE_STRIDE_SIZE; i++)
    {
        if (aux[i].stride != 0 && aux[i].rpl)
        {
            // Substitue min confidence for the next one
            res[dx].stride = aux[i].stride;
            res[dx].rpl = aux[i].rpl;
            dx++;
        }
    }

    if (dx == 0 && tmp->conf >= CONFIDENCE_MIN)
    {
        for (int i = 0; i < DELTA_TABLE_STRIDE_SIZE; i++)
        {
            if (aux[i].stride != 0)
            {
                // Substitue min confidence for the next one
                res[dx].stride = aux[i].stride;
                float temp = (float) aux[i].conf / (float) tmp->conf;
                uint64_t aux_conf   = (uint64_t) (temp * 100);
                res[dx].per = aux_conf;
                dx++;
            }
        }
        sort(res, res + MAX_PF, compare_greater_stride_t_per);

        for (int i = 0; i < MAX_PF; i++)
        {
            if (res[i].per > CONFIDENCE_L1) res[i].rpl = L1;
            else if (res[i].per > CONFIDENCE_L2) res[i].rpl = L2;
            else res[i].rpl = R;
        }
    }

    sort(res, res + MAX_PF, compare_greater_stride_t);

    return 1;
}

void find_and_update(uint32_t cpu, uint64_t latency, uint64_t ip, uint64_t cycle, uint64_t line_addr)
{ 
    // We were tracking this miss
    uint64_t addr[HISTORY_TABLE_WAY];
    uint16_t num_on_time = 0;

    // Get the IPs that can launch a prefetch
    num_on_time = history_table_get(cpu, latency, cycle, ip, line_addr, addr);

    for (uint32_t i = 0; i < num_on_time; i++)
    {
        // Increase conf ip
        if (i == 0) delta_table_increase_conf_ip(ip, cpu);
        
        // Max number of strides that we can find
        if (i >= MAX_HISTORY_IP) break;

        // Add information into delta table
        int64_t stride;
        line_addr &= ADDR_MASK;

        // Usually applications go from lower to higher memory position.
        // The operation order is important (mainly because we allow
        // negative strides)
        stride = (int64_t) (line_addr - addr[i]);

        if ((std::abs(stride) < (1 << STRIDE_MASK)))
        {
            // Only useful strides
            delta_table_add(ip, cpu, stride);
        }
    }
}

void CACHE::prefetcher_initialize()
{
    shadow_cache_init(cpu);
    latency_table_init(cpu);
    history_table_init(cpu);

    std::cout << "L1D [Berti] prefetcher" << std::endl;
}

uint64_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool hit_pref, uint8_t type, uint64_t metadata_in) 
{
    assert(type == LOAD || type == RFO);
    
    uint64_t line_addr = (addr >> LOG2_BLOCK_SIZE); // Line addr
    
    ip = ((ip >> 1) ^ (ip >> 4));
    ip = ip & IP_MASK;

    if (!cache_hit){
        // This is a miss

        // Add @ to latency table
        latency_table_add(line_addr, ip, cpu);

        // Add to history table
        history_table_add(ip, cpu, line_addr);

    } else if (cache_hit && shadow_cache_is_pf(cpu, line_addr)) {
        // Cache line access
        shadow_cache_reset_pf(cpu, line_addr);

        // Buscar strides Y actualizar
        uint64_t latency = shadow_cache_latency(cpu, line_addr);
        find_and_update(cpu, latency, ip, current_core_cycle[cpu] & TIME_MASK, line_addr);

        history_table_add(ip, cpu, line_addr); 
    } else {
        // Cache line access
        shadow_cache_reset_pf(cpu, line_addr);
        // No pf in hit
        //return;
    }

    // Get stride to prefetch
    stride_t stride[MAX_PF];
    for (int i = 0; i < MAX_PF; i++) {
        stride[i].conf = 0;
        stride[i].stride = 0;
        stride[i].rpl = R;
    }

    if (!delta_table_get(ip, cpu, stride)) return metadata_in;

    int launched = 0;
    for (int i = 0; i < MAX_PF_LAUNCH; i++) {
        uint64_t p_addr = (line_addr + stride[i].stride) << LOG2_BLOCK_SIZE;
        uint64_t p_b_addr = (p_addr >> LOG2_BLOCK_SIZE);

        uint8_t pf_fill_level = FILL_L1;
        float mshr_load = ((float) MSHR.size() / (float) MSHR_SIZE) * 100;

        // Level of prefetching depends son CONFIDENCE
        if (stride[i].rpl == L1 && mshr_load < MSHR_LIMIT)
        {
            pf_fill_level = FILL_L1;
        } else if (stride[i].rpl == L1 || stride[i].rpl == L2){
            pf_fill_level = FILL_L2;
        } else if (stride[i].rpl == L3){
            pf_fill_level = FILL_LLC;
        }

        if (prefetch_line(p_addr, pf_fill_level, 0)){
            launched++;
        }
    }

    return metadata_in;
}

uint64_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in, int64_t ret_val)
{
    uint64_t line_addr    = (addr >> LOG2_BLOCK_SIZE); // Line addr

    // Remove @ from latency table
    uint64_t ip      = latency_table_get_ip(line_addr, cpu);
    uint64_t latency = latency_table_del(line_addr, cpu);
    uint64_t cycle   = current_core_cycle[cpu] & TIME_MASK - latency;

    if (latency > LAT_MASK) latency = 0;

    // Add to the shadow cache
    shadow_cache_add(cpu, set, way, line_addr, prefetch, latency);

    if (latency != 0 && !prefetch)
    {
        find_and_update(cpu, latency, ip, cycle, line_addr);
    }

    return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
}

void CACHE::prefetcher_final_stats()
{
}
