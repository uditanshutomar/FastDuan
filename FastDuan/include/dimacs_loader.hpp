/*
 * FastDuan DIMACS Graph Loader
 * Reads DIMACS Challenge 9 format (.gr files)
 */

#ifndef DIMACS_LOADER_HPP
#define DIMACS_LOADER_HPP

#include "fast_duan.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>

namespace fastduan {

class DIMACSLoader {
public:
    static Graph load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        
        size_t num_vertices = 0;
        size_t num_edges = 0;
        std::vector<std::tuple<uint32_t, uint32_t, float>> edges;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            std::istringstream iss(line);
            char type;
            iss >> type;
            
            switch (type) {
                case 'c':
                    // Comment line - skip
                    break;
                    
                case 'p': {
                    // Problem line: p sp <nodes> <edges>
                    std::string format;
                    iss >> format >> num_vertices >> num_edges;
                    edges.reserve(num_edges);
                    break;
                }
                
                case 'a': {
                    // Arc line: a <from> <to> <weight>
                    uint32_t from, to;
                    float weight;
                    iss >> from >> to >> weight;
                    // Convert 1-indexed to 0-indexed
                    edges.emplace_back(from - 1, to - 1, weight);
                    break;
                }
                
                default:
                    // Unknown line type - skip
                    break;
            }
        }
        
        Graph g;
        g.build(num_vertices, edges);
        return g;
    }
};

} // namespace fastduan

#endif // DIMACS_LOADER_HPP
