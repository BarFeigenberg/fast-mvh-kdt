import numpy as np
import time

def pareto_frontier(costs):
    if len(costs) == 0: return costs
    costs = np.unique(costs, axis=0)
    is_efficient = np.ones(costs.shape[0], dtype=bool)
    for i, c in enumerate(costs):
        if is_efficient[i]:
            dominate = np.all(c <= costs, axis=1) & np.any(c < costs, axis=1)
            is_efficient[dominate] = False
    return costs[is_efficient]

def min_pareto(front1, front2):
    if len(front1) == 0: return front2
    if len(front2) == 0: return front1
    combined = np.vstack([front1, front2])
    return pareto_frontier(combined)

def cross_sum(front1, front2):
    if len(front1) == 0 or len(front2) == 0:
        return np.array([])
    res = front1[:, None, :] + front2[None, :, :]
    res = res.reshape(-1, front1.shape[1])
    return pareto_frontier(res)

def mosp_dp(graph, start, end):
    dp = {node: np.empty((0, 2)) for node in graph}
    if start not in dp:
        dp[start] = np.empty((0, 2))
    dp[start] = np.array([[0, 0]])
    
    # graph is a dict: node -> list of (neighbor, cost)
    # assume keys are topologically sorted
    for u in sorted(graph.keys()):
        if u not in dp or len(dp[u]) == 0: continue
        for v, cost in graph[u]:
            new_paths = dp[u] + cost
            if v not in dp: dp[v] = np.empty((0, 2))
            dp[v] = min_pareto(dp[v], new_paths)
            
    return dp.get(end, np.empty((0, 2))), dp

def create_layered_dag(layers, width):
    G = {}
    node_id = 0
    layer_nodes = []
    
    G[node_id] = []
    layer_nodes.append([node_id])
    node_id += 1
    
    for l in range(layers):
        current_layer = []
        for w in range(width):
            G[node_id] = []
            current_layer.append(node_id)
            node_id += 1
        layer_nodes.append(current_layer)
        
        prev_layer = layer_nodes[-2]
        for u in prev_layer:
            for v in current_layer:
                G[u].append((v, np.random.randint(1, 10, size=2)))
                
    G[node_id] = []
    for u in layer_nodes[-1]:
        G[u].append((node_id, np.random.randint(1, 10, size=2)))
    layer_nodes.append([node_id])
    
    return G, 0, node_id, layer_nodes

if __name__ == "__main__":
    np.random.seed(42)
    layers = 10
    width = 6
    G, start, end, layer_nodes = create_layered_dag(layers, width)
    
    # 1. Monolithic Search
    t0 = time.time()
    front_mono, dp_mono = mosp_dp(G, start, end)
    t1 = time.time()
    
    print(f"Monolithic Front Size: {len(front_mono)}")
    total_mono_fronts = sum(len(f) for f in dp_mono.values())
    print(f"Total intermediate paths stored: {total_mono_fronts}")
    
    # 2. Cut-set Decomposition
    # Let's pick the middle layer as the cut-set
    cut_layer = layer_nodes[layers // 2]
    print(f"\nDecomposing at layer {layers // 2} with {len(cut_layer)} nodes.")
    
    # 3. True Bidirectional Decomposition
    t4 = time.time()
    front_bidir = np.empty((0, 2))
    
    # We do a backward DP from G to all nodes
    # Reverse graph
    G_rev = {node: [] for node in G}
    for u in G:
        for v, cost in G[u]:
            G_rev[v].append((u, cost))
            
    # Run DP backward starting from end
    # Note: we need to sort keys in reverse topological order, which for our DAG is just reversed keys
    dp_back = {node: np.empty((0, 2)) for node in G}
    dp_back[end] = np.array([[0, 0]])
    
    for u in sorted(G_rev.keys(), reverse=True):
        if len(dp_back[u]) == 0: continue
        for v, cost in G_rev[u]:
            new_paths = dp_back[u] + cost
            dp_back[v] = min_pareto(dp_back[v], new_paths)
            
    total_back_fronts = sum(len(f) for f in dp_back.values())
    
    for L in cut_layer:
        combined = cross_sum(dp_mono[L], dp_back[L])
        front_bidir = min_pareto(front_bidir, combined)
        
    print(f"\nBidirectional Decomposition Front Size: {len(front_bidir)}")
    # Storage is just forward up to cut_layer + backward up to cut_layer
    forward_stored = sum(len(dp_mono[n]) for n in range(cut_layer[-1]+1))
    backward_stored = sum(len(dp_back[n]) for n in range(cut_layer[0], end+1))
    print(f"Total intermediate paths stored (Forward + Backward): {forward_stored + backward_stored}")
    mono_sorted = np.sort(front_mono, axis=0)
    print("Identical to mono?", np.array_equal(mono_sorted, np.sort(front_bidir, axis=0)))

