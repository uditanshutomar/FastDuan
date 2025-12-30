/* 
 * Fast RMAT Graph Generator
 * Usage: ./gen_rmat <scale> <edge_factor> <output.gr>
 */

#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <algorithm>
#include <tuple>
#include <set>

// RMAT Parameters (Graph500)
const double A = 0.57;
const double B = 0.19;
const double C = 0.19;
// D = 1 - (A+B+C) = 0.05

struct Edge {
    uint32_t u, v;
    float w;
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <scale> <edge_factor> <output.gr>" << std::endl;
        return 1;
    }
    
    int scale = std::stoi(argv[1]);
    int edge_factor = std::stoi(argv[2]);
    std::string filename = argv[3];
    
    size_t n = 1ULL << scale;
    size_t m = n * edge_factor;
    
    std::cout << "Generating RMAT-" << scale << " (" << n << " nodes, " << m << " edges)..." << std::endl;
    
    std::vector<Edge> edges;
    edges.reserve(m);
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::uniform_int_distribution<int> w_dist(1, 100);
    
    for (size_t i = 0; i < m; i++) {
        uint32_t u = 0, v = 0;
        size_t step = n / 2;
        
        // RMAT recursion unrolled
        for (int j = 0; j < scale; j++) {
            double p = dist(rng);
            if (p < A) {
                // Top-Left (u, v stay)
            } else if (p < A + B) {
                v += step; // Top-Right
            } else if (p < A + B + C) {
                u += step; // Bottom-Left
            } else {
                u += step;
                v += step; // Bottom-Right
            }
            step /= 2;
        }
        
        if (u != v) {
            edges.push_back({u, v, (float)w_dist(rng)});
        }
    }
    
    std::cout << "Writing to " << filename << "..." << std::endl;
    std::ofstream out(filename);
    out << "p sp " << n << " " << edges.size() << "\n";
    for (const auto& e : edges) {
        out << "a " << e.u + 1 << " " << e.v + 1 << " " << e.w << "\n";
    }
    
    std::cout << "Done." << std::endl;
    return 0;
}
