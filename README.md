# FastDuan

A C++20 and OpenMP implementation of single-source shortest paths, exploring how graph topology and memory allocation affect parallel performance.

The main implementation combines adaptive delta selection, per-thread buckets, atomic distance updates, and a custom slab allocator. It includes a binary-heap Dijkstra baseline and a separate vector-bucket parallel reference for comparison.

## Start here

- [Parallel implementation](FastDuan/src/parallel_sssp.cpp): work distribution, distance relaxation, and bucket processing
- [Memory allocator](FastDuan/include/fast_duan/memory.hpp): thread-local slabs and linked buckets
- [Graph representation](FastDuan/include/fast_duan/graph.hpp) and [delta selection](FastDuan/include/fast_duan/algorithms.hpp)
- [Correctness tests](FastDuan/tests/verify_correctness.cpp) and [benchmark notes](paper_context.md)

## Build and run a small example

Requires Make, a C++20 compiler, and OpenMP. Run the commands below from the repository root after cloning.

```bash
git clone https://github.com/uditanshutomar/FastDuan.git
cd FastDuan

# macOS with Apple Clang; install OpenMP once
brew install libomp
make -B -C FastDuan all
make -C FastDuan test
```

On Linux with GCC and its OpenMP runtime, replace the two Make commands with:

```bash
make -B -C FastDuan all CXX=g++
make -C FastDuan test CXX=g++
```

`-B` rebuilds every executable from source, including the graph generator. The repository also contains a prebuilt generator that may not match your platform.

```bash
mkdir -p graphs/synthetic
./scripts/gen_rmat 10 16 graphs/synthetic/rmat-10.gr
OMP_NUM_THREADS=4 ./FastDuan/parallel_sssp graphs/synthetic/rmat-10.gr
./FastDuan/baseline_dijkstra graphs/synthetic/rmat-10.gr
```

Both executables print elapsed time and a distance checksum. Matching checksums are a useful smoke check, not a substitute for comparing every distance. The existing test suite compares the sequential and parallel algorithms in `algorithms.hpp` against Dijkstra on seeded random graphs; it does not directly exercise the separate slab-based implementation in `parallel_sssp.cpp`.

## Recorded benchmark results

These are project-recorded measurements on an Apple M3 with 8 cores, documented in the [benchmark notes](paper_context.md). They are specific to those workloads and baselines, not a general claim of superiority over other SSSP libraries.

| Workload | Recorded result | Comparison |
| --- | --- | --- |
| RMAT-20, approximately 1M vertices | 116 ms vs. 358 ms | 3.08x faster than this repository's vector-bucket parallel reference |
| RMAT-24, approximately 16.7M vertices and 268M generated edge attempts | 2.69 seconds | Approximately 100 million edges/second |
| USA-road-d.CAL | 551 ms vs. 1,238 ms | 4-thread parallel vs. sequential execution |

For larger experiments, build first, then run:

```bash
bash scripts/reproduce_paper.sh
```

The script generates RMAT-20 and RMAT-22 workloads and writes `paper_results.csv`. It runs the California road-network case only when `graphs/road/USA-road-d.CAL.gr` is supplied. RMAT-24 is a separate experiment and is not run by this script. Large graphs require substantially more memory and disk than the small example above.

## Design choices

- **Adaptive delta selection:** use graph statistics to choose a bucket width rather than fixing one value for every workload
- **Thread-local allocation:** allocate vertex blocks from slabs to reduce repeated bucket allocation; slabs can still grow during a run
- **Private buckets and atomic distances:** reduce shared bucket contention while coordinating distance improvements
- **Monotonic memory:** retain blocks until the traversal ends, trading memory reuse within a run for simpler allocation

## Input format

The executables accept DIMACS Challenge 9 `.gr` files with nonnegative edge weights. Vertex identifiers in the file are one-based; the executables use the first vertex as the source.

```text
c Comment
p sp 3 3
a 1 2 4
a 2 3 2
a 1 3 9
```

Use [convert_snap.py](scripts/convert_snap.py) to convert SNAP edge lists.

## License

MIT
