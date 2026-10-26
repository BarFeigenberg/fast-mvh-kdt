import numpy as np
import matplotlib.pyplot as plt

def pareto_frontier(costs):
    is_efficient = np.ones(costs.shape[0], dtype=bool)
    for i, c in enumerate(costs):
        if is_efficient[i]:
            # Keep points that are NOT strictly dominated by c
            # A point x is dominated by c if c <= x and c != x
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

ideal_uL = np.min(G_uL, axis=0)
ideal_tL = np.min(G_tL, axis=0)

print("Ideal uL:", ideal_uL)
print("Ideal tL:", ideal_tL)
ideal_bound = np.maximum(0, ideal_uL - ideal_tL)
print("Ideal-based single vector bound:", ideal_bound)

# Let's find the true intersection of regions R(g_tL).
# A point c is valid if for EVERY g_tL in G_tL, there is SOME g_uL in G_uL such that g_uL <= c + g_tL.
# Which means c >= g_uL - g_tL.
# We can just sample a grid of c's and see which ones are valid!
grid_x = np.arange(0, 100, 2)
grid_y = np.arange(0, 100, 2)
valid_cs = []

for cx in grid_x:
    for cy in grid_y:
        c = np.array([cx, cy])
        
        is_valid_overall = True
        for g_tL in G_tL:
            # Does there exist g_uL <= c + g_tL?
            # c + g_tL
            shifted = c + g_tL
            
            # check if any g_uL is <= shifted
            if not np.any(np.all(G_uL <= shifted, axis=1)):
                is_valid_overall = False
                break
                
        if is_valid_overall:
            valid_cs.append(c)

valid_cs = np.array(valid_cs)
if len(valid_cs) > 0:
    pf = pareto_frontier(valid_cs)
    print("True differential heuristic Pareto front:")
    print(pf)
else:
    print("No valid c found in grid.")
