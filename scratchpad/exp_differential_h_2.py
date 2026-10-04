import numpy as np

def pareto_frontier(costs):
    is_efficient = np.ones(costs.shape[0], dtype=bool)
    for i, c in enumerate(costs):
        if is_efficient[i]:
            is_efficient[is_efficient] = np.any(costs[is_efficient] <= c, axis=1)
            is_efficient[i] = True
    return costs[is_efficient]

def pareto_frontier_max(costs):
    return -pareto_frontier(-costs)

# Let's say L is a landmark far to the right.
# u is far left, t is in the middle.
# So u->L is long, t->L is medium.
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
print("Ideal G_uL:", ideal_uL)
# [50, 50]

H = np.maximum(0, ideal_uL - G_tL)
print("Raw Diff Heuristic H:")
print(H)

H_pareto = pareto_frontier_max(H)
print("Maximal Diff Heuristic H_pareto:")
print(H_pareto)

# Is this admissible?
# Suppose actual cost c(u->t) is [40, 40].
# Then c + G_tL paths are:
# [80, 60], [70, 70], [60, 80]
# Notice that for EACH path here, there exists a path in G_uL that dominates it.
# [80, 60] is dominated by [80, 80]? No, [80, 60] dominates [80, 80] in standard MOSP where smaller is better.
# Wait, if G_uL is the pareto front of ALL paths from u to L.
# And c is ONE valid path from u to t. 
# Then concatenating c with ANY path in G_tL yields a valid path from u to L.
# Therefore, that concatenated path MUST be >= SOME path in G_uL.
# For example, c + [40, 20] = [80, 60].
# In G_uL, there must be a path <= [80, 60].
# Is there? [100, 50], [80, 80], [50, 100].
# None of these are <= [80, 60]! 
# Wait. If G_uL is the EXACT pareto front of all paths from u to L in the graph,
# and c is a VALID path from u to t in the graph, 
# then c + [40, 20] WOULD BE a valid path from u to L.
# So G_uL MUST contain a path <= [80, 60].
# Since it doesn't in our synthetic example, it means c=[40,40] is an IMPOSSIBLE actual cost for a graph that has these exact G_uL and G_tL!
