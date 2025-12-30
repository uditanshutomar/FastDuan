/*
 * FastDuan I/O Utilities
 * DIMACS Format Loading
 */

#ifndef FAST_DUAN_IO_HPP
#define FAST_DUAN_IO_HPP

#include <vector>
#include <tuple>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdint>

namespace fast_duan {

inline std::vector<std::tuple<uint32_t, uint32_t, float>> load_dimacs(const std::string& filename) {
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
            edges.emplace_back(u - 1, v - 1, w);
        }
    }
    return edges;
}

} // namespace fast_duan

#endif // FAST_DUAN_IO_HPP
