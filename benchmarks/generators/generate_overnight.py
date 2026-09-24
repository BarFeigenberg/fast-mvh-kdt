import os
import random
import heapq
import multiprocessing
import itertools

def generate_grid_edges(rows, cols, M, rho, seed):
    random.seed(seed)
    num_nodes = rows * cols
    edges_structure = []
    for r in range(rows):
        for c in range(cols):
            u = r * cols + c
            if c + 1 < cols:
                edges_structure.append((u, u + 1))
                edges_structure.append((u + 1, u))
            if r + 1 < rows:
                edges_structure.append((u, u + cols))
                edges_structure.append((u + cols, u))
    
    obj_edges = [[] for _ in range(M)]
    for u, v in edges_structure:
        base = 1.0 + random.randint(0, 19)
        costs = [base]
        for k in range(1, M):
            indep = float(random.randint(0, 19))
            val = rho * base + (1.0 - rho) * indep
            if val < 1.0: val = 1.0
            costs.append(int(val))
        for k in range(M):
            obj_edges[k].append((u, v, costs[k]))
            
    return num_nodes, len(edges_structure), obj_edges

def dijkstra(edges_k, max_node, start):
    dist = {u: float('inf') for u in range(1, max_node + 1)}
    dist[start] = 0.0
    pq = [(0.0, start)]
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist[u]: continue
        if u in edges_k:
            for v, w in edges_k[u].items():
                if dist[u] + w < dist[v]:
                    dist[v] = dist[u] + w
                    heapq.heappush(pq, (dist[v], v))
    return dist

def generate_and_save(args):
    N, M, rho = args
    out_dir = f"benchmarks/instances/overnight/grid{N}x{N}_M{M}_rho{rho}"
    os.makedirs(out_dir, exist_ok=True)
    
    seed = N * M + int(abs(rho)*100)
    num_nodes, num_edges, obj_edges = generate_grid_edges(N, N, M, rho, seed)
    
    # Save .gr files
    for k in range(M):
        fname = os.path.join(out_dir, f"c{k+1}.gr")
        with open(fname, "w") as f:
            f.write(f"p sp {num_nodes} {num_edges}\n")
            for u, v, cost in obj_edges[k]:
                f.write(f"a {u+1} {v+1} {int(cost)}\n")
    
    # Pre-build adjacency for Dijkstra
    adj = [{} for _ in range(M)]
    for k in range(M):
        for u, v, cost in obj_edges[k]:
            u1, v1 = u+1, v+1
            if u1 not in adj[k]: adj[k][u1] = {}
            adj[k][u1][v1] = cost
            
    # Max K is 100, we need landmarks
    random.seed(seed)
    max_K = 100
    landmarks = random.sample(range(1, num_nodes + 1), max_K)
    
    # Compute all distances from landmarks
    landmark_dists = []
    for l in landmarks:
        dists_for_l = []
        for k in range(M):
            dists_for_l.append(dijkstra(adj[k], num_nodes, l))
        landmark_dists.append(dists_for_l)
        
    goal = num_nodes # always bottom right
    
    # Save for K = 10, 25, 50, 100
    for K in [10, 25, 50, 100]:
        mvh_path = os.path.join(out_dir, f"mvh_K{K}.mvh")
        with open(mvh_path, "w") as f:
            for s in range(1, num_nodes + 1):
                for i in range(K):
                    h_vec = []
                    valid = True
                    for k in range(M):
                        ds = landmark_dists[i][k][s]
                        dg = landmark_dists[i][k][goal]
                        if ds == float('inf') or dg == float('inf'):
                            valid = False
                            break
                        h_vec.append(max(0, int(abs(ds - dg))))
                    if valid:
                        f.write(f"{s}")
                        for val in h_vec:
                            f.write(f"\t{val}")
                        f.write("\n")
    
    print(f"Generated {out_dir} with K=10,25,50,100")

def main():
    Ns = [10, 20, 30, 40, 50]
    Ms = [3, 4, 5, 6, 7, 8]
    rhos = [0.0, -0.3, -0.6]
    
    tasks = list(itertools.product(Ns, Ms, rhos))
    print(f"Total configurations to generate: {len(tasks)}")
    
    # Use multiprocessing to generate in parallel
    pool_size = multiprocessing.cpu_count()
    with multiprocessing.Pool(pool_size) as pool:
        pool.map(generate_and_save, tasks)

if __name__ == "__main__":
    main()
