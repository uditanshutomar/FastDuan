/*
 * FastDuan Baseline: Optimized Dijkstra
 * Binary Heap Implementation for Performance Comparison
 */

#include "fast_duan/graph.hpp"
#include "fast_duan/io.hpp"

#include <vector>
#include <queue>
#include <iomanip>
#include <iostream>
#include <chrono>

namespace baseline {

using namespace fast_duan;

std::vector<double> dijkstra_optimized(const Graph& g, size_t source) {
    std::vector<double> dist(g.n, INF);
    dist[source] = 0;
    
    // Use std::priority_queue (Binary Heap)
    // Pair: <distance, node>
    using P = std::pair<double, uint32_t>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    
    pq.push({0.0, static_cast<uint32_t>(source)});
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        
        if (d > dist[u]) continue;
        
        for (size_t i = g.offsets[u]; i < g.offsets[u+1]; i++) {
            uint32_t v = g.targets[i];
            float w = g.weights[i];
            
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                pq.push({dist[v], v});
            }
        }
    }
    return dist;
}

} // namespace baseline

int main(int argc, char* argv[]) {
    using namespace fast_duan;
    if (argc < 2) return 1;
    
    try {
        auto edges = load_dimacs(argv[1]);
        Graph g; g.build(edges);
        
        std::cout << "Baseline Dijkstra on " << g.n << " nodes." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto dist = baseline::dijkstra_optimized(g, 0);
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
