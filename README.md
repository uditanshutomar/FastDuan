<div align="center">

# FastDuan

**High-Performance Parallel SSSP Engine**

*Adaptive Delta-Stepping · Slab Memory Allocator · Lock-Free Parallelism*

<br/>

![C++](https://img.shields.io/badge/C++20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![OpenMP](https://img.shields.io/badge/OpenMP-Parallel-FF6A00?style=for-the-badge)
![CMake](https://img.shields.io/badge/CMake-3.16+-064F8C?style=for-the-badge&logo=cmake&logoColor=white)

</div>

---

## Performance

| Benchmark | Graph | Time | Metric |
|:--|:--|--:|--:|
| **vs Reference Parallel** | RMAT-20 (1M nodes) | 116 ms vs 358 ms | **3.08x speedup** |
| **Large-Scale Throughput** | RMAT-24 (16.7M nodes, 268M edges) | 2.69 s | **100 MTEPS** |
| **Road Network Scaling** | USA-road-d.CAL (4 threads) | 551 ms vs 1238 ms serial | **2.25x parallel** |

> Benchmarked on Apple M3 (8 cores). MTEPS = Million Traversed Edges Per Second.

---

## What It Does

FastDuan solves the **Single-Source Shortest Path (SSSP)** problem on large-scale graphs. It beats standard parallel implementations by **3x** through three key innovations:

1. **Adaptive Delta Selection** — Classifies graphs at runtime (road vs. social network topology) using BFS diameter estimation, then selects optimal delta parameters automatically. No manual tuning.

2. **Slab Memory Engine** — Custom thread-local monotonic allocator that hands out 4KB blocks (page-aligned), eliminating `std::vector` resize overhead and lock contention during parallel traversal.

3. **Lock-Free Parallel Bucketing** — Private per-thread buckets with relaxed atomic distance updates. Scales across cores without shared-state bottlenecks.

---

## Architecture

```
FastDuan/
├── include/fast_duan/
│   ├── graph.hpp          # CSR graph structure + topology stats
│   ├── algorithms.hpp     # Delta-stepping (sequential + parallel)
│   ├── memory.hpp         # Slab allocator (thread-local blocks)
│   ├── dijkstra.hpp       # Reference Dijkstra baseline
│   └── io.hpp             # DIMACS graph loader
├── src/
│   ├── parallel_sssp.cpp  # OpenMP parallel engine (primary)
│   ├── adaptive_sssp.cpp  # Serial with auto delta selection
│   └── geometric_sssp.cpp # Geometric bucket-width variant
├── bench/
│   ├── benchmark.cpp      # Full benchmark suite
│   ├── baseline_dijkstra  # Binary-heap Dijkstra reference
│   ├── reference_sssp.cpp # Standard parallel (std::vector)
│   └── cache_benchmark    # Cache behavior profiling
├── tests/
│   └── verify_correctness # Validates against Dijkstra ground truth
└── scripts/
    ├── reproduce_paper.sh # One-command full reproduction
    ├── gen_rmat.cpp       # RMAT graph generator (Scale 20-24+)
    ├── run_suite.sh       # Batch benchmark runner
    └── analyze_results.py # Results visualization
```

---

## Quick Start

### Build

```bash
cd FastDuan
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Requires: **C++20 compiler** (clang++ or g++) and **OpenMP**.
On macOS with Homebrew: `brew install libomp`

### Run

```bash
# Parallel SSSP on a road network
OMP_NUM_THREADS=8 ./build/parallel_sssp graphs/road/USA-road-d.CAL.gr

# Adaptive SSSP (auto-selects delta)
./build/adaptive_sssp graphs/synthetic/rmat-20.gr

# Compare against baseline Dijkstra
./build/baseline_dijkstra graphs/synthetic/rmat-20.gr
```

### Generate Test Graphs

```bash
# Build RMAT generator
cmake --build build --target gen_rmat

# Generate Scale-20 graph (1M nodes, 16M edges)
./scripts/gen_rmat 20 16 graphs/synthetic/rmat-20.gr

# Generate Scale-24 graph (16.7M nodes, 268M edges, ~2.3 GB)
./scripts/gen_rmat 24 16 graphs/synthetic/rmat-24.gr
```

### Reproduce All Results

```bash
./scripts/reproduce_paper.sh
```

---

## Graph Format

DIMACS Challenge 9 `.gr` format:

```
c Comment line
p sp <nodes> <edges>
a <src> <dst> <weight>
```

Use `scripts/convert_snap.py` to convert SNAP edge-list graphs.

---

## How the Slab Allocator Works

Standard parallel delta-stepping uses `std::vector` for buckets — each `push_back` can trigger reallocation with global heap locks, serializing threads. FastDuan replaces this with:

- **Thread-local slabs**: Each thread owns a chain of 4KB blocks (1024 `uint32_t` entries = 1 OS page)
- **Monotonic allocation**: Blocks are never freed mid-traversal — lifetime matches the SSSP kernel
- **Zero contention**: No shared allocator locks, no false sharing, no `malloc` calls in the hot loop

This alone accounts for the 3x speedup on social-network graphs where bucket churn is highest.

---

## License

MIT
