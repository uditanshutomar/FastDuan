#!/bin/bash
# FastDuan Reproduction Script
# Generates Table 1 and Table 2 data for the paper.

set -e

# Build
echo "Building project..."
cd "$(dirname "$0")/.."
# We are in 'Algo paper' root. Makefile is in 'FastDuan'.
make -C FastDuan all

BASE_DIR=$(pwd)
# Executables are in FastDuan/
BIN_DIR="$BASE_DIR/FastDuan"
DATA_DIR="$BASE_DIR/graphs"
mkdir -p "$DATA_DIR/synthetic"
mkdir -p "$DATA_DIR/road"

RESULTS_FILE="$BASE_DIR/paper_results.csv"
echo "Algorithm,Graph,Nodes,Time_ms" > "$RESULTS_FILE"

# Function to run and log
run_bench() {
    ALGO=$1
    GRAPH=$2
    NAME=$(basename "$GRAPH")
    echo "Running $ALGO on $NAME..."
    
    if [ ! -f "$GRAPH" ]; then
        echo "Graph $GRAPH not found, skipping."
        return
    fi
    
    OUTPUT=$($1 "$GRAPH")
    TIME=$(echo "$OUTPUT" | grep "Time:" | awk '{print $2}')
    NODES=$(echo "$OUTPUT" | grep "nodes" | awk '{print $4}' | tr -d '.')
    
    echo "$ALGO,$NAME,$NODES,$TIME" >> "$RESULTS_FILE"
    echo "  -> Time: ${TIME}ms"
}

# 1. RMAT-20 (Competitive Analysis)
echo "--- Experiment 1: RMAT-20 Competitive Analysis ---"
# Ensure RMAT-20
if [ ! -f "$DATA_DIR/synthetic/rmat-20.gr" ]; then
    echo "Generating RMAT-20..."
    $BASE_DIR/scripts/gen_rmat 20 16 "$DATA_DIR/synthetic/rmat-20.gr"
fi

run_bench "$BIN_DIR/baseline_dijkstra" "$DATA_DIR/synthetic/rmat-20.gr"
run_bench "$BIN_DIR/reference_sssp" "$DATA_DIR/synthetic/rmat-20.gr"
OMP_NUM_THREADS=8 run_bench "$BIN_DIR/parallel_sssp" "$DATA_DIR/synthetic/rmat-20.gr"

# 2. Road Network (Scalability)
echo "--- Experiment 2: Road Network Scalability ---"
# Assuming CAL exists (it was in user env)
GRAPH_CAL="$DATA_DIR/road/USA-road-d.CAL.gr"
if [ -f "$GRAPH_CAL" ]; then
    run_bench "$BIN_DIR/adaptive_sssp" "$GRAPH_CAL" # Serial
    OMP_NUM_THREADS=4 run_bench "$BIN_DIR/parallel_sssp" "$GRAPH_CAL" # Parallel
else
    echo "USA-road-d.CAL.gr not found in $DATA_DIR/road. Skipping."
fi

# 3. Large Scale (RMAT-22)
echo "--- Experiment 3: Large Scale (RMAT-22) ---"
if [ ! -f "$DATA_DIR/synthetic/rmat-22.gr" ]; then
    echo "Generating RMAT-22 (4M nodes)..."
    $BIN_DIR/../scripts/gen_rmat 22 16 "$DATA_DIR/synthetic/rmat-22.gr"
fi

# Only run FastDuan and Baseline on Large Scale
run_bench "$BIN_DIR/baseline_dijkstra" "$DATA_DIR/synthetic/rmat-22.gr"
OMP_NUM_THREADS=8 run_bench "$BIN_DIR/parallel_sssp" "$DATA_DIR/synthetic/rmat-22.gr"

echo "Done. Results saved to $RESULTS_FILE"
cat "$RESULTS_FILE"
