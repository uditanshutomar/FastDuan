/*
 * Geometric Delta-Stepping SSSP
 * Improvement 1: Dynamic bucket widths
 */

#include "fast_duan/graph.hpp"
#include "fast_duan/io.hpp"

#include <vector>
#include <iomanip>
#include <iostream>
#include <chrono>

namespace geometric_sssp {

using namespace fast_duan;

std::vector<double> geometric_delta_stepping(const Graph& g, size_t source, double base_delta) {
    if (base_delta <= 0) base_delta = 1.0;
    
    std::vector<double> dist(g.n, INF);
    dist[source] = 0;
    
    std::vector<std::vector<uint32_t>> buckets(64);
    buckets[0].push_back(static_cast<uint32_t>(source));
    
    size_t current_bucket = 0;
    
    auto get_bucket = [&](double d) -> size_t {
        if (d < base_delta) return 0;
        if (d <= 0) return 0;
        double val = d / base_delta;
        double lg = std::log2(val);
        int k = static_cast<int>(std::floor(lg)) + 1;
        if (k < 0) k = 0; 
        if (k >= 64) k = 63;
        return static_cast<size_t>(k);
    };

    while (current_bucket < buckets.size()) {
        double strict_limit = base_delta * std::pow(2.0, current_bucket);
        
        while (!buckets[current_bucket].empty()) {
            std::vector<uint32_t> frontier = std::move(buckets[current_bucket]);
            buckets[current_bucket].clear();
            
            for (uint32_t u : frontier) {
                if (dist[u] >= strict_limit) continue; 
                 
                for (size_t i = g.offsets[u]; i < g.offsets[u + 1]; i++) {
                    uint32_t v = g.targets[i];
                    double nd = dist[u] + g.weights[i];
                    if (nd < dist[v]) {
                        dist[v] = nd;
                        size_t bucket_idx = get_bucket(nd);
                        if (bucket_idx >= buckets.size()) bucket_idx = buckets.size() - 1;
                        buckets[bucket_idx].push_back(v);
                    }
                }
            }
        }
        current_bucket++;
    }
    return dist;
}

} // namespace geometric_sssp

int main(int argc, char* argv[]) {
    using namespace geometric_sssp;
    
    if (argc < 2) return 1;
    
    try {
        auto edges = load_dimacs(argv[1]);
        Graph g; g.build(edges);
        
        double delta = g.mean_weight / 2.0;
        if (delta < 1.0) delta = 1.0;
        
        std::cout << "Geometric SSSP. Base Delta: " << delta << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto dist = geometric_delta_stepping(g, 0, delta);
        auto end = std::chrono::high_resolution_clock::now();
        
        std::cout << "Time: " << std::chrono::duration<double, std::milli>(end - start).count() << " ms" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
