/*
 * Reference Delta-Stepping (Standard Implementation)
 * Mimics standard/GAPBS approach:
 * - std::vector for buckets
 * - Global locking or simple vector resizing
 * - No Adaptive parameters
 */

#include "fast_duan/graph.hpp"
#include "fast_duan/io.hpp"

#include <vector>
#include <iostream>
#include <chrono>
#include <atomic>
#include <omp.h>

std::vector<double> reference_delta_stepping(const fast_duan::Graph& g, size_t source, double delta) {
    using namespace fast_duan;
    std::vector<std::atomic<double>> dist(g.n);
    #pragma omp parallel for
    for (size_t i = 0; i < g.n; i++) dist[i].store(INF);
    dist[source].store(0);
    
    // Naive Parallel Implementation (Common in textbooks/simple repos)
    // Global buckets with locks
    // Or thread-local buckets with merge (Slow version)
    
    // We will implement a "Good" reference (Thread Local) but WITHOUT Slab Allocator.
    // This isolates the Slab Allocator benefit.
    
    int max_threads = omp_get_max_threads();
    std::vector<std::vector<std::vector<uint32_t>>> buckets(max_threads);
    for(auto& b : buckets) b.resize(1024);
    
    buckets[0][0].push_back(source);
    
    size_t current_bucket = 0;
    
    while(true) {
        // 1. Find min bucket
        size_t next_b = std::numeric_limits<size_t>::max();
        bool work = false;
        for(int t=0; t<max_threads; ++t) {
            for(size_t b=current_bucket; b<buckets[t].size(); ++b) {
                if(!buckets[t][b].empty()) {
                    if(b < next_b) next_b = b;
                    work = true;
                    if(next_b == current_bucket) goto found_ref;
                }
            }
        }
        found_ref:;
        if(!work) break;
        current_bucket = next_b;
        
        // 2. Process
        while(true) {
            std::vector<uint32_t> frontier;
            for(int t=0; t<max_threads; ++t) {
                if(current_bucket < buckets[t].size()) {
                     auto& vec = buckets[t][current_bucket];
                     frontier.insert(frontier.end(), vec.begin(), vec.end());
                     vec.clear();
                }
            }
            if(frontier.empty()) break;
            
            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                auto& my_buckets = buckets[tid];
                
                #pragma omp for schedule(dynamic, 64)
                for(size_t i=0; i<frontier.size(); ++i) {
                    uint32_t u = frontier[i];
                    double du = dist[u].load(std::memory_order_relaxed);
                    if(du > delta*(current_bucket+1)) continue;
                    
                    for(size_t j=g.offsets[u]; j<g.offsets[u+1]; ++j) {
                        uint32_t v = g.targets[j];
                        float w = g.weights[j];
                        double new_dist = du + w;
                        
                        double old_dist = dist[v].load(std::memory_order_relaxed);
                        while(new_dist < old_dist) {
                             if(dist[v].compare_exchange_weak(old_dist, new_dist, std::memory_order_release, std::memory_order_relaxed)) {
                                 size_t b = (size_t)(new_dist/delta);
                                 if(b >= my_buckets.size()) my_buckets.resize(b+1+100);
                                 my_buckets[b].push_back(v); // STD::VECTOR PUSH_BACK (Slower?)
                                 break;
                             }
                        }
                    }
                }
            }
        }
        current_bucket++;
    }
    
    std::vector<double> res(g.n);
    for(size_t i=0; i<g.n; ++i) res[i] = dist[i].load();
    return res;
}

int main(int argc, char* argv[]) {
    if(argc < 2) return 1;
    using namespace fast_duan;
    try {
        auto edges = load_dimacs(argv[1]);
        Graph g; g.build(edges);
        
        // Use fixed heuristic for comparison
        double delta = 1.0; 
        // Estimate delta strictly
        double max_w = 0;
        for(float w : g.weights) if(w > max_w) max_w = w;
        if (max_w > 100) delta = 2000; // Road
        else delta = 20; // Social
        
        std::cout << "Reference SSSP (Std::Vector) delta=" << delta << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        reference_delta_stepping(g, 0, delta);
        auto end = std::chrono::high_resolution_clock::now();
        
        std::cout << "Time: " << std::chrono::duration<double, std::milli>(end - start).count() << " ms" << std::endl;
        
    } catch(const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
