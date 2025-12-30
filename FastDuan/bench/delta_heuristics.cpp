/*
 * Delta Heuristics Benchmark Suite
 * Research: "Simple Delta Heuristics for Delta-Stepping SSSP"
 */

#include "fast_duan/graph.hpp"
#include "fast_duan/io.hpp"

#include <vector>
#include <iomanip>
#include <iostream>
#include <chrono>

namespace delta_heuristics {

using namespace fast_duan;

// ============================================================================
// Delta Heuristics
// ============================================================================

struct Heuristic {
    std::string name;
    double (*compute)(const Graph& g);
};

double h1_weight_range(const Graph& g) {
    return (g.max_weight + g.min_weight) / 2.0;
}

double h2_median(const Graph& g) {
    return g.median_weight;
}

double h3_degree_weighted(const Graph& g) {
    return g.mean_weight * std::log2(g.avg_degree + 1);
}

double h4_diameter_estimate(const Graph& g) {
    // Legacy heuristic using log(n) proxy for diameter
    double est_diameter = std::log2(g.n);
    return g.sum_weight / (g.m * est_diameter);
}

double h5_percentile_75(const Graph& g) {
    return g.percentile_75;
}

double h6_gapbs_default(const Graph& g) {
    return g.max_weight / 10.0;
}

double h7_max_weight(const Graph& g) {
    return g.max_weight;
}

double h8_mean_weight(const Graph& g) {
    return g.mean_weight;
}

std::vector<Heuristic> get_heuristics() {
    return {
        {"H1: (max+min)/2", h1_weight_range},
        {"H2: median", h2_median},
        {"H3: mean×log(deg)", h3_degree_weighted},
        {"H4: sum/(m×log(n))", h4_diameter_estimate},
        {"H5: 75th percentile", h5_percentile_75},
        {"H6: max/10 (GAPBS)", h6_gapbs_default},
        {"H7: max_weight", h7_max_weight},
        {"H8: mean_weight", h8_mean_weight},
    };
}

// ============================================================================
// Delta-Stepping Algorithm (Local implementation needed for repeated trials)
// Or use adaptive_sssp::delta_stepping if moved to header?
// Algorithm state is complex, better to keep simple local here or move algo to header.
// MOVING ALGO TO HEADER IS BEST PRACTICE.
// Let's assume we keep it here for now to avoid include loop or large headers.
// ============================================================================

double delta_stepping(const Graph& g, size_t source, double delta) {
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
    
    double checksum = 0;
    for (double d : dist) {
        if (d < INF) checksum += d;
    }
    return checksum;
}

double measure_time(const Graph& g, double delta, int trials = 3) {
    std::vector<double> times;
    times.reserve(trials);
    
    for (int i = 0; i < trials; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        delta_stepping(g, 0, delta);
        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }
    
    std::sort(times.begin(), times.end());
    return times[trials / 2]; 
}

// ============================================================================
// Oracle Search
// ============================================================================

struct OracleResult {
    double optimal_delta;
    double optimal_time;
};

OracleResult find_oracle(const Graph& g) {
    std::cout << "  Finding oracle optimal δ..." << std::flush;
    
    std::vector<double> candidates;
    for (double mult = 0.01; mult <= 100; mult *= 2) {
        candidates.push_back(g.mean_weight * mult);
    }
    candidates.push_back(g.min_weight);
    candidates.push_back(g.max_weight);
    candidates.push_back(g.median_weight);
    
    double best_delta = candidates[0];
    double best_time = 1e9;
    
    for (double delta : candidates) {
        if (delta <= 0) continue;
        double t = measure_time(g, delta, 3);
        if (t < best_time) {
            best_time = t;
            best_delta = delta;
        }
    }
    
    // Fine-tune
    for (double mult = 0.5; mult <= 2.0; mult += 0.25) {
        double delta = best_delta * mult;
        double t = measure_time(g, delta, 3);
        if (t < best_time) {
            best_time = t;
            best_delta = delta;
        }
    }
    
    std::cout << " δ=" << static_cast<int>(best_delta) << " (" << best_time << "ms)" << std::endl;
    return {best_delta, best_time};
}

// ============================================================================
// Main Benchmark
// ============================================================================

struct BenchmarkResult {
    std::string graph_name;
    double oracle_delta;
    double oracle_time;
    std::vector<std::pair<std::string, double>> heuristic_slowdowns;
};

BenchmarkResult run_benchmark(const std::string& path, const std::string& name) {
    std::cout << "\n═══════════════════════════════════════════════════════════" << std::endl;
    std::cout << "Graph: " << name << std::endl;
    
    auto edges = load_dimacs(path);
    Graph g; g.build(edges);
    
    std::cout << " " << g.n << " vertices, " << g.m << " edges" << std::endl;
    std::cout << "  Est. Diameter: " << g.estimated_diameter << std::endl;
    
    auto oracle = find_oracle(g);
    
    std::cout << "\n  Heuristic Evaluation:" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    std::cout << std::left << std::setw(25) << "Heuristic" 
              << std::right << std::setw(10) << "δ"
              << std::setw(12) << "Time (ms)"
              << std::setw(10) << "Slowdown" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    BenchmarkResult result;
    result.graph_name = name;
    result.oracle_delta = oracle.optimal_delta;
    result.oracle_time = oracle.optimal_time;
    
    std::cout << std::left << std::setw(25) << "Oracle (optimal)"
              << std::right << std::fixed << std::setprecision(0) << std::setw(10) << oracle.optimal_delta
              << std::setprecision(2) << std::setw(12) << oracle.optimal_time
              << std::setw(10) << "1.00x" << std::endl;
    
    for (const auto& h : get_heuristics()) {
        double delta = h.compute(g);
        double time = measure_time(g, delta, 3);
        double slowdown = time / oracle.optimal_time;
        
        std::cout << std::left << std::setw(25) << h.name
                  << std::right << std::fixed << std::setprecision(0) << std::setw(10) << delta
                  << std::setprecision(2) << std::setw(12) << time
                  << std::setw(9) << slowdown << "x" << std::endl;
        
        result.heuristic_slowdowns.emplace_back(h.name, slowdown);
    }
    
    return result;
}

} // namespace delta_heuristics

int main(int argc, char* argv[]) {
    using namespace delta_heuristics;
    
    std::cout << "Delta Heuristics Benchmark Suite" << std::endl;
    
    std::vector<std::pair<std::string, std::string>> graphs;
    if (argc > 1) {
        for (int i = 1; i < argc; i++) graphs.emplace_back(argv[i], argv[i]);
    } else {
        std::cerr << "Usage: ./delta_heuristics <graph.gr>..." << std::endl;
        return 1;
    }
    
    for (const auto& [path, name] : graphs) {
        try {
            run_benchmark(path, name);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    return 0;
}
