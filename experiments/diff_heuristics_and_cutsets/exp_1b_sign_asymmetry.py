"""
Micro-Experiment 1B: Sign Asymmetry in max(0, g_uL - g_tL)
Tests:
1. Inadmissibility of lower-bounding g_tL:
   Show that replacing g_tL with a component-wise lower bound (or ideal point of G_{t->L})
   violates admissibility and causes search to lose optimal Pareto solutions.
2. Admissibility of Realizable Paths:
   Show that taking K scalarized Dijkstra paths (upper bounds / realizable paths)
   for g_tL strictly preserves 100% bit-identical Pareto fronts (0 lost solutions).
3. Pruning Power Comparison:
   Compare pruning power (nodes pruned & search expansions) between:
   - Full exact Pareto front G_{t->L}
   - K = M single-objective Dijkstra paths
   - K = 2M extreme + diagonal scalarized Dijkstra paths
   - K = 16 random convex combination Dijkstra paths
"""

import numpy as np
import time
import heapq

def dominates(a, b):
    return np.all(a <= b) and np.any(a < b)

def weakly_dominates(a, b):
    return np.all(a <= b)

def pareto_filter(costs):
    if len(costs) == 0:
        return np.empty((0, costs.shape[1] if hasattr(costs, 'shape') else 0))
    costs = np.unique(np.array(costs), axis=0)
    is_efficient = np.ones(costs.shape[0], dtype=bool)
    for i, c in enumerate(costs):
        if is_efficient[i]:
            dom = np.all(c <= costs, axis=1) & np.any(c < costs, axis=1)
            is_efficient[dom] = False
    return costs[is_efficient]

def generate_grid_graph(n=6, m=3, seed=42):
    np.random.seed(seed)
    V = n * n
    adj = {i: [] for i in range(V)}
    for r in range(n):
        for c in range(n):
            u = r * n + c
            if c + 1 < n:
                v = r * n + (c + 1)
                cost = np.random.randint(1, 15, size=m)
                adj[u].append((v, cost))
                adj[v].append((u, cost))
            if r + 1 < n:
                v = (r + 1) * n + c
                cost = np.random.randint(1, 15, size=m)
                adj[u].append((v, cost))
                adj[v].append((u, cost))
    return adj, V

def compute_all_pareto_to_target(adj, V, target, m):
    fronts = {v: [] for v in range(V)}
    pq = []
    heapq.heappush(pq, (0, target, tuple([0]*m)))
    while pq:
        f1, u, cost_tup = heapq.heappop(pq)
        cost = np.array(cost_tup)
        if any(weakly_dominates(p, cost) for p in fronts[u]):
            continue
        fronts[u] = [p for p in fronts[u] if not dominates(cost, p)]
        fronts[u].append(cost)
        for v, edge_c in adj[u]:
            new_c = cost + edge_c
            heapq.heappush(pq, (new_c[0], v, tuple(new_c)))
    return {v: np.array(fronts[v]) for v in range(V)}

def run_scalarized_dijkstra(adj, V, s, target, weights, m):
    """Computes a single shortest path from s to target under scalar weights w^T c."""
    dist = {v: float('inf') for v in range(V)}
    dist[s] = 0.0
    actual_cost = {v: np.zeros(m) for v in range(V)}
    pq = [(0.0, s)]
    
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist[u]:
            continue
        if u == target:
            return actual_cost[target]
        for v, edge_c in adj[u]:
            w_cost = float(np.dot(weights, edge_c))
            if dist[u] + w_cost < dist[v]:
                dist[v] = dist[u] + w_cost
                actual_cost[v] = actual_cost[u] + edge_c
                heapq.heappush(pq, (dist[v], v))
    return actual_cost[target]

def get_scalarized_paths(adj, V, t, landmark, m, K_mode):
    """Returns K realizable path cost vectors from t to landmark."""
    paths = []
    if K_mode == 'M':
        # Single-objective Dijkstra for each objective
        for i in range(m):
            w = np.zeros(m)
            w[i] = 1.0
            p = run_scalarized_dijkstra(adj, V, t, landmark, w, m)
            paths.append(p)
    elif K_mode == '2M':
        for i in range(m):
            w = np.zeros(m)
            w[i] = 1.0
            paths.append(run_scalarized_dijkstra(adj, V, t, landmark, w, m))
        # Add diagonal / pairwise combinations
        w_diag = np.ones(m) / m
        paths.append(run_scalarized_dijkstra(adj, V, t, landmark, w_diag, m))
        for i in range(m):
            w = np.ones(m)
            w[i] = 3.0
            w = w / np.sum(w)
            paths.append(run_scalarized_dijkstra(adj, V, t, landmark, w, m))
    elif K_mode == '16':
        np.random.seed(123)
        for i in range(m):
            w = np.zeros(m)
            w[i] = 1.0
            paths.append(run_scalarized_dijkstra(adj, V, t, landmark, w, m))
        while len(paths) < 16:
            w = np.random.uniform(0.1, 1.0, size=m)
            w = w / np.sum(w)
            paths.append(run_scalarized_dijkstra(adj, V, t, landmark, w, m))
    return np.unique(np.array(paths), axis=0)

def search_with_diff_h(adj, V, s_start, s_goal, G_to_L, g_tL_candidates, m):
    """NAMOA* search using differential heuristics derived from g_tL_candidates."""
    fronts = {v: [] for v in range(V)}
    pq = []
    heapq.heappush(pq, (0, s_start, tuple([0]*m)))
    sols = []
    expansions = 0
    prunes = 0
    
    while pq:
        f1, u, cost_tup = heapq.heappop(pq)
        cost = np.array(cost_tup)
        
        # Differential pruning against known solutions
        if len(sols) > 0 and len(g_tL_candidates) > 0 and len(G_to_L[u]) > 0:
            pruned = False
            for g_tL_vec in g_tL_candidates:
                # Check if this g_tL refutes all g_uL in G_to_L[u]
                all_dom = True
                for g_uL_vec in G_to_L[u]:
                    h = np.maximum(0, g_uL_vec - g_tL_vec)
                    f = cost + h
                    if not any(weakly_dominates(sol, f) for sol in sols):
                        all_dom = False
                        break
                if all_dom:
                    pruned = True
                    break
            if pruned:
                prunes += 1
                continue
                
        if any(weakly_dominates(p, cost) for p in fronts[u]):
            continue
        fronts[u] = [p for p in fronts[u] if not dominates(cost, p)]
        fronts[u].append(cost)
        
        if u == s_goal:
            sols.append(cost)
            sols = list(pareto_filter(sols))
            continue
            
        expansions += 1
        for v, edge_c in adj[u]:
            new_c = cost + edge_c
            heapq.heappush(pq, (new_c[0], v, tuple(new_c)))
            
    res_front = np.array(sorted(sols, key=lambda x: tuple(x)))
    return res_front, expansions, prunes

def run_experiment_1b():
    print("="*60)
    print("MICRO-EXPERIMENT 1B: SIGN ASYMMETRY & SCALARIZED DIJKSTRA")
    print("="*60)
    
    configs = [
        (5, 3, 42),
        (5, 3, 101),
        (5, 4, 42),
        (6, 3, 77)
    ]
    
    for n, m, seed in configs:
        adj, V = generate_grid_graph(n=n, m=m, seed=seed)
        s_start = 0
        s_goal = V - 1
        landmark = n - 1
        
        # Exact G_to_L for all states
        G_to_L = compute_all_pareto_to_target(adj, V, landmark, m)
        G_tL_exact = G_to_L[s_goal]
        
        # 1. Oracle ground truth (no heuristic pruning)
        oracle_front, oracle_exp, _ = search_with_diff_h(adj, V, s_start, s_goal, G_to_L, [], m)
        print(f"\n--- Grid {n}x{n}, M={m}, Seed={seed} (Oracle Solutions: {len(oracle_front)}) ---")
        
        # 2. Test Flawed Lower-Bound Approximation for g_tL (Ideal point lower bound)
        # Replacing G_tL with its ideal point (component-wise minimum)
        g_tL_lower_bound = np.min(G_tL_exact, axis=0, keepdims=True)
        flawed_front, flawed_exp, flawed_prunes = search_with_diff_h(adj, V, s_start, s_goal, G_to_L, g_tL_lower_bound, m)
        lost_sols = len(oracle_front) - len(flawed_front)
        is_flawed_sound = (len(flawed_front) == len(oracle_front)) and np.all(oracle_front == flawed_front)
        print(f"Lower-Bound Approx (Ideal Point g_tL): Sols={len(flawed_front)}/{len(oracle_front)}, Sound={is_flawed_sound}, LostSols={lost_sols}, Expansions={flawed_exp}")
        
        # 3. Test Full Exact G_tL
        exact_front, exact_exp, exact_prunes = search_with_diff_h(adj, V, s_start, s_goal, G_to_L, G_tL_exact, m)
        exact_match = (len(exact_front) == len(oracle_front)) and np.all(oracle_front == exact_front)
        print(f"Full Exact G_tL ({len(G_tL_exact)} vecs): Sols={len(exact_front)}, Sound={exact_match}, Prunes={exact_prunes}, Expansions={exact_exp}")
        
        # 4. Test K = M scalarized Dijkstra paths
        g_tL_M = get_scalarized_paths(adj, V, s_goal, landmark, m, 'M')
        front_M, exp_M, prunes_M = search_with_diff_h(adj, V, s_start, s_goal, G_to_L, g_tL_M, m)
        match_M = (len(front_M) == len(oracle_front)) and np.all(oracle_front == front_M)
        print(f"Scalarized Dijkstra K=M ({len(g_tL_M)} paths): Sols={len(front_M)}, Sound={match_M}, Prunes={prunes_M}, Expansions={exp_M}")
        
        # 5. Test K = 2M scalarized Dijkstra paths
        g_tL_2M = get_scalarized_paths(adj, V, s_goal, landmark, m, '2M')
        front_2M, exp_2M, prunes_2M = search_with_diff_h(adj, V, s_start, s_goal, G_to_L, g_tL_2M, m)
        match_2M = (len(front_2M) == len(oracle_front)) and np.all(oracle_front == front_2M)
        print(f"Scalarized Dijkstra K=2M ({len(g_tL_2M)} paths): Sols={len(front_2M)}, Sound={match_2M}, Prunes={prunes_2M}, Expansions={exp_2M}")
        
        # 6. Test K = 16 scalarized Dijkstra paths
        g_tL_16 = get_scalarized_paths(adj, V, s_goal, landmark, m, '16')
        front_16, exp_16, prunes_16 = search_with_diff_h(adj, V, s_start, s_goal, G_to_L, g_tL_16, m)
        match_16 = (len(front_16) == len(oracle_front)) and np.all(oracle_front == front_16)
        print(f"Scalarized Dijkstra K=16 ({len(g_tL_16)} paths): Sols={len(front_16)}, Sound={match_16}, Prunes={prunes_16}, Expansions={exp_16}")

if __name__ == '__main__':
    run_experiment_1b()
