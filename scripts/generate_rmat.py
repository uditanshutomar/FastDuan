#!/usr/bin/env python3
"""
Generate R-MAT (Recursive Matrix) graphs for benchmarking.
Standard Graph500 parameters: a=0.57, b=0.19, c=0.19
"""

import sys
import random

def generate_rmat(scale, edge_factor=16, a=0.57, b=0.19, c=0.19, seed=42):
    """Generate R-MAT graph with given parameters."""
    random.seed(seed)
    
    n = 1 << scale  # 2^scale vertices
    m = n * edge_factor
    
    edges = set()
    d = 1.0 - a - b - c
    
    for _ in range(m * 2):  # Generate extra to account for duplicates
        u, v = 0, 0
        for _ in range(scale):
            r = random.random()
            if r < a:
                pass  # top-left
            elif r < a + b:
                v += 1 << (scale - 1 - _)
            elif r < a + b + c:
                u += 1 << (scale - 1 - _)
            else:
                u += 1 << (scale - 1 - _)
                v += 1 << (scale - 1 - _)
        
        if u != v:  # No self-loops
            edges.add((u, v))
        
        if len(edges) >= m:
            break
    
    return n, list(edges)

def write_dimacs(n, edges, output_path, weight_range=(1, 1000)):
    """Write edges in DIMACS format with random weights."""
    random.seed(42)
    
    with open(output_path, 'w') as f:
        f.write(f"c R-MAT graph, scale={int(n).bit_length()-1}\n")
        f.write(f"p sp {n} {len(edges)}\n")
        for u, v in edges:
            w = random.randint(weight_range[0], weight_range[1])
            f.write(f"a {u+1} {v+1} {w}\n")
    
    print(f"Generated R-MAT: {n} nodes, {len(edges)} edges")
    print(f"Output: {output_path}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 generate_rmat.py <scale> <output.gr>")
        print("  scale: 2^scale vertices (e.g., 18 = 262K nodes)")
        sys.exit(1)
    
    scale = int(sys.argv[1])
    output = sys.argv[2]
    
    n, edges = generate_rmat(scale)
    write_dimacs(n, edges, output)
