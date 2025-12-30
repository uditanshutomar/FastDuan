/*
 * FastDuan Graph Structure
 * Shared CSR definitions and statistics
 */

#ifndef FAST_DUAN_GRAPH_HPP
#define FAST_DUAN_GRAPH_HPP

#include <vector>
#include <cstdint>
#include <tuple>
#include <algorithm>
#include <numeric>
#include <queue>
#include <cmath>
#include <iostream>

namespace fast_duan {

constexpr double INF = 1e18; // Use large value but safe for addition

struct Graph {
    size_t n = 0, m = 0;
    std::vector<size_t> offsets;
    std::vector<uint32_t> targets;
    std::vector<float> weights;
    
    // Statistics
    float min_weight = 0, max_weight = 0;
    float sum_weight = 0, mean_weight = 0;
        float median_weight = 0;
    float percentile_75 = 0;
    float avg_degree = 0;
    size_t estimated_diameter = 0;
    
    enum class Type { ROAD, SOCIAL, UNKNOWN };
    Type graph_type = Type::UNKNOWN;
    
    // Build from edge list
    void build(const std::vector<std::tuple<uint32_t, uint32_t, float>>& edges) {
        if (edges.empty()) return;
        
        uint32_t max_v = 0;
        for (const auto& [u, v, w] : edges) {
            max_v = std::max(max_v, std::max(u, v));
        }
        n = max_v + 1;
        m = edges.size();
        
        std::vector<size_t> degrees(n, 0);
        for (const auto& [u, v, w] : edges) degrees[u]++;
        
        offsets.resize(n + 1);
        offsets[0] = 0;
        for (size_t i = 0; i < n; i++) offsets[i + 1] = offsets[i] + degrees[i];
        
        targets.resize(m);
        weights.resize(m);
        std::vector<size_t> current(n, 0);
        for (const auto& [u, v, w] : edges) {
            size_t idx = offsets[u] + current[u]++;
            targets[idx] = v;
            weights[idx] = w;
        }
        
        compute_stats();
        classify();
    }
    
    void compute_stats() {
        if (m == 0) return;
        
        min_weight = *std::min_element(weights.begin(), weights.end());
        max_weight = *std::max_element(weights.begin(), weights.end());
        sum_weight = std::accumulate(weights.begin(), weights.end(), 0.0f);
        mean_weight = sum_weight / m;
        avg_degree = static_cast<float>(m) / n;
        
        // Median and 75th percentile
        std::vector<float> sorted = weights;
        // Sort fully to get arbitrary percentiles accurately
        std::sort(sorted.begin(), sorted.end());
        median_weight = sorted[m / 2];
        percentile_75 = sorted[static_cast<size_t>(m * 0.75)];
        
        estimate_diameter_bfs();
    }
    
    void estimate_diameter_bfs() {
        if (n == 0) return;
        std::vector<int> dist(n, -1);
        std::queue<uint32_t> q;
        
        // Pick start node
        uint32_t start_node = 0;
        for (size_t i = 0; i < n; i++) if (offsets[i+1] > offsets[i]) { start_node = i; break; }
        
        dist[start_node] = 0;
        q.push(start_node);
        int max_d = 0;
        
        while (!q.empty()) {
            uint32_t u = q.front(); q.pop();
            if (dist[u] > max_d) max_d = dist[u];
            
            for (size_t i = offsets[u]; i < offsets[u+1]; i++) {
                uint32_t v = targets[i];
                if (dist[v] == -1) {
                    dist[v] = dist[u] + 1;
                    q.push(v);
                }
            }
        }
        estimated_diameter = max_d;
    }
    
    void classify() {
        double weight_ratio = (min_weight > 0) ? max_weight / min_weight : max_weight;
        
        if (estimated_diameter > 100) {
            graph_type = Type::ROAD;
        } else if (estimated_diameter < 50 || avg_degree > 10) {
            graph_type = Type::SOCIAL;
        } else if (avg_degree < 5 && weight_ratio > 100) {
            graph_type = Type::ROAD;
        } else {
            graph_type = Type::SOCIAL;
        }
    }
    
    const char* type_name() const {
        switch (graph_type) {
            case Type::ROAD: return "ROAD";
            case Type::SOCIAL: return "SOCIAL";
            default: return "UNKNOWN";
        }
    }
};

} // namespace fast_duan

#endif // FAST_DUAN_GRAPH_HPP
