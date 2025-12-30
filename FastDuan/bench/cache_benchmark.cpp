/*
 * Cache-Aware SSSP Benchmark Suite
 * For ALENEX 2026 Paper: "Cache-Conscious SSSP on Modern Architectures"
 *
 * Compares multiple SSSP implementations measuring cache behavior:
 * 1. Dijkstra with std::priority_queue (baseline)
 * 2. Dijkstra with CSR format
 * 3. Dijkstra with radix heap
 * 4. Delta-stepping (parallel-like bucketing)
 */

#include <vector>
#include <queue>
#include <cmath>
#include <limits>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdint>

namespace cache_sssp {

constexpr double INF = std::numeric_limits<double>::infinity();
constexpr uint32_t INVALID = UINT32_MAX;

// ============================================================================
// Graph Representations
// ============================================================================

// Adjacency List (baseline - pointer chasing)
struct AdjListGraph {
    size_t n, m;
    std::vector<std::vector<std::pair<uint32_t, float>>> adj;
    
    void build(size_t num_v, const std::vector<std::tuple<uint32_t, uint32_t, float>>& edges) {
        n = num_v;
        m = edges.size();
        adj.resize(n);
        for (const auto& [u, v, w] : edges) {
            adj[u].emplace_back(v, w);
        }
    }
};

// CSR Format (cache-friendly - sequential access)
struct CSRGraph {
    size_t n, m;
    std::vector<size_t> offsets;
    std::vector<uint32_t> targets;   // Separate arrays for better cache use
    std::vector<float> weights;
    
    void build(size_t num_v, const std::vector<std::tuple<uint32_t, uint32_t, float>>& edges) {
        n = num_v;
        m = edges.size();
        
        // Count degrees
        std::vector<size_t> degrees(n, 0);
        for (const auto& [u, v, w] : edges) {
            degrees[u]++;
        }
        
        // Build offsets
        offsets.resize(n + 1);
        offsets[0] = 0;
        for (size_t i = 0; i < n; i++) {
            offsets[i + 1] = offsets[i] + degrees[i];
        }
        
        // Build edge arrays
        targets.resize(m);
        weights.resize(m);
        std::vector<size_t> current(n, 0);
        for (const auto& [u, v, w] : edges) {
            size_t idx = offsets[u] + current[u]++;
            targets[idx] = v;
            weights[idx] = w;
        }
    }
};

// ============================================================================
// Priority Queue Variants
// ============================================================================

struct PQEntry {
    double dist;
    uint32_t vertex;
    bool operator>(const PQEntry& o) const { return dist > o.dist; }
};

// ============================================================================
// SSSP Algorithms
// ============================================================================

// Algorithm 1: Dijkstra + Adjacency List + std::priority_queue
std::vector<double> dijkstra_adjlist(const AdjListGraph& g, size_t source) {
    std::vector<double> dist(g.n, INF);
    dist[source] = 0;
    
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;
    pq.push({0.0, static_cast<uint32_t>(source)});
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        
        if (d > dist[u]) continue;
        
        for (const auto& [v, w] : g.adj[u]) {
            double nd = d + w;
            if (nd < dist[v]) {
                dist[v] = nd;
                pq.push({nd, v});
            }
        }
    }
    return dist;
}

// Algorithm 2: Dijkstra + CSR + std::priority_queue
std::vector<double> dijkstra_csr(const CSRGraph& g, size_t source) {
    std::vector<double> dist(g.n, INF);
    dist[source] = 0;
    
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;
    pq.push({0.0, static_cast<uint32_t>(source)});
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        
        if (d > dist[u]) continue;
        
        // Sequential access through CSR arrays
        for (size_t i = g.offsets[u]; i < g.offsets[u + 1]; i++) {
            uint32_t v = g.targets[i];
            double nd = d + g.weights[i];
            if (nd < dist[v]) {
                dist[v] = nd;
                pq.push({nd, v});
            }
        }
    }
    return dist;
}

// Algorithm 3: Dijkstra + CSR + Software Prefetching
std::vector<double> dijkstra_csr_prefetch(const CSRGraph& g, size_t source) {
    std::vector<double> dist(g.n, INF);
    dist[source] = 0;
    
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;
    pq.push({0.0, static_cast<uint32_t>(source)});
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        
        if (d > dist[u]) continue;
        
        size_t start = g.offsets[u];
        size_t end = g.offsets[u + 1];
        
        // Prefetch distance array entries
        for (size_t i = start; i < end && i < start + 8; i++) {
            __builtin_prefetch(&dist[g.targets[i]], 0, 1);
        }
        
        for (size_t i = start; i < end; i++) {
            // Prefetch ahead
            if (i + 8 < end) {
                __builtin_prefetch(&dist[g.targets[i + 8]], 0, 1);
            }
            
            uint32_t v = g.targets[i];
            double nd = d + g.weights[i];
            if (nd < dist[v]) {
                dist[v] = nd;
                pq.push({nd, v});
            }
        }
    }
    return dist;
}

// Algorithm 4: Delta-Stepping (bucket-based)
std::vector<double> delta_stepping(const CSRGraph& g, size_t source, double delta) {
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
// Graph Loading
// ============================================================================

std::vector<std::tuple<uint32_t, uint32_t, float>> load_dimacs(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Failed to open: " + filename);
    }
    
    std::vector<std::tuple<uint32_t, uint32_t, float>> edges;
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == 'c') continue;
        
        std::istringstream iss(line);
        char type;
        iss >> type;
        
        if (type == 'p') {
            std::string format;
            size_t n, m;
            iss >> format >> n >> m;
            edges.reserve(m);
        } else if (type == 'a') {
            uint32_t u, v;
            float w;
            iss >> u >> v >> w;
            edges.emplace_back(u - 1, v - 1, w);  // 0-indexed
        }
    }
    return edges;
}

// ============================================================================
// Benchmark Harness
// ============================================================================

struct BenchResult {
    std::string name;
    double avg_time_ms;
    size_t vertices_reached;
    double checksum;
};

template<typename Func>
BenchResult benchmark(const std::string& name, Func&& func, int trials, int warmup = 3) {
    std::vector<double> dist;
    
    // Warmup
    for (int i = 0; i < warmup; i++) {
        dist = func();
    }
    
    // Measured runs
    std::vector<double> times;
    times.reserve(trials);
    
    for (int i = 0; i < trials; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        dist = func();
        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }
    
    double sum = 0;
    for (double t : times) sum += t;
    double avg = sum / times.size();
    
    // Compute stats
    size_t reached = 0;
    double checksum = 0;
    for (double d : dist) {
        if (d < INF) {
            reached++;
            checksum += d;
        }
    }
    
    return {name, avg, reached, checksum};
}

} // namespace cache_sssp

int main(int argc, char* argv[]) {
    using namespace cache_sssp;
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <graph.gr> [trials]" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    int trials = (argc > 2) ? std::stoi(argv[2]) : 10;
    
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║          Cache-Aware SSSP Benchmark Suite                    ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    // Load graph
    std::cout << "Loading graph: " << filename << std::endl;
    auto edges = load_dimacs(filename);
    
    // Find max vertex
    uint32_t max_v = 0;
    for (const auto& [u, v, w] : edges) {
        max_v = std::max(max_v, std::max(u, v));
    }
    size_t num_vertices = max_v + 1;
    
    std::cout << "Vertices: " << num_vertices << ", Edges: " << edges.size() << std::endl;
    std::cout << "Trials: " << trials << " (+ 3 warmup)" << std::endl;
    std::cout << std::endl;
    
    // Build graph representations
    std::cout << "Building graph representations..." << std::endl;
    AdjListGraph adj_graph;
    adj_graph.build(num_vertices, edges);
    
    CSRGraph csr_graph;
    csr_graph.build(num_vertices, edges);
    
    std::cout << "  Adjacency List memory: ~" 
              << (adj_graph.adj.capacity() * sizeof(void*) + edges.size() * 8) / 1024 / 1024 
              << " MB" << std::endl;
    std::cout << "  CSR memory: ~" 
              << (csr_graph.offsets.size() * 8 + csr_graph.targets.size() * 4 + csr_graph.weights.size() * 4) / 1024 / 1024 
              << " MB" << std::endl;
    std::cout << std::endl;
    
    // Compute optimal delta for delta-stepping
    double max_weight = 0;
    for (const auto& [u, v, w] : edges) {
        max_weight = std::max(max_weight, static_cast<double>(w));
    }
    double delta = max_weight / 10;  // Heuristic
    
    // Run benchmarks
    std::cout << "Running benchmarks..." << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    std::vector<BenchResult> results;
    
    results.push_back(benchmark("1. Dijkstra + AdjList", [&]() {
        return dijkstra_adjlist(adj_graph, 0);
    }, trials));
    
    results.push_back(benchmark("2. Dijkstra + CSR", [&]() {
        return dijkstra_csr(csr_graph, 0);
    }, trials));
    
    results.push_back(benchmark("3. Dijkstra + CSR + Prefetch", [&]() {
        return dijkstra_csr_prefetch(csr_graph, 0);
    }, trials));
    
    results.push_back(benchmark("4. Delta-Stepping (δ=" + std::to_string(static_cast<int>(delta)) + ")", [&]() {
        return delta_stepping(csr_graph, 0, delta);
    }, trials));
    
    // Print results
    std::cout << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                        RESULTS                               ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(35) << "Algorithm" 
              << std::right << std::setw(12) << "Time (ms)"
              << std::setw(12) << "Speedup"
              << std::setw(12) << "Reached" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    double baseline = results[0].avg_time_ms;
    for (const auto& r : results) {
        std::cout << std::left << std::setw(35) << r.name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(12) << r.avg_time_ms
                  << std::setw(11) << (baseline / r.avg_time_ms) << "x"
                  << std::setw(12) << r.vertices_reached << std::endl;
    }
    
    std::cout << std::endl;
    
    // Verify correctness
    std::cout << "Correctness check (checksums should match):" << std::endl;
    for (const auto& r : results) {
        std::cout << "  " << r.name << ": " << std::fixed << std::setprecision(0) 
                  << r.checksum << std::endl;
    }
    
    return 0;
}
