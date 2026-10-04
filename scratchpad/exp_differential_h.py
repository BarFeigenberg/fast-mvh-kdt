import numpy as np
import time

def pareto_frontier(costs):
    """Returns the pareto frontier of a set of 2D or 3D points."""
    is_efficient = np.ones(costs.shape[0], dtype=bool)
    for i, c in enumerate(costs):
        if is_efficient[i]:
            is_efficient[is_efficient] = np.any(costs[is_efficient] <= c, axis=1)
            is_efficient[i] = True
    return costs[is_efficient]

def generate_random_pareto_set(n_points=10, dims=2, seed=42):
    np.random.seed(seed)
    pts = np.random.randint(10, 100, size=(n_points*10, dims))
    return pareto_frontier(pts)

def naive_differential_heuristic(G_uL, G_tL):
    """
    Computes a set of valid lower bounds H(u, t).
    For any path cost c(u->t), and any g_tL in G_tL, 
    there exists a g_uL in G_uL such that g_uL <= c(u->t) + g_tL.
    Therefore, c(u->t) >= g_uL - g_tL.
    Since this must hold for ALL c(u->t) and ALL g_tL, it's complex.
    Actually, c(u->t) is independent of g_tL.
    For a fixed path u->t with cost c, and a fixed path t->L with cost g_tL,
    their concatenation has cost c + g_tL.
    This must be >= some g_uL in G_uL (the ideal or pareto front of u->L).
    Wait, if G_uL is the pareto front, then for ANY path u->L, there is a g_uL in G_uL that dominates it (<=).
    Wait no, the actual cost of u->t->L is c + g_tL.
    Since c + g_tL is a valid path from u to L, there MUST exist some g_uL in G_uL such that g_uL <= c + g_tL.
    So c >= g_uL - g_tL for that specific g_uL.
    But we don't know WHICH g_uL it will be.
    To make a valid lower bound for c, we can take the element-wise minimum of G_uL?
    If we just take ideal(G_uL) = min_{g in G_uL} g.
    Then ideal(G_uL) <= c + g_tL.
    So c >= ideal(G_uL) - g_tL.
    This holds for ANY g_tL in G_tL!
    So we can pick the g_tL that MAXIMIZES this bound!
    So c >= max_{g_tL in G_tL} (ideal(G_uL) - g_tL).
    But max over a vector means we might not get a single vector.
    Actually, we can form a set of lower bounds!
    H = { max(0, ideal(G_uL) - g_tL) | g_tL in G_tL }
    Let's test this logic.
    """
    ideal_uL = np.min(G_uL, axis=0)
    H = []
    for g_tL in G_tL:
        diff = ideal_uL - g_tL
        diff[diff < 0] = 0
        H.append(diff)
    H = np.array(H)
    return pareto_frontier(H) # We want the maximal lower bounds (which are non-dominated in the reversed sense, wait... bigger is better for a lower bound, so we actually want the pareto front where 'better' means greater. Since our pareto_frontier function assumes 'better' means smaller, we negate.)

def pareto_frontier_max(costs):
    # For lower bounds, we want vectors that are NOT dominated by other lower bounds.
    # A bound h1 dominates h2 if h1 >= h2 (elementwise) and h1 != h2.
    # We want to keep bounds that are not dominated by any other.
    return -pareto_frontier(-costs)

def build_diff_heuristic(G_uL, G_tL):
    ideal_uL = np.min(G_uL, axis=0)
    H = np.maximum(0, ideal_uL - G_tL)
    return pareto_frontier_max(H)

if __name__ == "__main__":
    print("Testing differential heuristics...")
    G_uL = generate_random_pareto_set(10, 2, 42)
    G_tL = generate_random_pareto_set(10, 2, 43)
    print("G_uL:\n", G_uL)
    print("Ideal G_uL:", np.min(G_uL, axis=0))
    print("G_tL:\n", G_tL)
    
    H = build_diff_heuristic(G_uL, G_tL)
    print("Generated Heuristic Set H(u, t):\n", H)
    
    # Test if it's admissible
    # Suppose actual cost c(u->t) is [20, 20]
    # c + g_tL must be >= some g_uL.
    # Let's say c = [20, 20]. Is it bounded by H?
    # For every h in H, is h <= c?
    # If c is a valid path, there must exist g_uL <= c + g_tL for all g_tL?
    # Actually, c + g_tL is a path from u to L.
    # So there MUST exist some g_uL in G_uL that dominates it.
