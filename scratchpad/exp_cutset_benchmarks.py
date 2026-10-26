import numpy as np
import time
import sys

# -------- Helpers --------
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
    return pareto_frontier(np.vstack([front1, front2]))

def cross_sum_naive(front1, front2):
    if len(front1) == 0 or len(front2) == 0:
        return np.empty((0, front1.shape[1] if len(front1)>0 else front2.shape[1]))
    res = front1[:, None, :] + front2[None, :, :]
    res = res.reshape(-1, front1.shape[1])
    return pareto_frontier(res)

# A simplified KDT branch and bound for Minkowski sum
def cross_sum_kdt_simulated(front1, front2, global_target):
    # We will simulate the KDT pruning by checking ideal bounds.
    # In reality, KDT does this hierarchically. Here we just measure 
    # how many pair combinations survive the ideal point check.
    if len(front1) == 0 or len(front2) == 0: return np.empty((0, front1.shape[1])), 0
    
    # We just compute the exact same as naive but count pruned branches.
    # We'll just return the exact answer, and simulate the "kdt_visits".
    # For a real implementation, we'd build the trees.
    exact = cross_sum_naive(front1, front2)
    kdt_visits = len(front1) + len(front2) # O(N + M) roughly if disjoint
    return exact, kdt_visits

# -------- Graph Generators --------
def generate_grid_dag(rows, cols, dim=3, rho=0.0):
    G = {}
    nodes = rows * cols
    for i in range(nodes): G[i] = []
    
    # Generate costs with correlation rho (approximated)
    # If rho < 0, anti-correlated. We can just use random for now, or 
    # generate correlated vectors.
    np.random.seed(42)
    def get_cost():
        c = np.random.randint(1, 20, size=dim)
        if rho < 0 and dim >= 2:
            # force anti-correlation between dim 0 and 1
            c[0] = np.random.randint(1, 10)
            c[1] = 20 - c[0] + np.random.randint(-2, 3)
            c[1] = max(1, c[1])
        return c

    for r in range(rows):
        for c in range(cols):
            u = r * cols + c
            if c + 1 < cols:
                v = r * cols + (c + 1)
                G[u].append((v, get_cost()))
            if r + 1 < rows:
                v = (r + 1) * cols + c
                G[u].append((v, get_cost()))
    return G, 0, nodes - 1

def generate_bottleneck_dag(dim=3):
    # Two dense 5x5 grids connected by a 2-node wide bridge.
    G = {}
    node_id = 0
    
    # Blob 1
    blob1_nodes = []
    for r in range(6):
        for c in range(6):
            G[node_id] = []
            blob1_nodes.append(node_id)
            node_id += 1
            
    for r in range(6):
        for c in range(6):
            u = r * 6 + c
            if c + 1 < 6:
                G[u].append((u + 1, np.random.randint(1, 10, size=dim)))
            if r + 1 < 6:
                G[u].append((u + 6, np.random.randint(1, 10, size=dim)))
                
    # Bridge
    bridge = [node_id, node_id+1]
    G[node_id] = []; G[node_id+1] = []
    node_id += 2
    
    # Connect end of Blob 1 to Bridge
    for u in [34, 35]: # bottom right of blob 1
        for b in bridge:
            G[u].append((b, np.random.randint(1, 10, size=dim)))
            
    # Blob 2
    blob2_nodes = []
    start_b2 = node_id
    for r in range(6):
        for c in range(6):
            G[node_id] = []
            blob2_nodes.append(node_id)
            node_id += 1
            
    for b in bridge:
        for v in [start_b2, start_b2+1]:
            G[b].append((v, np.random.randint(1, 10, size=dim)))
            
    for r in range(6):
        for c in range(6):
            u = start_b2 + r * 6 + c
            if c + 1 < 6:
                G[u].append((u + 1, np.random.randint(1, 10, size=dim)))
            if r + 1 < 6:
                G[u].append((u + 6, np.random.randint(1, 10, size=dim)))
                
    return G, 0, node_id - 1, bridge

# -------- Search Algorithms --------

def search_monolithic(G, start, end):
    t0 = time.time()
    dp = {u: np.empty((0, G[0][0][1].shape[0])) for u in G}
    dp[start] = np.array([np.zeros(G[0][0][1].shape[0])])
    
    expansions = 0
    max_memory = 0
    
    for u in sorted(G.keys()):
        f_u = dp[u]
        if len(f_u) == 0: continue
        expansions += len(f_u)
        max_memory = max(max_memory, len(f_u))
        
        for v, cost in G[u]:
            dp[v] = min_pareto(dp[v], f_u + cost)
            
    runtime = time.time() - t0
    return dp[end], runtime, expansions, max_memory

def search_cutset_model_A(G, start, end, cutset):
    # Narrow Cut-Set Decomposition (Reverse topological solve for L->G first)
    t0 = time.time()
    dim = G[0][0][1].shape[0]
    
    # 1. Reverse graph for L->G DP
    G_rev = {u: [] for u in G}
    for u in G:
        for v, cost in G[u]:
            G_rev[v].append((u, cost))
            
    dp_back = {u: np.empty((0, dim)) for u in G}
    dp_back[end] = np.array([np.zeros(dim)])
    
    back_expansions = 0
    max_memory = 0
    
    # Solve backward from end to all cutset nodes
    for u in sorted(G_rev.keys(), reverse=True):
        if len(dp_back[u]) == 0: continue
        back_expansions += len(dp_back[u])
        max_memory = max(max_memory, len(dp_back[u]))
        for v, cost in G_rev[u]:
            dp_back[v] = min_pareto(dp_back[v], dp_back[u] + cost)
            
    H_star = {L: dp_back[L] for L in cutset}
    
    # 2. Forward search S -> L with global target frontier cross-pruning
    dp_fwd = {u: np.empty((0, dim)) for u in G}
    dp_fwd[start] = np.array([np.zeros(dim)])
    
    fwd_expansions = 0
    F_target = np.empty((0, dim))
    
    for u in sorted(G.keys()):
        # Only process up to cutset
        if u > max(cutset): break
        
        f_u = dp_fwd[u]
        if len(f_u) == 0: continue
        
        # Cross-landmark Pruning: Check if ANY path can beat F_target.
        # This requires H_star, but we only have H_star for cutset.
        # For a full implementation, we'd need heuristic bounds for ALL nodes.
        # Here we just use the exact local frontier if it reaches a landmark.
        if u in cutset:
            combo = cross_sum_naive(f_u, H_star[u])
            F_target = min_pareto(F_target, combo)
            continue # stop at cutset
            
        fwd_expansions += len(f_u)
        max_memory = max(max_memory, len(f_u))
        
        for v, cost in G[u]:
            if v > max(cutset) and v not in cutset: continue
            dp_fwd[v] = min_pareto(dp_fwd[v], f_u + cost)
            
    runtime = time.time() - t0
    return F_target, runtime, fwd_expansions + back_expansions, max_memory

# -------- Benchmarking --------

def run_benchmarks():
    print("LANDMARK CUT-SET BENCHMARK SUITE")
    print("=================================")
    results = []
    
    # 1. Bottleneck Topologies
    G_bot, s_bot, e_bot, cut_bot = generate_bottleneck_dag(dim=3)
    f_mono, t_mono, exp_mono, mem_mono = search_monolithic(G_bot, s_bot, e_bot)
    f_cut, t_cut, exp_cut, mem_cut = search_cutset_model_A(G_bot, s_bot, e_bot, cut_bot)
    
    identical = np.array_equal(np.sort(f_mono, axis=0), np.sort(f_cut, axis=0))
    speedup = t_mono / t_cut if t_cut > 0 else float('inf')
    
    results.append({
        "Topology": "Bottleneck (2x 6x6, k=2)",
        "Dim": 3,
        "Identical": identical,
        "Mono Time": t_mono,
        "Cut Time": t_cut,
        "Speedup": speedup,
        "Mono Exp": exp_mono,
        "Cut Exp": exp_cut,
        "Mono Mem": mem_mono,
        "Cut Mem": mem_cut
    })
    
    # 2. Elongated Grid
    G_elong, s_elong, e_elong = generate_grid_dag(10, 30, dim=3, rho=-0.3)
    # Cutset at col 15
    cut_elong = [r * 30 + 15 for r in range(10)]
    f_mono, t_mono, exp_mono, mem_mono = search_monolithic(G_elong, s_elong, e_elong)
    f_cut, t_cut, exp_cut, mem_cut = search_cutset_model_A(G_elong, s_elong, e_elong, cut_elong)
    
    identical = np.array_equal(np.sort(f_mono, axis=0), np.sort(f_cut, axis=0))
    speedup = t_mono / t_cut if t_cut > 0 else float('inf')
    
    results.append({
        "Topology": "Elongated (10x30, k=10)",
        "Dim": 3,
        "Identical": identical,
        "Mono Time": t_mono,
        "Cut Time": t_cut,
        "Speedup": speedup,
        "Mono Exp": exp_mono,
        "Cut Exp": exp_cut,
        "Mono Mem": mem_mono,
        "Cut Mem": mem_cut
    })
    
    # 3. Isotropic Wide Grid (Negative Control)
    G_iso, s_iso, e_iso = generate_grid_dag(15, 15, dim=3, rho=0.0)
    # Cutset at col 7
    cut_iso = [r * 15 + 7 for r in range(15)]
    f_mono, t_mono, exp_mono, mem_mono = search_monolithic(G_iso, s_iso, e_iso)
    f_cut, t_cut, exp_cut, mem_cut = search_cutset_model_A(G_iso, s_iso, e_iso, cut_iso)
    
    identical = np.array_equal(np.sort(f_mono, axis=0), np.sort(f_cut, axis=0))
    speedup = t_mono / t_cut if t_cut > 0 else float('inf')
    
    results.append({
        "Topology": "Isotropic Grid (15x15, k=15)",
        "Dim": 3,
        "Identical": identical,
        "Mono Time": t_mono,
        "Cut Time": t_cut,
        "Speedup": speedup,
        "Mono Exp": exp_mono,
        "Cut Exp": exp_cut,
        "Mono Mem": mem_mono,
        "Cut Mem": mem_cut
    })
    
    # 4. High-Dim Bottleneck
    G_bot4, s_bot4, e_bot4, cut_bot4 = generate_bottleneck_dag(dim=4)
    f_mono, t_mono, exp_mono, mem_mono = search_monolithic(G_bot4, s_bot4, e_bot4)
    f_cut, t_cut, exp_cut, mem_cut = search_cutset_model_A(G_bot4, s_bot4, e_bot4, cut_bot4)
    
    identical = np.array_equal(np.sort(f_mono, axis=0), np.sort(f_cut, axis=0))
    speedup = t_mono / t_cut if t_cut > 0 else float('inf')
    
    results.append({
        "Topology": "Bottleneck (4D, k=2)",
        "Dim": 4,
        "Identical": identical,
        "Mono Time": t_mono,
        "Cut Time": t_cut,
        "Speedup": speedup,
        "Mono Exp": exp_mono,
        "Cut Exp": exp_cut,
        "Mono Mem": mem_mono,
        "Cut Mem": mem_cut
    })

    for r in results:
        print(f"--- {r['Topology']} ---")
        print(f"Identical: {r['Identical']}")
        print(f"Mono Time: {r['Mono Time']:.4f} s | Cut Time: {r['Cut Time']:.4f} s | Speedup: {r['Speedup']:.2f}x")
        print(f"Mono Exp: {r['Mono Exp']} | Cut Exp: {r['Cut Exp']}")
        print(f"Mono Mem: {r['Mono Mem']} | Cut Mem: {r['Cut Mem']}")

if __name__ == "__main__":
    run_benchmarks()
