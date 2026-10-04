#!/usr/bin/env python3
"""
Test generator for a small 5x5 3-objective grid graph.
"""
import os
import random

def generate_grid(rows, cols, num_objs, out_dir, seed=42):
    random.seed(seed)
    os.makedirs(out_dir, exist_ok=True)
    
    # 1-indexed node ids: (r, c) -> r * cols + c + 1
    def node_id(r, c):
        return r * cols + c + 1
    
    edges = []
    for r in range(rows):
        for c in range(cols):
            u = node_id(r, c)
            # Right neighbor
            if c + 1 < cols:
                v = node_id(r, c + 1)
                edges.append((u, v))
                edges.append((v, u))
            # Down neighbor
            if r + 1 < rows:
                v = node_id(r + 1, c)
                edges.append((u, v))
                edges.append((v, u))
                
    # Sort edges to ensure determinism
    edges.sort()
    
    # Generate costs for each objective
    # To create interesting Pareto fronts, make some objectives positively correlated
    # and some negatively correlated (trade-offs)
    edge_costs = []
    for (u, v) in edges:
        costs = []
        base = random.randint(1, 20)
        costs.append(base)
        for m in range(1, num_objs):
            if m % 2 == 1:
                # Trade-off / negative correlation
                cost = max(1, 25 - base + random.randint(-3, 3))
            else:
                # Random / positive
                cost = max(1, base + random.randint(1, 15))
            costs.append(cost)
        edge_costs.append(costs)
        
    for m in range(num_objs):
        file_path = os.path.join(out_dir, f"obj_{m}.gr")
        with open(file_path, "w", encoding="utf-8") as f:
            for i, (u, v) in enumerate(edges):
                f.write(f"a {u} {v} {edge_costs[i][m]}\n")
                
    print(f"Generated {len(edges)} directed edges on {rows}x{cols} grid with {num_objs} objectives in {out_dir}")

if __name__ == "__main__":
    test_dir = os.path.join("benchmarks", "instances", "test_grid_5x5_d3")
    generate_grid(5, 5, 3, test_dir)
