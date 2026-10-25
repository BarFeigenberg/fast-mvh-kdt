"""
Micro-Experiment 2A: Cut-Set Decomposition Architectures
Tests:
1. Ground Truth Oracle: Monolithic exact search from Start to Goal.
2. Flawed Pillar 3 Eager Injection: Eagerly injects g(Start->L) + H*(L) into T
   assuming p_1 <= f_1. Demonstrates the exact soundness breakdown (lost solutions).
3. Architecture 1 (Lazy Terminal Sinks):
   Treats L as an absorbing sink in forward search. Binds lazily via ChooseH into OPEN.
   Expands in strict f_1 order.
4. Architecture 2 (Eager Stitching with Witness-Checked T + Residue R(goal)):
   Eagerly injects stitched paths into T, but uses Algorithm 1's LocalDom + Update
   with Full Witness check (p_1 <= f_1) and residue fallback R(goal).
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

def generate_bottleneck_graph(n_stages=4, width=5, m=3, seed=42):
    """
    Generates a layered DAG with a narrow cut-set bottleneck.
    Stage 0: Start node (id 0)
    Stage 1: Layer of width nodes
    Stage 2: Cut-set bottleneck (1 or 2 nodes)
    Stage 3: Layer of width nodes
    Stage 4: Goal node
    """
    np.random.seed(seed)
    layers = []
    curr_id = 0
    
    # Layer 0: start
    layers.append([curr_id])
    curr_id += 1
    
    # Layer 1: width nodes
    l1 = list(range(curr_id, curr_id + width))
    layers.append(l1)
    curr_id += width
    
    # Layer 2: Bottleneck cut-set (2 nodes)
    cut_nodes = list(range(curr_id, curr_id + 2))
    layers.append(cut_nodes)
    curr_id += 2
    
    # Layer 3: width nodes
    l3 = list(range(curr_id, curr_id + width))
    layers.append(l3)
    curr_id += width
    
    # Layer 4: goal
    goal_id = curr_id
    layers.append([goal_id])
    curr_id += 1
    
    V = curr_id
    adj = {i: [] for i in range(V)}
    
    # Add dense trade-off edges between adjacent layers
    for s in range(len(layers) - 1):
        for u in layers[s]:
            for v in layers[s+1]:
                # Negative correlation / trade-off cost
                c = np.random.randint(2, 20, size=m)
                # Introduce strong negative correlation
                c[0] = np.random.randint(1, 30)
                c[1] = 31 - c[0] + np.random.randint(-3, 3)
                if m >= 3:
                    c[2] = np.random.randint(1, 25)
                if m >= 4:
                    c[3] = np.random.randint(1, 25)
                c = np.maximum(1, c)
                adj[u].append((v, c))
                
    return adj, V, 0, goal_id, cut_nodes

def compute_backward_pareto(adj, V, targets, goal, m):
    """Computes exact Pareto front from each cut node to goal."""
    # Since it's a DAG, simple backward search from goal
    H_star = {t: [] for t in targets}
    # Backward edges
    rev_adj = {i: [] for i in range(V)}
    for u in adj:
        for v, c in adj[u]:
            rev_adj[v].append((u, c))
            
    fronts = {v: [] for v in range(V)}
    pq = [(0, goal, tuple([0]*m))]
    
    while pq:
        f1, u, cost_tup = heapq.heappop(pq)
        cost = np.array(cost_tup)
        if any(weakly_dominates(p, cost) for p in fronts[u]):
            continue
        fronts[u] = [p for p in fronts[u] if not dominates(cost, p)]
        fronts[u].append(cost)
        
        for v, edge_c in rev_adj[u]:
            new_c = cost + edge_c
            heapq.heappush(pq, (new_c[0], v, tuple(new_c)))
            
    for t in targets:
        # Sort H*(t) lexicographically by h1
        pts = pareto_filter(fronts[t])
        H_star[t] = sorted(pts, key=lambda x: tuple(x))
    return H_star

# 1. Monolithic Oracle
def run_monolithic_oracle(adj, V, s_start, s_goal, m):
    fronts = {v: [] for v in range(V)}
    pq = [(0, s_start, tuple([0]*m))]
    sols = []
    expansions = 0
    
    while pq:
        f1, u, cost_tup = heapq.heappop(pq)
        cost = np.array(cost_tup)
        
        # Check against goal sols
        if any(weakly_dominates(s, cost) for s in sols):
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
            
    return np.array(sorted(sols, key=lambda x: tuple(x))), expansions

# 2. Flawed Pillar 3 Eager Injection
def run_flawed_pillar3(adj, V, s_start, s_goal, cut_nodes, H_star, m):
    """
    Simulates Pillar 3 eager injection into T with Tr(f) dominance.
    Demonstrates why paths are lost when p1 <= f1 invariant is violated!
    """
    fronts = {v: [] for v in range(V)}
    T = [] # Stores full vectors, but GoalDom only checks Tr!
    pq = [(0, s_start, tuple([0]*m))]
    sols = []
    lost_paths_pruned = 0
    expansions = 0
    
    while pq:
        f1, u, cost_tup = heapq.heappop(pq)
        cost = np.array(cost_tup)
        
        # Flawed DR GoalDom: checks Tr(sol) <= Tr(cost), assuming sol_1 <= cost_1!
        is_pruned = False
        for sol in T:
            # Check Tr only!
            if np.all(sol[1:] <= cost[1:]):
                # Dominated according to flawed Pillar 3!
                is_pruned = True
                break
        if is_pruned:
            lost_paths_pruned += 1
            continue
            
        if any(weakly_dominates(p, cost) for p in fronts[u]):
            continue
        fronts[u] = [p for p in fronts[u] if not dominates(cost, p)]
        fronts[u].append(cost)
        
        if u == s_goal:
            sols.append(cost)
            sols = list(pareto_filter(sols))
            continue
            
        if u in cut_nodes:
            # Eagerly stitch with H*(u) and directly inject into T!
            for h in H_star[u]:
                stitched_sol = cost + h
                # Eagerly add to T without waiting for OPEN extraction!
                T.append(stitched_sol)
            # Cut node is terminal for subproblem 1
            continue
            
        expansions += 1
        for v, edge_c in adj[u]:
            new_c = cost + edge_c
            heapq.heappush(pq, (new_c[0], v, tuple(new_c)))
            
    return np.array(sorted(sols, key=lambda x: tuple(x))), lost_paths_pruned, expansions

# 3. Architecture 1: Lazy Terminal Sinks
def run_architecture_1(adj, V, s_start, s_goal, cut_nodes, H_star, m):
    """
    Architecture 1: Treats cut_nodes as terminal sinks in forward search.
    When a path reaches L_i, binds lazily to h^1(L_i) and inserts (L_i, g, j) into OPEN.
    Extracts in strict f_1 order. Only when extracted from OPEN is f added to T!
    """
    fronts = {v: [] for v in range(V)}
    T = []
    # Priority queue stores: (f1, node_type, u, g_tup, h_idx)
    # node_type 0 = normal node, 1 = terminal sink node
    pq = []
    heapq.heappush(pq, (0, 0, s_start, tuple([0]*m), 0))
    sols = []
    expansions = 0
    max_open_size = 0
    
    while pq:
        max_open_size = max(max_open_size, len(pq))
        f1, ntype, u, g_tup, h_idx = heapq.heappop(pq)
        g = np.array(g_tup)
        
        if ntype == 1:
            # Terminal sink node!
            # Current evaluation f = g + H*(u)[h_idx]
            h = H_star[u][h_idx]
            f = g + h
            
            # Check if f is dominated by T
            if any(weakly_dominates(sol, f) for sol in sols):
                pass # Dominated
            else:
                sols.append(f)
                sols = list(pareto_filter(sols))
                T.append(f)
                
            # Lazy re-evaluation: push next heuristic h_{idx+1} if exists
            if h_idx + 1 < len(H_star[u]):
                next_h = H_star[u][h_idx + 1]
                next_f1 = g[0] + next_h[0]
                heapq.heappush(pq, (next_f1, 1, u, g_tup, h_idx + 1))
            continue
            
        # Normal node
        if any(weakly_dominates(s, g) for s in sols):
            continue
        if any(weakly_dominates(p, g) for p in fronts[u]):
            continue
        fronts[u] = [p for p in fronts[u] if not dominates(g, p)]
        fronts[u].append(g)
        
        if u in cut_nodes:
            # Arrived at cut node! Bind lazily to h_idx = 0 and push into OPEN
            if len(H_star[u]) > 0:
                h0 = H_star[u][0]
                init_f1 = g[0] + h0[0]
                heapq.heappush(pq, (init_f1, 1, u, g_tup, 0))
            continue
            
        expansions += 1
        for v, edge_c in adj[u]:
            new_g = g + edge_c
            heapq.heappush(pq, (new_g[0], 0, v, tuple(new_g), 0))
            
    return np.array(sorted(sols, key=lambda x: tuple(x))), expansions, max_open_size

# 4. Architecture 2: Eager Stitching with Witness-Checked T + Residue R(goal)
def run_architecture_2(adj, V, s_start, s_goal, cut_nodes, H_star, m):
    """
    Architecture 2: Eagerly injects stitched paths into T, but uses Algorithm 1's
    LocalDom + Update with Full Witness check (p_1 <= f_1) and residue fallback R(goal).
    """
    fronts = {v: [] for v in range(V)}
    F_goal = [] # Truncated frontier of T
    R_goal = [] # Residue set of T (full vectors)
    pq = [(0, s_start, tuple([0]*m))]
    sols = []
    expansions = 0
    max_open_size = 0
    
    def goal_dom_with_residue(q):
        # Truncated query
        has_tr_dominator = False
        for p in F_goal:
            if np.all(p[1:] <= q[1:]):
                # Truncated dominator found! Check full witness:
                if p[0] <= q[0]:
                    return True # FULL WITNESS!
                has_tr_dominator = True
                
        if not has_tr_dominator:
            return False # No truncated dominator
            
        # Exact residue fallback
        for r in R_goal:
            if weakly_dominates(r, q):
                return True
        return False

    def update_goal(g_vec):
        nonlocal F_goal, R_goal
        new_F = []
        for p in F_goal:
            if np.all(g_vec[1:] <= p[1:]):
                # p is evicted from F_goal
                if p[0] < g_vec[0]:
                    # Retain in residue R_goal
                    R_goal.append(p)
            else:
                new_F.append(p)
        new_F.append(g_vec)
        F_goal = new_F

    while pq:
        max_open_size = max(max_open_size, len(pq))
        f1, u, cost_tup = heapq.heappop(pq)
        cost = np.array(cost_tup)
        
        # Witness-checked GoalDom with residue!
        if goal_dom_with_residue(cost):
            continue
            
        if any(weakly_dominates(p, cost) for p in fronts[u]):
            continue
        fronts[u] = [p for p in fronts[u] if not dominates(cost, p)]
        fronts[u].append(cost)
        
        if u in cut_nodes:
            # Eagerly stitch and insert into T using Algorithm 1's Update!
            for h in H_star[u]:
                stitched = cost + h
                sols.append(stitched)
                update_goal(stitched)
            sols = list(pareto_filter(sols))
            continue
            
        expansions += 1
        for v, edge_c in adj[u]:
            new_c = cost + edge_c
            heapq.heappush(pq, (new_c[0], v, tuple(new_c)))
            
    final_sols = pareto_filter(sols)
    return np.array(sorted(final_sols, key=lambda x: tuple(x))), expansions, max_open_size, len(R_goal)

def run_experiment_2a():
    print("="*60)
    print("MICRO-EXPERIMENT 2A: CUT-SET DECOMPOSITION ARCHITECTURES")
    print("="*60)
    
    seeds = [42, 101, 777, 999]
    dims = [3, 4]
    
    for m in dims:
        for seed in seeds:
            adj, V, s_start, s_goal, cut_nodes = generate_bottleneck_graph(n_stages=4, width=6, m=m, seed=seed)
            H_star = compute_backward_pareto(adj, V, cut_nodes, s_goal, m)
            
            # 1. Oracle
            t0 = time.perf_counter()
            oracle_sols, oracle_exp = run_monolithic_oracle(adj, V, s_start, s_goal, m)
            t_oracle = (time.perf_counter() - t0) * 1000
            
            # 2. Flawed Pillar 3
            flawed_sols, lost_prunes, flawed_exp = run_flawed_pillar3(adj, V, s_start, s_goal, cut_nodes, H_star, m)
            flawed_match = (len(flawed_sols) == len(oracle_sols)) and np.all(flawed_sols == oracle_sols)
            lost_count = len(oracle_sols) - len(flawed_sols)
            
            # 3. Architecture 1 (Lazy Terminal Sinks)
            t0 = time.perf_counter()
            arch1_sols, arch1_exp, arch1_open = run_architecture_1(adj, V, s_start, s_goal, cut_nodes, H_star, m)
            t_arch1 = (time.perf_counter() - t0) * 1000
            arch1_match = (len(arch1_sols) == len(oracle_sols)) and np.all(arch1_sols == oracle_sols)
            
            # 4. Architecture 2 (Witness-Checked T + Residue R(goal))
            t0 = time.perf_counter()
            arch2_sols, arch2_exp, arch2_open, r_size = run_architecture_2(adj, V, s_start, s_goal, cut_nodes, H_star, m)
            t_arch2 = (time.perf_counter() - t0) * 1000
            arch2_match = (len(arch2_sols) == len(oracle_sols)) and np.all(arch2_sols == oracle_sols)
            
            print(f"\n--- M={m}, Seed={seed} (Oracle Solutions: {len(oracle_sols)}) ---")
            print(f"  Flawed Pillar 3:  Sols={len(flawed_sols)} | Sound={flawed_match} | LOST SOLUTIONS={lost_count} | FalselyPruned={lost_prunes}")
            print(f"  Architecture 1:   Sols={len(arch1_sols)} | Sound={arch1_match} | Time={t_arch1:.2f}ms | Exp={arch1_exp} | MaxOPEN={arch1_open}")
            print(f"  Architecture 2:   Sols={len(arch2_sols)} | Sound={arch2_match} | Time={t_arch2:.2f}ms | Exp={arch2_exp} | MaxOPEN={arch2_open} | R(goal)={r_size}")
            
            assert arch1_match, "Architecture 1 lost solutions!"
            assert arch2_match, "Architecture 2 lost solutions!"

if __name__ == '__main__':
    run_experiment_2a()
