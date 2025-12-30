#!/bin/bash
# Run adaptive SSSP on all available graphs
# Usage: ./scripts/run_suite.sh [output_csv]

OUT=${1:-results_serial.csv}
BIN=FastDuan/adaptive_sssp

# Ensure binary exists
[ -f "$BIN" ] || { echo "Binary $BIN not found"; exit 1; }

echo "timestamp,graph,vertices,edges,avg_degree,est_diameter,class,delta,time_ms" > "$OUT"

# Find all .gr files
find graphs -name "*.gr" | sort | while read -r f; do
    name=$(basename "$f")
    echo "Benchmarking $name..."
    
    # Run 5 trials
    for i in {1..5}; do
        res=$($BIN "$f" 0)
        
        # Parse output
        vertices=$(echo "$res" | grep "Vertices:" | awk '{print $2}')
        edges=$(echo "$res" | grep "Edges:" | awk '{print $2}')
        deg=$(echo "$res" | grep "Avg degree:" | awk '{print $3}')
        diam=$(echo "$res" | grep "Est. Diameter:" | awk '{print $3}')
        class=$(echo "$res" | grep "Classified as:" | awk '{print $3}')
        delta=$(echo "$res" | grep "Selected" | awk '{print $3}')
        time=$(echo "$res" | grep "Time:" | awk '{print $2}')
        
        if [ -z "$time" ]; then
            echo "Error running $name"
            continue
        fi
        
        echo "$(date +%s),$name,$vertices,$edges,$deg,$diam,$class,$delta,$time" >> "$OUT"
    done
done

echo "Done. Results in $OUT"
