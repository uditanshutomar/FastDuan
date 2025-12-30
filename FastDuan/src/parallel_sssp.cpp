/*
 * FastDuan: Parallel Adaptive Delta-Stepping SSSP (SOTA Edition)
 * Innovation: Slab Allocator & Private Buckets
 */

#include "fast_duan/graph.hpp"
#include "fast_duan/io.hpp"
#include "fast_duan/algorithms.hpp"
#include "fast_duan/memory.hpp"

#include <vector>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <atomic>
#include <omp.h>

namespace parallel_sota {

using namespace fast_duan;

std::vector<double> delta_stepping_memory_opt(const Graph& g, size_t source, double delta) {
    std::vector<std::atomic<double>> dist(g.n);
    #pragma omp parallel for
    for (size_t i = 0; i < g.n; i++) dist[i].store(INF, std::memory_order_relaxed);
    
    dist[source].store(0, std::memory_order_relaxed);
    
    int max_threads = omp_get_max_threads();
    
    // Allocators (One per thread to avoid locking)
    std::vector<std::unique_ptr<BlockAllocator>> allocators(max_threads);
    for(int i=0; i<max_threads; i++) allocators[i] = std::make_unique<BlockAllocator>();
    
    // Per-thread buckets: [tid][bucket_idx] -> LinkedBucket
    // Since LinkedBucket is small (2 ptrs), valid to resize vector.
    std::vector<std::vector<LinkedBucket>> per_thread_buckets(max_threads);
    for(auto& b : per_thread_buckets) {
        b.resize(1024); // Start capacity
    }
    
    // Init
    per_thread_buckets[0][0].push_back((uint32_t)source, *allocators[0]);
    
    size_t current_bucket = 0;
    
    while (true) {
        // 1. Find next bucket
        size_t next_bucket = std::numeric_limits<size_t>::max();
        bool work_remains = false;
        
        for (int t = 0; t < max_threads; t++) {
            // Check limits. If buckets resized, size() changed.
            // Using raw loop is safe if we don't shrink.
            for (size_t b = current_bucket; b < per_thread_buckets[t].size(); b++) {
                if (!per_thread_buckets[t][b].empty()) {
                    if (b < next_bucket) next_bucket = b;
                    work_remains = true;
                    // Heuristic optimization
                    if (next_bucket == current_bucket) goto found;
                }
            }
        }
        found:;
        
        if (!work_remains) break;
        current_bucket = next_bucket;
        
        // 2. Process Bucket
        while (true) {
            // We need to iterate ALL nodes in current_bucket across ALL threads.
            // But now they are in LinkedBuckets (Linked Lists).
            // Flattening is expensive (copying data).
            // Iterating Linked Lists in parallel is tricky? 
            // "Bag of Tasks": Collect all Blocks?
            // Yes. Collect pointers to all non-empty Blocks.
            
            std::vector<Block*> work_blocks;
            size_t total_elements = 0;
            
            for (int t = 0; t < max_threads; t++) {
                if (current_bucket < per_thread_buckets[t].size()) {
                    LinkedBucket& lb = per_thread_buckets[t][current_bucket];
                    Block* curr = lb.head;
                    while (curr) {
                        if (curr->size > 0) {
                            work_blocks.push_back(curr);
                            total_elements += curr->size;
                        }
                        curr = curr->next;
                    }
                    lb.clear(); // Logically clear 
                }
            }
            
            if (total_elements == 0) break;
            
            // Parallel Iterate Blocks?
            // "Work Stealing" or Static Block assignment.
            // Since blocks are small (1024), we can just #pragma omp parallel for on 'work_blocks'.
            // Simple and Efficient!
            
            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                auto& my_alloc = *allocators[tid];
                auto& my_buckets = per_thread_buckets[tid];
                
                #pragma omp for schedule(dynamic)
                for (size_t i = 0; i < work_blocks.size(); i++) {
                    Block* block = work_blocks[i];
                    for (uint32_t k = 0; k < block->size; k++) {
                        uint32_t u = block->data[k];
                        double du = dist[u].load(std::memory_order_relaxed);
                        
                        if (du > delta * (current_bucket + 1)) continue;
                        
                        // Prefetch offsets?
                        // __builtin_prefetch(&g.offsets[u], 0, 1);
                        
                        for (size_t j = g.offsets[u]; j < g.offsets[u+1]; j++) {
                            uint32_t v = g.targets[j];
                            float w = g.weights[j];
                            double new_dist = du + w;
                            
                            double old_dist = dist[v].load(std::memory_order_relaxed);
                            while (new_dist < old_dist) {
                                if (dist[v].compare_exchange_weak(old_dist, new_dist, 
                                                                  std::memory_order_release, 
                                                                  std::memory_order_relaxed)) {
                                    size_t bucket_idx = static_cast<size_t>(new_dist / delta);
                                    
                                    if (bucket_idx >= my_buckets.size()) {
                                        my_buckets.resize(bucket_idx + 1 + 20);
                                    }
                                    my_buckets[bucket_idx].push_back(v, my_alloc);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        current_bucket++;
    }
    
    std::vector<double> res(g.n);
    #pragma omp parallel for
    for (size_t i = 0; i < g.n; i++) res[i] = dist[i].load(std::memory_order_relaxed);
    return res;
}

} // namespace parallel_sota

int main(int argc, char* argv[]) {
    using namespace parallel_sota;
    if (argc < 2) return 1;
    
    #if defined(_OPENMP)
    std::cout << "FastDuan SOTA (Slab Allocator) with OpenMP threads: " << omp_get_max_threads() << std::endl;
    #else
    std::cout << "Serial SOTA" << std::endl;
    #endif
    
    try {
        auto edges = load_dimacs(argv[1]);
        Graph g; g.build(edges);
        
        double delta = select_delta(g);
        std::cout << "Selected Delta: " << delta << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto dist = delta_stepping_memory_opt(g, 0, delta);
        auto end = std::chrono::high_resolution_clock::now();
        
        std::cout << "Time: " << std::chrono::duration<double, std::milli>(end - start).count() << " ms" << std::endl;
        
        double cksum = 0;
        for (double d : dist) if (d < INF) cksum += d;
        std::cout << "Checksum: " << (long long)cksum << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
