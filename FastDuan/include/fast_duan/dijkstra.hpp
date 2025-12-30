/*
 * FastDuan Reference Implementation
 * Standard Dijkstra's Algorithm for Verification
 */

#ifndef FAST_DUAN_DIJKSTRA_HPP
#define FAST_DUAN_DIJKSTRA_HPP

#include "graph.hpp"
#include <vector>
#include <queue>
#include <limits>

namespace fast_duan {

inline std::vector<double> dijkstra_reference(const Graph& g, size_t source) {
    std::vector<double> dist(g.n, INF);
    dist[source] = 0;
    
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

} // namespace fast_duan

#endif // FAST_DUAN_DIJKSTRA_HPP
