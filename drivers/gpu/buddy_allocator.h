#pragma once

#include <cstdint>
#include <vector>
#include <mutex>

struct MemoryBlock {
    uint64_t addr;
    size_t size;
    bool is_free;
};

class BuddyAllocator {
public:
    explicit BuddyAllocator(uint64_t base, size_t total_size);
    ~BuddyAllocator();

    int allocate(size_t size, uint64_t* addr_out);
    int free(uint64_t addr);

private:
    uint64_t base_;
    size_t total_size_;
    std::vector<MemoryBlock> blocks_;
    mutable std::mutex mtx_;

    size_t align_to_base(size_t size);
    int find_block(size_t size);
    void split(int index);
    void merge(int index);
};
