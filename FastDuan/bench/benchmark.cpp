/*
 * FastDuan Benchmark
 * Compares FastDuan against baseline Dijkstra on DIMACS graphs
 */

#include "fast_duan.hpp"
#include "dimacs_loader.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace fastduan;
using namespace std::chrono;

// Baseline Dijkstra for comparison
class BaselineDijkstra {
public:
    explicit BaselineDijkstra(const Graph& g) : graph_(g), distances_(g.n, INF) {}
    
    void solve(size_t source) {
        std::fill(distances_.begin(), distances_.end(), INF);
        distances_[source] = 0.0;
        
        std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;
        pq.push({0.0, source});
        
        while (!pq.empty()) {
            auto [dist, u] = pq.top();
            pq.pop();
            
            if (dist > distances_[u]) continue;
            
            for (const Edge* e = graph_.begin(u); e != graph_.end(u); ++e) {
                double new_dist = dist + e->weight;
                if (new_dist < distances_[e->to]) {
                    distances_[e->to] = new_dist;
                    pq.push({new_dist, e->to});
                }
            }
        }
    }
    
    double distance(size_t v) const { return distances_[v]; }
    
private:
    const Graph& graph_;
    std::vector<double> distances_;
};

void benchmark(const std::string& name, auto& solver, const Graph& g, size_t source, int trials) {
    std::vector<double> times;
    times.reserve(trials);
    
    for (int i = 0; i < trials; i++) {
        auto start = high_resolution_clock::now();
        solver.solve(source);
        auto end = high_resolution_clock::now();
        
        double elapsed = duration<double>(end - start).count();
        times.push_back(elapsed);
        
        std::cout << "  Trial " << (i + 1) << ": " << std::fixed << std::setprecision(5) 
                  << elapsed << "s" << std::endl;
    }
    
    double sum = 0;
    for (double t : times) sum += t;
    double avg = sum / times.size();
    
    std::cout << "\n" << name << " Average Time: " << std::fixed << std::setprecision(5) 
              << avg << "s\n" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <graph.gr> [trials] [source]" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    int trials = (argc > 2) ? std::stoi(argv[2]) : 5;
    size_t source = (argc > 3) ? std::stoull(argv[3]) : 0;
    
    std::cout << "=== FastDuan Benchmark ===" << std::endl;
    std::cout << "Graph: " << filename << std::endl;
    std::cout << "Trials: " << trials << std::endl;
    std::cout << "Source: " << source << std::endl;
    std::cout << std::endl;
    
    // Load graph
    std::cout << "Loading graph..." << std::endl;
    auto load_start = high_resolution_clock::now();
    Graph g = DIMACSLoader::load(filename);
    auto load_end = high_resolution_clock::now();
    
    std::cout << "Graph loaded: " << g.n << " vertices, " << g.m << " edges" << std::endl;
    std::cout << "Load time: " << std::fixed << std::setprecision(4) 
              << duration<double>(load_end - load_start).count() << "s" << std::endl;
    std::cout << std::endl;
    
    // Benchmark Baseline Dijkstra
    std::cout << "--- Baseline Dijkstra ---" << std::endl;
    BaselineDijkstra dijkstra(g);
    benchmark("Dijkstra", dijkstra, g, source, trials);
    
    // Benchmark FastDuan
    std::cout << "--- FastDuan ---" << std::endl;
    FastDuanSolver fast_duan(g);
    benchmark("FastDuan", fast_duan, g, source, trials);
    
    // Verify correctness (sample a few vertices)
    std::cout << "--- Correctness Check ---" << std::endl;
    dijkstra.solve(source);
    fast_duan.solve(source);
    
    int errors = 0;
    for (size_t v = 0; v < std::min(g.n, size_t(1000)); v += 100) {
        double d1 = dijkstra.distance(v);
        double d2 = fast_duan.distance(v);
        if (std::abs(d1 - d2) > 1e-6) {
            std::cout << "MISMATCH at vertex " << v << ": Dijkstra=" << d1 
                      << ", FastDuan=" << d2 << std::endl;
            errors++;
        }
    }
    
    if (errors == 0) {
        std::cout << "All sampled distances match!" << std::endl;
    } else {
        std::cout << errors << " mismatches found!" << std::endl;
    }
    
    return 0;
}
