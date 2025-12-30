/*
 * FastDuan Algorithms Library
 * Single-Source Shortest Path Implementations
 */

#ifndef FAST_DUAN_ALGORITHMS_HPP
#define FAST_DUAN_ALGORITHMS_HPP

#include "graph.hpp"
#include <atomic>
#include <cmath>

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace fast_duan {

// ============================================================================
// Adaptive Delta Selection
// ============================================================================

inline double select_delta(const Graph& g) {
    switch (g.graph_type) {
        case Graph::Type::ROAD: return g.mean_weight;
        case Graph::Type::SOCIAL: return g.sum_weight / (g.m * std::log2(g.n));
        default: return g.max_weight / 10.0;
    }
}

// ============================================================================
// Sequential Delta-Stepping
// ============================================================================

inline std::vector<double> delta_stepping_seq(const Graph& g, size_t source, double delta) {
    if (delta <= 0) delta = 1.0;
    std::vector<double> dist(g.n, INF);
    dist[source] = 0;
    
    std::vector<std::vector<uint32_t>> buckets(1);
    buckets[0].push_back(static_cast<uint32_t>(source));
    
    size_t current_bucket = 0;
    
    while (current_bucket < buckets.size()) {
        while (!buckets[current_bucket].empty()) {
            std::vector<uint32_t> frontier = std::move(buckets[current_bucket]);
            buckets[current_bucket].clear();
            
            for (uint32_t u : frontier) {
                if (dist[u] > delta * (current_bucket + 1)) continue;
                
                for (size_t i = g.offsets[u]; i < g.offsets[u + 1]; i++) {
                    uint32_t v = g.targets[i];
                    double nd = dist[u] + g.weights[i];
                    if (nd < dist[v]) {
                        dist[v] = nd;
                        size_t bucket_idx = static_cast<size_t>(nd / delta);
                        if (bucket_idx >= buckets.size()) {
                            buckets.resize(bucket_idx + 1);
                        }
                        buckets[bucket_idx].push_back(v);
                    }
                }
            }
        }
        current_bucket++;
    }
    return dist;
}

// ============================================================================
// Parallel Delta-Stepping (OpenMP)
// ============================================================================

#if defined(_OPENMP)

inline void atomic_min(std::atomic<double>& var, double val) {
    double prev_val = var.load(std::memory_order_relaxed);
    while (prev_val > val && !var.compare_exchange_weak(prev_val, val, 
                                                        std::memory_order_release, 
                                                        std::memory_order_relaxed)) {
    }
}

inline std::vector<double> delta_stepping_par(const Graph& g, size_t source, double delta) {
    if (delta <= 0) delta = 1.0;
    std::vector<std::atomic<double>> dist(g.n);
    for (size_t i = 0; i < g.n; i++) dist[i].store(INF, std::memory_order_relaxed);
    dist[source].store(0, std::memory_order_relaxed);
    
    std::vector<std::vector<uint32_t>> buckets(1);
    buckets[0].push_back(static_cast<uint32_t>(source));
    
    size_t current_bucket_idx = 0;
    int max_threads = omp_get_max_threads();
    std::vector<std::vector<std::vector<uint32_t>>> thread_buffers(max_threads); 
    
    while (true) {
        if (current_bucket_idx >= buckets.size() || buckets[current_bucket_idx].empty()) {
            bool found = false;
            for (size_t i = current_bucket_idx; i < buckets.size(); i++) {
                if (!buckets[i].empty()) {
                    current_bucket_idx = i;
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }
        
        while (!buckets[current_bucket_idx].empty()) {
            std::vector<uint32_t> frontier = std::move(buckets[current_bucket_idx]);
            buckets[current_bucket_idx].clear();
            
            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                if (thread_buffers[tid].empty()) thread_buffers[tid].resize(100);
                for (auto& buf : thread_buffers[tid]) buf.clear();
                
                #pragma omp for schedule(dynamic, 64)
                for (size_t i = 0; i < frontier.size(); i++) {
                    uint32_t u = frontier[i];
                    double du = dist[u].load(std::memory_order_relaxed);
                    if (du > delta * (current_bucket_idx + 1)) continue;
                    
                    for (size_t j = g.offsets[u]; j < g.offsets[u+1]; j++) {
                        uint32_t v = g.targets[j];
                        float w = g.weights[j];
                        double new_dist = du + w;
                        
                        double old_dist = dist[v].load(std::memory_order_relaxed);
                        if (new_dist < old_dist) {
                            atomic_min(dist[v], new_dist);
                            if (dist[v].load(std::memory_order_relaxed) == new_dist) {
                                size_t bucket = static_cast<size_t>(new_dist / delta);
                                size_t diff = (bucket > current_bucket_idx) ? bucket - current_bucket_idx : 0;
                                if (diff >= thread_buffers[tid].size()) thread_buffers[tid].resize(diff + 10);
                                thread_buffers[tid][diff].push_back(v);
                            }
                        }
                    }
                }
            }
            
            for (int tid = 0; tid < max_threads; tid++) {
                for (size_t diff = 0; diff < thread_buffers[tid].size(); diff++) {
                    if (thread_buffers[tid][diff].empty()) continue;
                    size_t glob_idx = current_bucket_idx + diff;
                    if (glob_idx >= buckets.size()) buckets.resize(glob_idx + 1);
                    buckets[glob_idx].insert(buckets[glob_idx].end(), thread_buffers[tid][diff].begin(), thread_buffers[tid][diff].end());
                    thread_buffers[tid][diff].clear();
                }
            }
        }
        current_bucket_idx++;
    }
    
    std::vector<double> res(g.n);
    for (size_t i = 0; i < g.n; i++) res[i] = dist[i].load();
    return res;
}
#endif // _OPENMP

} // namespace fast_duan

#endif // FAST_DUAN_ALGORITHMS_HPP
