#!/usr/bin/env python3
"""
Convert SNAP edgelist format to DIMACS .gr format with random weights.
Adds random integer weights [1, 1000] for meaningful delta-stepping analysis.
"""

import sys
import gzip
import random
from collections import defaultdict

def convert_snap_to_dimacs(input_path, output_path, weight_range=(1, 1000)):
    """Convert SNAP edgelist to DIMACS with random weights."""
    
    edges = []
    max_node = 0
    
    # Read edges
    opener = gzip.open if input_path.endswith('.gz') else open
    with opener(input_path, 'rt') as f:
        for line in f:
            if line.startswith('#'):
                continue
            parts = line.strip().split()
            if len(parts) >= 2:
                u, v = int(parts[0]), int(parts[1])
                max_node = max(max_node, u, v)
                edges.append((u, v))
    
    n = max_node + 1
    m = len(edges)
    
    # Write DIMACS format
    random.seed(42)  # Reproducible
    with open(output_path, 'w') as f:
        f.write(f"c Converted from SNAP format\n")
        f.write(f"c Original: {input_path}\n")
        f.write(f"p sp {n} {m}\n")
        for u, v in edges:
            w = random.randint(weight_range[0], weight_range[1])
            f.write(f"a {u+1} {v+1} {w}\n")  # 1-indexed
    
    print(f"Converted: {n} nodes, {m} edges")
    print(f"Output: {output_path}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 convert_snap.py input.txt[.gz] output.gr")
        sys.exit(1)
    
    convert_snap_to_dimacs(sys.argv[1], sys.argv[2])
