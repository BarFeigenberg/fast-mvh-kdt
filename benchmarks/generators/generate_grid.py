import os
import sys
import argparse
import random

def write_gr_file(fname, edges, num_nodes, num_edges):
    with open(fname, "w") as f:
        f.write(f"p sp {num_nodes} {num_edges}\n")
        for u, v, cost in edges:
            f.write(f"a {u+1} {v+1} {int(cost)}\n")  # DIMACS uses 1-based indexing for nodes typically, wait, let's check ex1-c1.gr.

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate synthetic multi-objective grid graph")
    parser.add_argument("--rows", type=int, required=True)
    parser.add_argument("--cols", type=int, required=True)
    parser.add_argument("-M", type=int, required=True, help="Number of objectives")
    parser.add_argument("--rho", type=float, required=True, help="Correlation coefficient")
    parser.add_argument("--seed", type=int, default=123)
    parser.add_argument("--out-dir", type=str, required=True)
    args = parser.parse_args()

    random.seed(args.seed)

    rows, cols, M, rho = args.rows, args.cols, args.M, args.rho

    num_nodes = rows * cols
    
    # Pre-generate edge structures
    # 4-connected grid
    edges_structure = []
    for r in range(rows):
        for c in range(cols):
            u = r * cols + c
            # east
            if c + 1 < cols:
                v = u + 1
                edges_structure.append((u, v))
                edges_structure.append((v, u))
            # south
            if r + 1 < rows:
                v = u + cols
                edges_structure.append((u, v))
                edges_structure.append((v, u))

    num_edges = len(edges_structure)
    
    # For each edge, generate M costs
    obj_edges = [[] for _ in range(M)]

    for u, v in edges_structure:
        # Generate cost for this edge
        base = 1.0 + random.randint(0, 19)
        c0 = base
        costs = [c0]
        for k in range(1, M):
            indep = float(random.randint(0, 19))
            val = rho * base + (1.0 - rho) * indep
            if val < 1.0:
                val = 1.0
            costs.append(int(val))
        
        for k in range(M):
            obj_edges[k].append((u, v, costs[k]))

    os.makedirs(args.out_dir, exist_ok=True)

    for k in range(M):
        fname = os.path.join(args.out_dir, f"c{k+1}.gr")
        write_gr_file(fname, obj_edges[k], num_nodes, num_edges)
    
    print(f"Generated {M} objectives in {args.out_dir}")
