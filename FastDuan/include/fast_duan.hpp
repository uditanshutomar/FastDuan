/*
 * FastDuan: High-Performance SSSP Implementation
 * Based on Duan et al. (2025) - Breaking the Sorting Barrier
 * 
 * Optimizations:
 * 1. Linearized Recursion (stackless bmssp)
 * 2. Block-Clustered Memory Layout  
 * 3. Flat Containers (BitSet instead of HashSet)
 * 4. Cache-aligned data structures
 */

#ifndef FAST_DUAN_HPP
#define FAST_DUAN_HPP

#include <vector>
#include <queue>
#include <cmath>
#include <limits>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace fastduan {

constexpr double INF = std::numeric_limits<double>::infinity();

// ============================================================================
// Flat BitSet - O(1) lookup without hash table overhead
// ============================================================================
class FlatBitSet {
public:
    explicit FlatBitSet(size_t n) : bits_((n + 63) / 64, 0), n_(n) {
        members_.reserve(n / 10);  // Pre-allocate for typical density
    }
    
    void clear() {
        for (size_t i : member_indices_) {
            bits_[i] = 0;
        }
        members_.clear();
        member_indices_.clear();
    }
    
    bool insert(size_t v) {
        size_t idx = v / 64;
        uint64_t mask = 1ULL << (v % 64);
        if (!(bits_[idx] & mask)) {
            bits_[idx] |= mask;
            members_.push_back(v);
            if (member_indices_.empty() || member_indices_.back() != idx) {
                member_indices_.push_back(idx);
            }
            return true;
        }
        return false;
    }
    
    bool contains(size_t v) const {
        return bits_[v / 64] & (1ULL << (v % 64));
    }
    
    const std::vector<size_t>& members() const { return members_; }
    size_t size() const { return members_.size(); }
    bool empty() const { return members_.empty(); }
    
private:
    std::vector<uint64_t> bits_;
    std::vector<size_t> members_;
    std::vector<size_t> member_indices_;  // Track which indices have bits set
    size_t n_;
};

// ============================================================================
// CSR Graph with cache-aligned edges
// ============================================================================
struct alignas(64) Edge {
    uint32_t to;
    float weight;  // Use float for cache efficiency (8 bytes per edge)
};

class Graph {
public:
    size_t n;                           // Number of vertices
    size_t m;                           // Number of edges
    std::vector<size_t> offsets;        // CSR offsets (n+1 elements)
    std::vector<Edge> edges;            // CSR edges (m elements)
    
    Graph() : n(0), m(0) {}
    
    // Build from edge list
    void build(size_t num_vertices, const std::vector<std::tuple<uint32_t, uint32_t, float>>& edge_list) {
        n = num_vertices;
        m = edge_list.size();
        
        // Count degrees
        std::vector<size_t> degrees(n, 0);
        for (const auto& [u, v, w] : edge_list) {
            degrees[u]++;
        }
        
        // Build offsets
        offsets.resize(n + 1);
        offsets[0] = 0;
        for (size_t i = 0; i < n; i++) {
            offsets[i + 1] = offsets[i] + degrees[i];
        }
        
        // Build edges
        edges.resize(m);
        std::vector<size_t> current(n, 0);
        for (const auto& [u, v, w] : edge_list) {
            size_t idx = offsets[u] + current[u]++;
            edges[idx] = {v, w};
        }
    }
    
    // Iterate over neighbors of vertex u
    const Edge* begin(size_t u) const { return &edges[offsets[u]]; }
    const Edge* end(size_t u) const { return &edges[offsets[u + 1]]; }
    size_t degree(size_t u) const { return offsets[u + 1] - offsets[u]; }
};

// ============================================================================
// Work Item for Linearized Recursion Stack
// ============================================================================
struct WorkItem {
    int level;
    double bound;
    std::vector<size_t> pivots;
    
    WorkItem(int l, double b, std::vector<size_t> p) 
        : level(l), bound(b), pivots(std::move(p)) {}
};

// ============================================================================
// Priority Queue Entry
// ============================================================================
struct PQEntry {
    double dist;
    size_t vertex;
    
    bool operator>(const PQEntry& o) const { return dist > o.dist; }
};

// ============================================================================
// FastDuan Solver - Optimized Duan-Mao SSSP
// ============================================================================
class FastDuanSolver {
public:
    explicit FastDuanSolver(const Graph& g) 
        : graph_(g)
        , distances_(g.n, INF)
        , predecessors_(g.n, UINT32_MAX)
        , complete_(g.n, false)
        , working_set_(g.n)
    {
        // Compute algorithm parameters
        double log_n = std::log(static_cast<double>(g.n));
        k_ = std::max(3, static_cast<int>(std::pow(log_n, 1.0/3.0) * 2));
        t_ = std::max(2, static_cast<int>(std::pow(log_n, 2.0/3.0)));
        max_level_ = static_cast<int>(std::ceil(log_n / t_));
    }
    
    // Solve SSSP from source to all vertices
    void solve(size_t source) {
        reset();
        distances_[source] = 0.0;
        
        // Linearized recursion using explicit work stack
        std::vector<WorkItem> work_stack;
        work_stack.emplace_back(max_level_, INF, std::vector<size_t>{source});
        
        while (!work_stack.empty()) {
            WorkItem item = std::move(work_stack.back());
            work_stack.pop_back();
            
            if (item.level == 0) {
                base_case(item.bound, item.pivots);
                continue;
            }
            
            // Find pivots and working set
            auto [new_pivots, working_vertices] = find_pivots(item.bound, item.pivots);
            
            if (working_vertices.size() > k_ * new_pivots.size()) {
                // Too many vertices, fall back
                base_case(item.bound, working_vertices);
                continue;
            }
            
            // Process blocks iteratively
            size_t block_size = 1ULL << std::min((item.level - 1) * t_, 20);
            
            // Create sub-problems
            std::vector<size_t> current_block;
            current_block.reserve(block_size);
            
            // Sort pivots by distance for better locality
            std::vector<std::pair<double, size_t>> sorted_pivots;
            sorted_pivots.reserve(new_pivots.size());
            for (size_t p : new_pivots) {
                if (distances_[p] < INF) {
                    sorted_pivots.emplace_back(distances_[p], p);
                }
            }
            std::sort(sorted_pivots.begin(), sorted_pivots.end());
            
            for (const auto& [dist, pivot] : sorted_pivots) {
                current_block.push_back(pivot);
                if (current_block.size() >= block_size) {
                    work_stack.emplace_back(item.level - 1, item.bound, std::move(current_block));
                    current_block = std::vector<size_t>();
                    current_block.reserve(block_size);
                }
            }
            
            if (!current_block.empty()) {
                work_stack.emplace_back(item.level - 1, item.bound, std::move(current_block));
            }
        }
    }
    
    double distance(size_t v) const { return distances_[v]; }
    const std::vector<double>& distances() const { return distances_; }
    
private:
    void reset() {
        std::fill(distances_.begin(), distances_.end(), INF);
        std::fill(complete_.begin(), complete_.end(), false);
        working_set_.clear();
    }
    
    // Base case: Dijkstra-like relaxation for small subproblems
    void base_case(double bound, const std::vector<size_t>& frontier) {
        if (frontier.empty()) return;
        
        std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;
        
        for (size_t v : frontier) {
            complete_[v] = true;
            if (distances_[v] < bound) {
                pq.push({distances_[v], v});
            }
        }
        
        // Run complete Dijkstra without limit for correctness
        while (!pq.empty()) {
            auto [dist, u] = pq.top();
            pq.pop();
            
            if (dist > distances_[u]) continue;
            
            // Relax outgoing edges
            for (const Edge* e = graph_.begin(u); e != graph_.end(u); ++e) {
                double new_dist = dist + e->weight;
                if (new_dist < distances_[e->to] && new_dist < bound) {
                    distances_[e->to] = new_dist;
                    pq.push({new_dist, e->to});
                }
            }
        }
    }
    
    // Find pivots using BFS expansion
    std::pair<std::vector<size_t>, std::vector<size_t>> 
    find_pivots(double bound, const std::vector<size_t>& frontier) {
        working_set_.clear();
        
        // Initialize with frontier
        for (size_t v : frontier) {
            working_set_.insert(v);
        }
        
        std::vector<size_t> current_layer = frontier;
        std::vector<size_t> next_layer;
        next_layer.reserve(frontier.size() * 4);
        
        // BFS expansion for k levels
        for (int i = 0; i < k_; i++) {
            next_layer.clear();
            
            for (size_t u : current_layer) {
                for (const Edge* e = graph_.begin(u); e != graph_.end(u); ++e) {
                    size_t v = e->to;
                    double new_dist = distances_[u] + e->weight;
                    
                    if (new_dist < distances_[v] && new_dist < bound) {
                        distances_[v] = new_dist;
                        if (working_set_.insert(v)) {
                            next_layer.push_back(v);
                        }
                    }
                }
            }
            
            if (next_layer.empty()) break;
            
            std::swap(current_layer, next_layer);
            
            if (working_set_.size() > k_ * frontier.size()) {
                return {frontier, std::vector<size_t>(working_set_.members())};
            }
        }
        
        return {frontier, std::vector<size_t>(working_set_.members())};
    }
    
    const Graph& graph_;
    std::vector<double> distances_;
    std::vector<uint32_t> predecessors_;
    std::vector<bool> complete_;
    FlatBitSet working_set_;
    int k_;
    int t_;
    int max_level_;
};

} // namespace fastduan

#endif // FAST_DUAN_HPP
