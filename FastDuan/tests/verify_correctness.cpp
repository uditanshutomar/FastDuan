/*
 * FastDuan Correctness Verification
 * Running randomized tests
 */

#include "fast_duan/graph.hpp"
#include "fast_duan/dijkstra.hpp"
#include "fast_duan/algorithms.hpp"

#include <iostream>
#include <vector>
#include <random>
#include <cassert>

using namespace fast_duan;

Graph generate_random_graph(size_t n, size_t m) {
    std::vector<std::tuple<uint32_t, uint32_t, float>> edges;
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist_v(0, n - 1);
    std::uniform_real_distribution<float> dist_w(1.0, 100.0);
    
    for (size_t i = 0; i < m; i++) {
        uint32_t u = dist_v(rng);
        uint32_t v = dist_v(rng);
        if (u != v) {
            edges.emplace_back(u, v, dist_w(rng));
        }
    }
    Graph g; g.build(edges);
    return g;
}

void verify(const std::vector<double>& result, const std::vector<double>& truth) {
    assert(result.size() == truth.size());
    for (size_t i = 0; i < result.size(); i++) {
        // Allow tiny floating point error
        double diff = std::abs(result[i] - truth[i]);
        if (diff > 1e-4 && !(result[i] > 1e17 && truth[i] > 1e17)) {
            std::cerr << "Mismatch at " << i << ": Got " << result[i] << ", Expected " << truth[i] << std::endl;
            exit(1);
        }
    }
}

int main() {
    std::cout << "Running Correctness Tests..." << std::endl;
    
    // Test 1: Small Random Graph (Adaptive Seq)
    {
        std::cout << "[Test 1] Random Graph (1000 nodes, 5000 edges) - Sequential... ";
        Graph g = generate_random_graph(1000, 5000);
        double delta = select_delta(g);
        
        auto truth = dijkstra_reference(g, 0);
        auto result = delta_stepping_seq(g, 0, delta);
        
        verify(result, truth);
        std::cout << "PASSED" << std::endl;
    }
    
    // Test 2: Parallel
    #if defined(_OPENMP)
    {
        std::cout << "[Test 2] Random Graph (5000 nodes, 20000 edges) - Parallel... ";
        Graph g = generate_random_graph(5000, 20000);
        double delta = select_delta(g);
        
        auto truth = dijkstra_reference(g, 0);
        auto result = delta_stepping_par(g, 0, delta);
        
        verify(result, truth);
        std::cout << "PASSED" << std::endl;
    }
    #endif

    std::cout << "All Tests Passed." << std::endl;
    return 0;
}
