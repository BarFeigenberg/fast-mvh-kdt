import numpy as np

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

def create_bottleneck_dag(layers_before, layers_after, width, bottleneck_width):
    G = {}
    node_id = 0
    
    G[node_id] = []
    start = node_id
    node_id += 1
    
    # Before bottleneck
    prev_layer = [start]
    for _ in range(layers_before):
        curr_layer = []
        for _ in range(width):
            G[node_id] = []
            curr_layer.append(node_id)
            node_id += 1
        for u in prev_layer:
            for v in curr_layer:
                G[u].append((v, np.random.randint(1, 20, size=2)))
        prev_layer = curr_layer
        
    # Bottleneck
    bottleneck_layer = []
    for _ in range(bottleneck_width):
        G[node_id] = []
        bottleneck_layer.append(node_id)
        node_id += 1
    for u in prev_layer:
        for v in bottleneck_layer:
            G[u].append((v, np.random.randint(1, 20, size=2)))
    prev_layer = bottleneck_layer
    
    # After bottleneck
    for _ in range(layers_after):
        curr_layer = []
        for _ in range(width):
            G[node_id] = []
            curr_layer.append(node_id)
            node_id += 1
        for u in prev_layer:
            for v in curr_layer:
                G[u].append((v, np.random.randint(1, 20, size=2)))
        prev_layer = curr_layer
        
    # End node
    G[node_id] = []
    end = node_id
    for u in prev_layer:
        G[u].append((end, np.random.randint(1, 20, size=2)))
        
    return G, start, end, bottleneck_layer

def search(G, start, end, heuristic=None, target_frontier=None):
    # DFS or A* like. Since it's a DAG, let's just do a topological DP but with pruning!
    # If we prune, we skip updating successors.
    dp = {node: np.empty((0, 2)) for node in G}
    dp[start] = np.array([[0, 0]])
    
    expansions = 0
    checks = 0
    
    for u in sorted(G.keys()):
        if len(dp[u]) == 0: continue
        
        # Pruning step
        valid_paths = []
        for g in dp[u]:
            expansions += 1
            # If we have a heuristic for this node, check if it can be pruned by target_frontier
            pruned = False
            if heuristic is not None and u in heuristic and target_frontier is not None:
                # Check if g + h is dominated by target_frontier for all h in heuristic[u]
                # To be NOT pruned, there must be AT LEAST ONE h in heuristic[u] such that g + h is NOT dominated by target_frontier
                # Equivalently, if for ALL h, g+h is dominated by target_frontier, we prune.
                can_survive = False
                for h in heuristic[u]:
                    checks += 1
                    f = g + h
                    # is f dominated by target_frontier?
                    # dominated if there exists tf in target_frontier such that tf <= f
                    is_dominated = np.any(np.all(target_frontier <= f, axis=1) & np.any(target_frontier < f, axis=1))
                    if not is_dominated:
                        can_survive = True
                        break
                pruned = not can_survive
                
            if not pruned:
                valid_paths.append(g)
                
        dp[u] = np.array(valid_paths) if valid_paths else np.empty((0, 2))
        
        if len(dp[u]) == 0: continue
        
        for v, cost in G[u]:
            new_paths = dp[u] + cost
            if v not in dp: dp[v] = np.empty((0, 2))
            dp[v] = min_pareto(dp[v], new_paths)
            
    return dp[end], expansions, checks

if __name__ == "__main__":
    np.random.seed(42)
    G, start, end, b_layer = create_bottleneck_dag(3, 3, 5, 2)
    
    # First, get the true target frontier (assume we found it via some heuristic)
    # We will use it to bound.
    dp_true = {node: np.empty((0, 2)) for node in G}
    dp_true[start] = np.array([[0, 0]])
    for u in sorted(G.keys()):
        if len(dp_true[u]) == 0: continue
        for v, cost in G[u]:
            dp_true[v] = min_pareto(dp_true.get(v, np.empty((0, 2))), dp_true[u] + cost)
            
    target_frontier = dp_true[end]
    print(f"Target Frontier Size: {len(target_frontier)}")
    
    # 1. Pure Forward Search (No pruning)
    f_mono, exp_mono, chk_mono = search(G, start, end)
    print(f"Monolithic: {exp_mono} expansions, {chk_mono} checks")
    
    # 2. Selective Bidirectional
    # Precompute backward DP ONLY from bottleneck
    # Wait, backward DP from end to bottleneck computes EXACT h for bottleneck nodes.
    # Let's compute it.
    G_rev = {node: [] for node in G}
    for u in G:
        for v, cost in G[u]:
            G_rev[v].append((u, cost))
            
    dp_back = {node: np.empty((0, 2)) for node in G}
    dp_back[end] = np.array([[0, 0]])
    for u in sorted(G_rev.keys(), reverse=True):
        if len(dp_back[u]) == 0: continue
        for v, cost in G_rev[u]:
            dp_back[v] = min_pareto(dp_back.get(v, np.empty((0, 2))), dp_back[u] + cost)
            
    heuristic = {node: dp_back[node] for node in b_layer}
    
    f_sel, exp_sel, chk_sel = search(G, start, end, heuristic, target_frontier)
    print(f"Selective Bidirectional (at bottleneck only): {exp_sel} expansions, {chk_sel} checks")
    
    # 3. Full Bidirectional
    # Heuristic at ALL nodes
    full_heuristic = {node: dp_back[node] for node in G}
    f_full, exp_full, chk_full = search(G, start, end, full_heuristic, target_frontier)
    print(f"Full Bidirectional: {exp_full} expansions, {chk_full} checks")
