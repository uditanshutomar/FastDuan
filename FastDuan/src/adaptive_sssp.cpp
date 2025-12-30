/*
 * FastDuan: Adaptive SSSP (v2)
 */

#include "fast_duan/graph.hpp"
#include "fast_duan/io.hpp"
#include "fast_duan/algorithms.hpp" // Now includes implementation

#include <vector>
#include <iomanip>
#include <iostream>
#include <chrono>

int main(int argc, char* argv[]) {
    using namespace fast_duan;
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <graph.gr> [source]" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    size_t source = (argc > 2) ? std::stoull(argv[2]) : 0;
    
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║            Adaptive Delta-Stepping SSSP (v2)                 ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        auto edges = load_dimacs(filename);
        Graph g;
        g.build(edges);
        
        std::cout << "  Graph: " << g.n << "V, " << g.m << "E (" << g.type_name() << ")" << std::endl;
        std::cout << "  Est. Diameter: " << g.estimated_diameter << std::endl;
        
        double delta = select_delta(g);
        std::cout << "  Selected δ: " << static_cast<long>(delta) << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        // Use the library function
        auto dist = delta_stepping_seq(g, source, delta);
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        
        // Stats
        size_t reachable = 0;
        double max_dist = 0;
        for (double d : dist) if (d < INF) { reachable++; max_dist = std::max(max_dist, d); }
        
        std::cout << "  Time: " << std::fixed << std::setprecision(2) << time_ms << " ms" << std::endl;
        std::cout << "  Reachable: " << reachable << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
