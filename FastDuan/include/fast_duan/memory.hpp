/*
 * FastDuan High-Performance Memory Pool
 * Slab Allocator for Graph Traversal
 */

#ifndef FAST_DUAN_MEMORY_HPP
#define FAST_DUAN_MEMORY_HPP

#include <vector>
#include <atomic>
#include <cstdint>
#include <memory>
#include <iostream>

namespace fast_duan {

// A block of vertices. 
// Size tuned to cache line (64 bytes) or page?
// 1024 ints = 4KB (1 Page). Good for OS allocator interaction.
constexpr size_t BLOCK_SIZE = 1024;

struct Block {
    uint32_t data[BLOCK_SIZE];
    uint32_t size = 0;
    Block* next = nullptr;
};

// Thread-Local Block Allocator
// Acquires large chunks from OS/Global, hands out Blocks to thread.
class BlockAllocator {
public:
    BlockAllocator() {
        // Pre-allocate decent amount?
        current_slab = new Block[SLAB_COUNT];
        slab_idx = 0;
    }
    
    ~BlockAllocator() {
        for (auto* ptr : allocated_slabs) delete[] ptr;
        delete[] current_slab;
    }
    
    Block* alloc() {
        if (slab_idx == SLAB_COUNT) {
            allocated_slabs.push_back(current_slab);
            current_slab = new Block[SLAB_COUNT];
            slab_idx = 0;
        }
        return &current_slab[slab_idx++];
    }
    
    void reset() {
        // Reuse logic?
        // For simplicity in Delta-Stepping (which clears buckets),
        // we might not simple-reset. We construct/destruct buckets.
        // But fast reset is critical.
        // Let's keep it simple: Monotonic allocator.
        // Resetting the whole pool is hard if blocks are scattered.
        // SSSP runs once. We can leak/cleaup at end.
        // For repeated benchmarks, we destruct allocator.
    }

private:
    static constexpr size_t SLAB_COUNT = 128;
    Block* current_slab;
    size_t slab_idx;
    std::vector<Block*> allocated_slabs;
};

// Logical Bucket using Linked Blocks
// High throughput Append, Fast Iterate.
struct LinkedBucket {
    Block* head = nullptr;
    Block* tail = nullptr;
    
    void push_back(uint32_t v, BlockAllocator& allocator) {
        if (!tail || tail->size == BLOCK_SIZE) {
            Block* new_block = allocator.alloc();
            new_block->size = 0;
            new_block->next = nullptr;
            
            if (tail) tail->next = new_block;
            else head = new_block;
            tail = new_block;
        }
        tail->data[tail->size++] = v;
    }
    
    bool empty() const {
        return head == nullptr;
    }
    
    void clear() {
        head = nullptr;
        tail = nullptr;
        // Note: Blocks are NOT freed. They are owned by Allocator.
        // This causes memory usage to grow Monotonically.
        // This is fine for one SSSP run. 
        // FastDuan: 4M nodes * 10 updates ~ 40M elements. 160MB. Trivial.
    }
    
    // Iterator helper
    template<typename Func>
    void for_each(Func f) {
        Block* curr = head;
        while (curr) {
            for (uint32_t i = 0; i < curr->size; i++) {
                f(curr->data[i]);
            }
            curr = curr->next;
        }
    }
};

} // namespace fast_duan

#endif // FAST_DUAN_MEMORY_HPP
