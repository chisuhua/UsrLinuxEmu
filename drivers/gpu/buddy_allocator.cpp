#include "buddy_allocator.h"
#include <iostream>
#include <cassert>

BuddyAllocator::BuddyAllocator(uint64_t base, size_t total_size)
    : base_(base), total_size_(total_size) {
    assert((total_size_ & (total_size_ - 1)) == 0); // 必须是 2 的幂次
    blocks_.push_back({base_, total_size_, true});
    std::cout << "[BuddyAllocator] Initialized with base=0x" << std::hex << base_
              << ", size=0x" << total_size_ << std::dec << std::endl;
}

BuddyAllocator::~BuddyAllocator() {
    std::cout << "[BuddyAllocator] Destroying allocator." << std::endl;
}

size_t BuddyAllocator::align_to_base(size_t size) {
    if (size <= 0) return 0;
    size_t order = 0;
    while ((1ULL << order) < size) ++order;
    return 1ULL << order;
}

/*
int BuddyAllocator::allocate(size_t size, uint64_t* addr_out) {
    for (auto& b : blocks_) {
        if (b.is_free && b.size >= size) {
            while (b.size > size) split(&b - &blocks_[0]);
            b.is_free = false;
            *addr_out = b.addr;
            return 0;
        }
    }
    return -1;
}
*/
int BuddyAllocator::allocate(size_t size, uint64_t* addr_out) {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t aligned_size = align_to_base(size);
    int idx = find_block(aligned_size);
    if (idx < 0) return -1;

    while (blocks_[idx].size > aligned_size) {
        split(idx);
    }

    blocks_[idx].is_free = false;
    *addr_out = blocks_[idx].addr;

    std::cout << "[BuddyAllocator] Allocated: 0x" << std::hex << *addr_out
              << " (" << std::dec << aligned_size << " bytes)" << std::endl;

    return 0;
}

int BuddyAllocator::find_block(size_t size) {
    for (int i = 0; i < blocks_.size(); ++i) {
        if (blocks_[i].is_free && blocks_[i].size >= size) {
            return i;
        }
    }
    return -1;
}


int BuddyAllocator::free(uint64_t addr) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& b : blocks_) {
        if (b.addr == addr && !b.is_free) {
            b.is_free = true;
            merge(&b - &blocks_[0]);
            return 0;
        }
    }
    return -1;
}

void BuddyAllocator::split(int index) {
    size_t new_size = blocks_[index].size / 2;
    uint64_t left = blocks_[index].addr;
    uint64_t right = left + new_size;

    blocks_[index].size = new_size;
    blocks_[index].is_free = false;

    blocks_.insert(blocks_.begin() + index + 1, {right, new_size, true});
}

void BuddyAllocator::merge(int index) {
    int buddy_index = -1;
    size_t size = blocks_[index].size;
    uint64_t addr = blocks_[index].addr;

    for (int i = 0; i < blocks_.size(); ++i) {
        if (i == index) continue;
        uint64_t buddy_addr = addr ^ size;
        if (blocks_[i].addr == buddy_addr && blocks_[i].size == size && blocks_[i].is_free) {
            buddy_index = i;
            break;
        }
    }

    if (buddy_index != -1) {
        // 合并成一个更大的块
        int merged_idx = std::min(index, buddy_index);
        blocks_[merged_idx].size = size * 2;
        blocks_.erase(blocks_.begin() + std::max(index, buddy_index));
        merge(merged_idx); // 继续向上合并
    }
}