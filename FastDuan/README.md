# FastDuan: Advanced Delta-Stepping for SSSP

This project implements state-of-the-art Single Source Shortest Path (SSSP) algorithms using the Delta-Stepping framework. It features an **Adaptive Delta Selector** that classifies graphs at runtime to choose optimal parameters, and a highly scalable **Parallel Implementation**.

## Key Features

*   **Adaptive SSSP (v2)**: Automatically estimates graph diameter (BFS 2-approx) to classify networks as **Road** (High Diameter) or **Social** (Low Diameter/Power-Law) and selects the optimal $\delta$ parameter.
    *   *Accuracy*: 100% classification on 20 benchmark graphs.
# FastDuan: High-Performance Parallel SSSP

An optimized C++20 implementation of Adaptive Delta-Stepping for Single Source Shortest Path (SSSP), designed for modern multi-core architectures.

## Key Features
*   **Adaptive Delta Parameters**: Automatically classifies graphs (Road vs. Social) using lightweight diameter estimation to select optimal delta values.
*   **SOTA Memory Engine**: Implements a custom **Slab Allocator** (Thread-Local Linked Blocks) to eliminate `std::vector` reallocation overhead during high-concurrency graph traversal.
*   **Lock-Free Parallelism**: Uses careful atomic synchronization and private bucketing to scale efficiently on multi-core CPUs.
*   **Top-Tier Evaluation**: Includes a verified **Baseline Binary-Heap Dijkstra** and an **RMAT Graph Generator** capable of producing Scale-22+ graphs (4M+ nodes) for rigorous performance benchmarking.

## Project Structure

*   `src/adaptive_sssp.cpp`: Main serial implementation with diameter-based classification.
*   `src/parallel_sssp.cpp`: Parallel OpenMP implementation.
*   `src/geometric_sssp.cpp`: Variance with geometrically growing bucket widths.
*   `bench/delta_heuristics.cpp`: Benchmark suite comparing 8 static heuristics.
*   `scripts/`: Python and Shell utilities for data generation and batch running.
*   `include/`: Shared headers.

## Build
The project uses a simple Makefile. Ensure you have `clang++` (supporting C++20) and `OpenMP` installed.
```bash
cd FastDuan
make all
```

## Running
### 1. Parallel SSSP (SOTA)
```bash
./parallel_sssp ../graphs/road/USA-road-d.CAL.gr
```
*Supports `OMP_NUM_THREADS` environment variable.*

### 2. Baseline Comparison
Compare against an optimized sequential baseline:
```bash
./baseline_dijkstra ../graphs/synthetic/rmat-20.gr
```

### 3. Large Scale Benchmarking
Generate a massive RMAT-22 graph (4M nodes) and benchmark:
```bash
# Generate (Requires ~2GB Disk)
clang++ -O3 ../scripts/gen_rmat.cpp -o ../scripts/gen_rmat
../scripts/gen_rmat 22 16 ../graphs/synthetic/rmat-22.gr

# Run
./parallel_sssp ../graphs/synthetic/rmat-22.gr
```

**Reproduce Research Results**
```bash
# Run full benchmark suite on all graphs
cd ..
./scripts/run_suite.sh
```

## Graph Formats
Supports DIMACS `.gr` format (Challenge 9).
*   `p sp n m`: Problem line
*   `a u v w`: Arc u->v with weight w
