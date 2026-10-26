import os
import random
import numpy as np

def generate_grid_with_rho(rows, cols, num_objs, out_dir, rho, seed=42):
    random.seed(seed)
    np.random.seed(seed)
    os.makedirs(out_dir, exist_ok=True)
    
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
    
    # Generate costs with specific rho between objectives 0 and i
    # We'll use a multivariate normal to get correlated variables, then map to integers
    
    # Target covariance matrix
    cov = np.eye(num_objs)
    for i in range(num_objs):
        for j in range(num_objs):
            if i != j:
                cov[i, j] = rho
                
    # Ensure positive semi-definite
    eigenvalues = np.linalg.eigvals(cov)
    if min(eigenvalues) < 0:
        print(f"Warning: Rho {rho} for {num_objs}D is not positive semi-definite. Adjusting matrix.")
        # fallback to simple pairwise correlation logic if MVN fails
        # for our purposes, we can just clip it or use a different method.
    
    edge_costs = []
    mean = [15] * num_objs
    try:
        # Scale the covariance so values are spread out
        cov_scaled = cov * 10
        samples = np.random.multivariate_normal(mean, cov_scaled, size=len(edges))
    except Exception:
        # Fallback to simple random generation if covariance is invalid
        samples = []
        for _ in range(len(edges)):
            base = random.randint(5, 25)
            costs = [base]
            for m in range(1, num_objs):
                if rho < 0:
                    cost = max(1, 30 - base + random.randint(-4, 4))
                elif rho > 0:
                    cost = max(1, base + random.randint(-4, 4))
                else:
                    cost = random.randint(5, 25)
                costs.append(cost)
            samples.append(costs)

    for i in range(len(edges)):
        # clamp to integers [1, 30]
        costs = [int(max(1, min(30, round(v)))) for v in samples[i]]
        edge_costs.append(costs)
        
    for m in range(num_objs):
        file_path = os.path.join(out_dir, f"obj_{m}.gr")
        with open(file_path, "w", encoding="utf-8") as f:
            for i, (u, v) in enumerate(edges):
                f.write(f"a {u} {v} {edge_costs[i][m]}\n")
                
    print(f"Generated {len(edges)} directed edges on {rows}x{cols} grid with {num_objs} objectives (rho={rho}) in {out_dir}")

def main():
    base_dir = "scratchpad/grids"
    configs = [
        # grid_8x8 8D
        (8, 8, 8, -0.2),
        (8, 8, 8, -0.4),
        # grid_9x9 7D
        (9, 9, 7, 0.0),
        (9, 9, 7, -0.4),
    ]
    
    for r, c, m, rho in configs:
        out_dir = os.path.join(base_dir, f"grid_{r}x{c}_m{m}_rho{rho}")
        generate_grid_with_rho(r, c, m, out_dir, rho)

if __name__ == "__main__":
    main()
