cmake_minimum_required(VERSION 3.16)
project(FastDuan VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Old Benchmark
add_executable(delta_heuristics bench/delta_heuristics.cpp)

# New Adaptive SSSP (v2)
add_executable(adaptive_sssp src/adaptive_sssp.cpp)

# Geometric Delta Stepping
add_executable(geometric_sssp src/geometric_sssp.cpp)

# Parallel SSSP (OpenMP)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    add_executable(parallel_sssp src/parallel_sssp.cpp)
    target_link_libraries(parallel_sssp PUBLIC OpenMP::OpenMP_CXX)
else()
    message(STATUS "OpenMP not found. Parallel build might fail without manual flags.")
    add_executable(parallel_sssp src/parallel_sssp.cpp)
    # Manual fallback for Apple Clang (Homebrew)
    if(APPLE AND EXISTS "/opt/homebrew/opt/libomp/include")
        target_include_directories(parallel_sssp PRIVATE "/opt/homebrew/opt/libomp/include")
        target_link_libraries(parallel_sssp PRIVATE "-L/opt/homebrew/opt/libomp/lib -lomp")
    endif()
endif()
