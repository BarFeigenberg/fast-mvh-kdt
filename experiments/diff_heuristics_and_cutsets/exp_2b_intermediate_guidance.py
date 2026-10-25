"""
Micro-Experiment 2B: Intermediate Compound Guidance Across Multi-Node Cut-Sets (|L| = k > 1)
Tests:
1. Inadmissibility of Single-Landmark Guidance:
   Show that blindly guiding intermediate nodes towards a single L_1 loses optimal paths
   that route through L_j != L_1.
2. Admissibility of the Compact Union Bound:
   H_guide(u) = ParetoMin_{L_i in L} { h_id(u -> L_i) + h_id(L_i -> Goal) }
   Verify 100% bit-identical Pareto fronts for k in {1, 2, 3, 4}.
3. Efficiency:
   Verify that |H_guide(u)| <= k, bounding intermediate ChooseH calls to O(k) while
   providing strong search guidance and cutting expansions over blind search.
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

def generate_multi_cutset_graph(k=3, width=6, m=3, seed=42):
    """
    Generates a layered DAG with a cut-set containing k separator nodes.
    Paths through different L_i have distinct trade-offs to stress-test guidance.
    """
    np.random.seed(seed)
    curr_id = 0
    
    # Layer 0: start
    s_start = curr_id
    curr_id += 1
    
    # Layer 1: pre-cut width
    l1 = list(range(curr_id, curr_id + width))
    curr_id += width
    
    # Layer 2: Cut-set with k nodes
    cut_nodes = list(range(curr_id, curr_id + k))
    curr_id += k
    
    # Layer 3: post-cut width
    l3 = list(range(curr_id, curr_id + width))
    curr_id += width
    
    # Layer 4: goal
    s_goal = curr_id
    curr_id += 1
    
    V = curr_id
    adj = {i: [] for i in range(V)}
    
    # Connect start to l1
    for v in l1:
        c = np.random.randint(2, 15, size=m)
        adj[s_start].append((v, c))
        
    # Connect l1 to cut_nodes
    for u in l1:
        for idx, L in enumerate(cut_nodes):
            # Create specialized edge costs so different L offer different advantages
            c = np.random.randint(5, 20, size=m)
            c[idx % m] = np.random.randint(1, 4) # Bias toward objective idx % m
            adj[u].append((L, c))
            
    # Connect cut_nodes to l3
    for idx, L in enumerate(cut_nodes):
        for v in l3:
            c = np.random.randint(5, 20, size=m)
            c[(idx + 1) % m] = np.random.randint(1, 4)
            adj[L].append((v, c))
            
    # Connect l3 to goal
    for u in l3:
        c = np.random.randint(2, 15, size=m)
        adj[u].append((s_goal, c))
        
    return adj, V, s_start, s_goal, cut_nodes

def compute_ideal_points(adj, V, m):
    """Computes ideal single-objective shortest paths between all pairs using Dijkstra."""
    dist = {}
    for obj in range(m):
        for s in range(V):
            d = {v: float('inf') for v in range(V)}
            d[s] = 0
            pq = [(0, s)]
            while pq:
                cd, u = heapq.heappop(pq)
                if cd > d[u]: continue
                for v, edge_c in adj[u]:
                    w = edge_c[obj]
                    if d[u] + w < d[v]:
                        d[v] = d[u] + w
                        heapq.heappush(pq, (d[v], v))
            for v in range(V):
                if (s, v) not in dist:
                    dist[(s, v)] = np.zeros(m)
                dist[(s, v)][obj] = d[v]
    return dist

def run_search_with_guidance(adj, V, s_start, s_goal, cut_nodes, dist, m, mode='union'):
    """
    NAMOA* search with guidance:
    mode == 'none': blind search (h = 0)
    mode == 'single': inadmissibly guides only toward cut_nodes[0]
    mode == 'union': compact admissible union bound H_guide(u)
    """
    fronts = {v: [] for v in range(V)}
    sols = []
    expansions = 0
    choose_h_calls = 0
    
    # Priority queue: (f1, u, cost_tup, h_idx)
    pq = []
    
    def get_H_guide(u):
        nonlocal choose_h_calls
        choose_h_calls += 1
        if u == s_goal:
            return [np.zeros(m)]
        if mode == 'none':
            return [np.zeros(m)]
        elif mode == 'single':
            # Single-landmark heuristic towards cut_nodes[0]
            L0 = cut_nodes[0]
            h = dist[(u, L0)] + dist[(L0, s_goal)]
            return [h]
        elif mode == 'union':
            # Compact admissible union bound
            candidates = []
            for L in cut_nodes:
                cand = dist[(u, L)] + dist[(L, s_goal)]
                candidates.append(cand)
            # Pareto-filter to keep non-dominated lower bounds (at most k)
            # For lower bounds, we want bounds not dominated by others.
            # In standard Pareto minimization, a lower bound h1 dominates h2 if h1 >= h2.
            # So minimal elements of -c are maximal lower bounds.
            pts = np.unique(np.array(candidates), axis=0)
            is_eff = np.ones(len(pts), dtype=bool)
            for i, p in enumerate(pts):
                if is_eff[i]:
                    dom = np.all(p >= pts, axis=1) & np.any(p > pts, axis=1)
                    is_eff[dom] = False
            return pts[is_eff]

    # Initial start node
    H_start = get_H_guide(s_start)
    h0 = H_start[0]
    heapq.heappush(pq, (h0[0], s_start, tuple([0]*m), 0))
    
    while pq:
        f1, u, cost_tup, h_idx = heapq.heappop(pq)
        g = np.array(cost_tup)
        
        # Check against known goal solutions
        if any(weakly_dominates(s, g) for s in sols):
            continue
        if any(weakly_dominates(p, g) for p in fronts[u]):
            continue
            
        fronts[u] = [p for p in fronts[u] if not dominates(g, p)]
        fronts[u].append(g)
        
        if u == s_goal:
            sols.append(g)
            sols = list(pareto_filter(sols))
            continue
            
        expansions += 1
        for v, edge_c in adj[u]:
            new_g = g + edge_c
            # Check local dominance
            if any(weakly_dominates(p, new_g) for p in fronts[v]):
                continue
            H_v = get_H_guide(v)
            for i, h in enumerate(H_v):
                f = new_g + h
                if any(weakly_dominates(s, f) for s in sols):
                    continue
                heapq.heappush(pq, (f[0], v, tuple(new_g), i))
                break # Bind first unrefuted heuristic
                
    res_front = np.array(sorted(sols, key=lambda x: tuple(x)))
    return res_front, expansions, choose_h_calls

def run_experiment_2b():
    print("="*60)
    print("MICRO-EXPERIMENT 2B: INTERMEDIATE COMPOUND GUIDANCE")
    print("="*60)
    
    m = 3
    for k in [1, 2, 3, 4]:
        print(f"\n=================== CUT-SET SIZE k = {k} ===================")
        for seed in [42, 101, 777]:
            adj, V, s_start, s_goal, cut_nodes = generate_multi_cutset_graph(k=k, width=6, m=m, seed=seed)
            dist = compute_ideal_points(adj, V, m)
            
            # 1. Oracle (Blind search - 100% admissible ground truth)
            front_oracle, exp_oracle, calls_oracle = run_search_with_guidance(adj, V, s_start, s_goal, cut_nodes, dist, m, mode='none')
            
            # 2. Inadmissible Single-Landmark Guidance (biased toward L_0)
            front_single, exp_single, calls_single = run_search_with_guidance(adj, V, s_start, s_goal, cut_nodes, dist, m, mode='single')
            single_match = (len(front_single) == len(front_oracle)) and np.all(front_single == front_oracle)
            lost_in_single = len(front_oracle) - len(front_single)
            
            # 3. Compact Admissible Union Bound H_guide(u)
            front_union, exp_union, calls_union = run_search_with_guidance(adj, V, s_start, s_goal, cut_nodes, dist, m, mode='union')
            union_match = (len(front_union) == len(front_oracle)) and np.all(front_union == front_oracle)
            lost_in_union = len(front_oracle) - len(front_union)
            
            print(f"Seed {seed} (Oracle Solutions: {len(front_oracle)}):")
            print(f"  Blind (No H):     Sols={len(front_oracle)} | Expansions={exp_oracle} | ChooseH={calls_oracle}")
            print(f"  Single-Landmark:  Sols={len(front_single)} | Sound={single_match} | LOST={lost_in_single} | Expansions={exp_single}")
            print(f"  Union Bound H:    Sols={len(front_union)} | Sound={union_match} | LOST={lost_in_union} | Expansions={exp_union} | ChooseH={calls_union}")
            
            assert union_match, "Compact Union Bound lost solutions!"

if __name__ == '__main__':
    run_experiment_2b()
