#include <cstdint>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <assert.h>

class RRTable {
    private:

    vector<uint64_t> rr_table;
    uint64_t table_size;
    uint64_t index_mask;
    uint64_t tag_mask;

    public:

    vector<uint64_t> offsets;
    vector<uint64_t> scores;
    uint64_t offset_index;
    uint64_t train_round;

    uint64_t bad_score = 1;
    uint64_t max_score = 31;
    uint64_t max_round = 100;

    uint64_t trained_offset;

    void init(uint32_t index_bit, uint32_t tag_bit, uint64_t delay) {
        rr_table.resize(pow(2, index_bit));
        table_size = pow(2, index_bit);
        index_mask = table_size - 1;
        tag_mask = pow(2, tag_bit) - 1;
    }

    void insert(uint64_t addr) {
        uint64_t index = (addr & index_mask) ^ ((addr >> (int)log2(table_size)) & index_mask);
        assert(index < table_size);
        rr_table[index] = (addr >> (int)log2(table_size)) & tag_mask;
    }

    bool find(uint64_t addr) {
        uint64_t index = (addr & index_mask) ^ ((addr >> (int)log2(table_size)) & index_mask);
        assert(index < table_size);
        if (rr_table[index] == ((addr >> (int)log2(table_size)) & tag_mask)) {
            return true;
        }
        return false;
    }
};