import os
import argparse
import random
import heapq
import glob

def load_graph(dir_path):
    files = sorted(glob.glob(os.path.join(dir_path, "*.gr")))
    if not files:
        raise ValueError("No .gr files found")
    
    M = len(files)
    edges = {}
    max_node = 0
    
    for k, fpath in enumerate(files):
        with open(fpath, "r") as f:
            for line in f:
                if line.startswith("a "):
                    parts = line.strip().split()
                    u, v, cost = int(parts[1]), int(parts[2]), float(parts[3])
                    max_node = max(max_node, u, v)
                    if u not in edges:
                        edges[u] = {}
                    if v not in edges[u]:
                        edges[u][v] = [0]*M
                    edges[u][v][k] = cost
                    
    return max_node, M, edges

def dijkstra(edges, max_node, start, obj_idx):
    dist = {u: float('inf') for u in range(1, max_node + 1)}
    dist[start] = 0.0
    pq = [(0.0, start)]
    
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist[u]:
            continue
        
        if u in edges:
            for v, costs in edges[u].items():
                w = costs[obj_idx]
                if dist[u] + w < dist[v]:
                    dist[v] = dist[u] + w
                    heapq.heappush(pq, (dist[v], v))
                    
    return dist

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate MVH using landmarks")
    parser.add_argument("--map", type=str, required=True, help="Directory containing .gr files")
    parser.add_argument("--goal", type=int, required=True, help="Goal node (1-based)")
    parser.add_argument("-K", type=int, required=True, help="Number of landmarks")
    parser.add_argument("--seed", type=int, default=123)
    parser.add_argument("--out", type=str, required=True, help="Output .mvh file")
    
    args = parser.parse_args()
    random.seed(args.seed)
    
    max_node, M, edges = load_graph(args.map)
    
    landmarks = random.sample(range(1, max_node + 1), min(args.K, max_node))
    
    # Precompute distances from landmarks
    landmark_dists = []
    for l in landmarks:
        dists_for_l = []
        for k in range(M):
            d = dijkstra(edges, max_node, l, k)
            dists_for_l.append(d)
        landmark_dists.append(dists_for_l)
        
    with open(args.out, "w") as f:
        for s in range(1, max_node + 1):
            for i, l in enumerate(landmarks):
                # h_k(s) = dist_k(s, l) - dist_k(goal, l)
                # Since graph is undirected in our grids, dist(s, l) = dist(l, s)
                # But to be safe and admissible:
                # Actually, standard differential heuristic is |dist(s, l) - dist(goal, l)|
                h_vec = []
                valid = True
                for k in range(M):
                    ds = landmark_dists[i][k][s]
                    dg = landmark_dists[i][k][args.goal]
                    if ds == float('inf') or dg == float('inf'):
                        valid = False
                        break
                    val = max(0, int(abs(ds - dg)))
                    h_vec.append(val)
                
                if valid:
                    f.write(f"{s}")
                    for val in h_vec:
                        f.write(f"\t{val}")
                    f.write("\n")
    
    print(f"Generated {args.K} heuristics for {max_node} states in {args.out}")
