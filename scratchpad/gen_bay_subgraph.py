import os, random
from collections import deque

M_total = 8
size = 2000
center = 160636
seed = 42

random.seed(seed)

def load_graph():
    print("Loading edges...")
    edges = {} # u -> {v: [d, t]}
    for k, fn in enumerate(["BAY-d.gr", "BAY-t.gr"]):
        with open(f"scratchpad/maps/bay/{fn}", "r") as f:
            for line in f:
                if line.startswith("a "):
                    _, u, v, w = line.split()
                    u, v, w = int(u), int(v), int(w)
                    if u not in edges: edges[u] = {}
                    if v not in edges[u]: edges[u][v] = [-1, -1]
                    edges[u][v][k] = w
    
    # We will do BFS on undirected graph equivalent to find connected component
    adj = {}
    for u in edges:
        for v in edges[u]:
            if u not in adj: adj[u] = set()
            if v not in adj: adj[v] = set()
            adj[u].add(v)
            adj[v].add(u)
            
    return edges, adj

edges, adj = load_graph()

print("Running BFS...")
visited = [center]
queue = deque([center])
visited_set = {center}

while queue and len(visited) < size:
    u = queue.popleft()
    for v in sorted(list(adj.get(u, []))): # sorted for determinism
        if v not in visited_set:
            visited_set.add(v)
            visited.append(v)
            queue.append(v)
            if len(visited) >= size:
                break

print(f"Visited {len(visited)} nodes.")
# Map old ID -> new ID (1 to size)
mapping = {old_id: new_id + 1 for new_id, old_id in enumerate(visited)}
goal_new = mapping[center]
start_new = mapping[visited[-1]]

print(f"Goal: {goal_new}, Start: {start_new}")

out_dir = "scratchpad/maps/bay_8d_2000"
os.makedirs(out_dir, exist_ok=True)

# Generate objectives
# 0: distance, 1: time, 2-7: random U[1, 100]
print("Writing subgraphs...")
for k in range(M_total):
    with open(f"{out_dir}/obj_{k}.gr", "w") as f:
        for u_old in visited:
            u_new = mapping[u_old]
            if u_old in edges:
                for v_old, costs in edges[u_old].items():
                    if v_old in visited_set:
                        v_new = mapping[v_old]
                        if k == 0: w = costs[0]
                        elif k == 1: w = costs[1]
                        else: w = random.randint(1, 100)
                        if w == -1: w = 1000 # fallback if missing in one file
                        f.write(f"a {u_new} {v_new} {w}\n")

with open(f"{out_dir}/info.txt", "w") as f:
    f.write(f"start={start_new}\ngoal={goal_new}\nsize={len(visited)}\n")

print("Done.")
