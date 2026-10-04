#!/usr/bin/env python3
"""
generate_instances.py - Benchmark Instance Generator

Generates multi-objective benchmark instances in standard DIMACS .gr format:
1. Grid graphs of varying dimensions (10x10 to 50x50)
2. Random geometric graphs (100 to 1000 nodes)
3. Multi-objective edge costs (3, 4, 5 objectives) with independent and anti-correlated trade-offs.
"""

import math
import os
import random
from typing import List, Tuple

def save_instance_gr(out_dir: str, num_nodes: int, edges: List[Tuple[int, int]], edge_costs: List[List[int]]):
    """Save multi-objective edge costs into obj_0.gr, obj_1.gr, etc."""
    os.makedirs(out_dir, exist_ok=True)
    num_objs = len(edge_costs[0])
    
    for m in range(num_objs):
        fpath = os.path.join(out_dir, f"obj_{m}.gr")
        with open(fpath, "w", encoding="utf-8") as f:
            for (u, v), costs in zip(edges, edge_costs):
                f.write(f"a {u} {v} {costs[m]}\n")

def generate_grid_graph(rows: int, cols: int, num_objs: int, out_dir: str, correlation: str = "tradeoff", seed: int = 42):
    random.seed(seed)
    def node_id(r, c):
        return r * cols + c + 1

    edges = []
    for r in range(rows):
        for c in range(cols):
            u = node_id(r, c)
            if c + 1 < cols:
                v = node_id(r, c + 1)
                edges.append((u, v))
                edges.append((v, u))
            if r + 1 < rows:
                v = node_id(r + 1, c)
                edges.append((u, v))
                edges.append((v, u))

    edges.sort()
    edge_costs = []
    for (u, v) in edges:
        costs = []
        base = random.randint(2, 30)
        costs.append(base)
        for m in range(1, num_objs):
            if correlation == "tradeoff":
                if m % 2 == 1:
                    cost = max(1, 35 - base + random.randint(-4, 4))
                else:
                    cost = max(1, base + random.randint(1, 15))
            else: # uniform independent
                cost = random.randint(2, 30)
            costs.append(cost)
        edge_costs.append(costs)

    num_nodes = rows * cols
    save_instance_gr(out_dir, num_nodes, edges, edge_costs)
    print(f"Generated Grid {rows}x{cols} ({num_nodes} nodes, {len(edges)} edges, {num_objs} objs, {correlation}) -> {out_dir}")

def generate_geometric_graph(num_nodes: int, radius: float, num_objs: int, out_dir: str, correlation: str = "tradeoff", seed: int = 42):
    random.seed(seed)
    coords = [(random.random(), random.random()) for _ in range(num_nodes)]
    edges = []
    for i in range(num_nodes):
        for j in range(num_nodes):
            if i != j:
                dx = coords[i][0] - coords[j][0]
                dy = coords[i][1] - coords[j][1]
                dist = math.hypot(dx, dy)
                if dist <= radius:
                    edges.append((i + 1, j + 1)) # 1-indexed

    # Ensure connectedness by adding spanning tree edges if needed
    for i in range(1, num_nodes):
        # connect to nearest lower index if no outgoing edge
        has_edge = any(u == i + 1 for (u, v) in edges)
        if not has_edge:
            edges.append((i + 1, i))
            edges.append((i, i + 1))

    edges = sorted(list(set(edges)))
    edge_costs = []
    for (u, v) in edges:
        p1 = coords[u - 1]
        p2 = coords[v - 1]
        eucl = int(math.hypot(p1[0] - p2[0], p1[1] - p2[1]) * 100) + 1
        costs = [eucl]
        for m in range(1, num_objs):
            if correlation == "tradeoff":
                if m % 2 == 1:
                    cost = max(1, 120 - eucl + random.randint(-5, 5))
                else:
                    cost = max(1, eucl + random.randint(2, 20))
            else:
                cost = random.randint(1, 100)
            costs.append(cost)
        edge_costs.append(costs)

    save_instance_gr(out_dir, num_nodes, edges, edge_costs)
    print(f"Generated Geometric Graph ({num_nodes} nodes, {len(edges)} edges, {num_objs} objs) -> {out_dir}")

def main():
    base_dir = os.path.join("benchmarks", "instances")
    os.makedirs(base_dir, exist_ok=True)

    # 1. Grids: 10x10, 15x15, 20x20, 25x25, 30x30, 40x40, 50x50 with 3, 4, 5 objectives
    grid_configs = [
        (10, 10, 3, "tradeoff"),
        (10, 10, 4, "tradeoff"),
        (10, 10, 5, "tradeoff"),
        (15, 15, 3, "tradeoff"),
        (15, 15, 4, "tradeoff"),
        (15, 15, 5, "tradeoff"),
        (20, 20, 3, "tradeoff"),
        (20, 20, 4, "tradeoff"),
        (25, 25, 3, "tradeoff"),
        (25, 25, 3, "uniform"),
        (30, 30, 3, "tradeoff"),
        (40, 40, 3, "tradeoff"),
        (50, 50, 3, "tradeoff"),
    ]

    for r, c, d, corr in grid_configs:
        name = f"grid_{r}x{c}_d{d}_{corr}"
        generate_grid_graph(r, c, d, os.path.join(base_dir, name), correlation=corr)

    # 2. Geometric graphs: 100, 250, 500 nodes
    geom_configs = [
        (100, 0.25, 3, "tradeoff"),
        (100, 0.25, 4, "tradeoff"),
        (100, 0.25, 5, "tradeoff"),
        (250, 0.16, 3, "tradeoff"),
        (250, 0.16, 4, "tradeoff"),
        (500, 0.12, 3, "tradeoff"),
    ]

    for n, rad, d, corr in geom_configs:
        name = f"geom_n{n}_d{d}_{corr}"
        generate_geometric_graph(n, rad, d, os.path.join(base_dir, name), correlation=corr)

if __name__ == "__main__":
    main()
