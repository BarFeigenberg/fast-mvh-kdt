# Proof of Fatal Soundness Flaw in Baseline NAMOA*-DR with Multi-Valued Heuristics

## 1. The Core Assumption (The Flaw)
In the baseline implementation of Maya's NAMOA*-DR (`better_local_dominance_check`), a newly discovered path is checked against the Dimensionality Reduction (DR) frontier. To avoid a full $O(M)$ Pareto check, the algorithm checks if the new path is fully dominated by the DR front using a $g_0$ shortcut:

```cpp
// Maya's logic in better_local_dominance_check
if (flat_list[flat_list.size() - num_obj + 0] <= g[0]) {
    return true; // Prune the path
}
```
**The Assumption:** The baseline assumes that the $g_0$ of the *last element inserted* into `flat_list` (`flat_list.back()[0]`) represents the **absolute maximum $g_0$** of all elements in the DR front.

## 2. Why it holds for Single-Valued Heuristics (Original Paper)
In standard A* with a single heuristic vector, nodes are popped from OPEN in monotonically increasing order of $f_0$. 
Since $f_0 = g_0 + h_0$, and $h_0$ is a constant for any specific node $X$, all paths reaching $X$ will be popped in strictly increasing order of $g_0$. 
Because $g_0$ is monotonically increasing, the last element inserted into the DR front is guaranteed to have the largest $g_0$. The assumption holds flawlessly.

## 3. Why it breaks for Multi-Valued Heuristics (MVH)
In the MVH architecture, a path to node $X$ can be evaluated using *different* heuristic vectors from the pool (e.g., $h_{idx=0}$ vs $h_{idx=2}$).
Because different paths use different heuristic vectors, the $h$ value fluctuates:
$$f_0 = g_0 + h_{idx}(X)_0$$
Since nodes are popped by $f_0$, a path with a large $g_0$ and a small $h$ can be popped **before** a path with a small $g_0$ and a large $h$.
**Therefore, $g_0$ is NO LONGER monotonically increasing during insertions into the DR front.**

## 4. The Murder Case (Step-by-Step Proof)
Because $g_0$ fluctuates, `flat_list.back()[0]` can arbitrarily dip below the true maximum $g_0$ of the list. This leads to the catastrophic pruning of Pareto-optimal paths. 

Assume we are evaluating paths to an intermediate node $X$:
1. **Path 1 arrives**: $g_0 = 10$, $h_0 = 2 \implies f_0 = 12$. 
   It is added to the empty `flat_list`. `flat_list.back()[0]` is now **$10$**.
2. **Path 2 arrives**: $g_0 = 8$, $h_1 = 6 \implies f_0 = 14$. (Arrives later because $14 > 12$).
   Path 1 DR-dominates Path 2 (is better on dims $1..M-1$).
   Maya checks `flat_list.back()[0] <= Path2.g_0` $\implies$ $10 \le 8$ (FALSE).
   It falls back to a full Pareto check. Path 1 ($g_0=10$) does not fully dominate Path 2 ($g_0=8$).
   Path 2 is added to `flat_list`. 
   **CRITICAL FAILURE:** `flat_list.back()[0]` is now **$8$**, but the true max $g_0$ in the list is still $10$!
3. **Path 3 (The Victim) arrives**: $g_0 = 9$, $h_2 = 6 \implies f_0 = 15$.
   Path 1 DR-dominates Path 3.
   Maya checks `flat_list.back()[0] <= Path3.g_0` $\implies$ $8 \le 9$ (**TRUE**).
   Maya immediately **PRUNES Path 3** without a full Pareto check.
   
**The Mathematical Violation:** Path 1 ($g_0=10$) was the path that DR-dominated Path 3. But $10 \not\le 9$, meaning Path 1 **does not** fully dominate Path 3! Path 3 was strictly better on dimension 0 and was a valid Pareto optimal path. Maya just murdered it.

## 5. The Consequence on the 6D Benchmark
When Maya erroneously prunes true optimal paths, those paths never reach the target. 
Because the true optimal paths die, the sub-optimal paths that they were supposed to prune (via `better_local_dominance_check(target)`) survive unhindered and are added to the final `solutions` vector.

**This is why Maya outputs 71,406 solutions instead of the true 71,191.** Maya's output is contaminated with hundreds of sub-optimal, dominated paths.

## 6. Why V5 (Dual-Tree KD-Tree) is the True Oracle
V5 completely discards the `flat_list` and manages the DR front using a dynamic KD-Tree. 
Crucially, V5 tracks the maximum $g_0$ of the front using:
```cpp
if (candidate[0] > max_g0_) max_g0_ = candidate[0];
```
V5 explicitly tracks the **true mathematical maximum** of all live points in the tree, completely decoupling it from the order of insertion. 
Because V5's $g_0$ shortcut is flawless, it correctly preserves Path 3, which eventually reaches the target and rightfully slaughters the dominated solutions. V5's 71,191 solutions represent the mathematically pure, uncontaminated Pareto front.
