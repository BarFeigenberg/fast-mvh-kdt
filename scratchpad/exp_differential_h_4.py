import numpy as np
import itertools

def pareto_frontier(costs):
    is_efficient = np.ones(costs.shape[0], dtype=bool)
    for i, c in enumerate(costs):
        if is_efficient[i]:
            dominate = np.all(c <= costs, axis=1) & np.any(c < costs, axis=1)
            is_efficient[dominate] = False
    return costs[is_efficient]

G_uL = np.array([
    [100, 50],
    [80, 80],
    [50, 100]
])

G_tL = np.array([
    [40, 20],
    [30, 30],
    [20, 40]
])

# Compute S_j = B(g_{tL})
S = []
for g_tL in G_tL:
    # S_j is the set of lower bounds for this g_tL
    # max(0, g_uL - g_tL)
    B = np.maximum(0, G_uL - g_tL)
    # We only need the pareto front of B (minimal elements)
    B_front = pareto_frontier(B)
    S.append(B_front)

# Now take the Cartesian product of S_1 x S_2 x ... x S_m
all_combinations = list(itertools.product(*S))

# For each combination, take the element-wise max
max_vectors = []
for combo in all_combinations:
    combo = np.array(combo)
    max_vec = np.max(combo, axis=0)
    max_vectors.append(max_vec)

max_vectors = np.array(max_vectors)
true_H = pareto_frontier(max_vectors)

print("Mathematically derived true H(u, t):")
print(true_H)
