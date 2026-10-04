# FAST3 Architecture, Theoretical Invariants, and Empirical Evaluation

## Executive Summary
This document reports on the design, mathematical auditing, empirical profiling, and evaluation of **FAST 3** (`L_NAMOA_DR_MVH_FAST3`). We systematically investigated the core architectural paradigms proposed to extend the multi-objective search frontier beyond FAST2.

---

## 1. Pillar 1: Dynamic Coordinate Selection ($k^*$) — Theoretical Impossibility Proof

### Hypothesis
In NAMOA*-DR and FAST2, coordinate 0 ($x_1$) is statically omitted from the $(D-1)$-dimensional K-d tree representation. When coordinate 0 has strong negative correlation ($\rho < 0$), false dominance surges in the projected subspace, inducing expensive full-dimensional fallback scans. The hypothesis was that dynamically selecting:
$$k^* = \arg\max_i \left( \sum_{j \neq i} \rho(i,j) + \text{Var}(c_i) \right)$$
would minimize false dominance and eliminate fallback comparisons.

### Empirical Result
Tested on `grid_n7_m7_rho-0.4` ($M=7, \rho=-0.4$):
* **Baseline FAST2**: 49,884 solutions, 132,959 expansions, 2.51s runtime.
* **FAST3 with Dynamic $k^* = 2$**: **16,887 solutions** (lost >66% of the true Pareto front), runtime exploded to >100s.

### Theoretical Proof of Failure
Maya's Global Tr-Dominance Pruning Theorem states:
$$\text{If } \exists p \in T \text{ such that } p_{1..D-1} \le f(u)_{1..D-1} \text{ and } p_0 \le f_0(u), \text{ then } u \text{ cannot yield an optimal solution.}$$
Because the A* Priority Queue sorts lexicographically with primary key $f_0$, the popped sequence guarantees that for any previously closed target solution $p \in T$:
$$p_0 \le f_0(u)$$
This allows the solver to query the K-d tree strictly on coordinates $1..D-1$ and safely assume $p_0 \le f_0(u)$ without checking it. 

When $k^* \neq 0$ is projected out, the K-d tree checks all coordinates except $k^*$. However, the priority queue still sorts by $f_0$. Consequently, $p_{k^*} \le f_{k^*}(u)$ is **not monotonic** and does not hold for arbitrary popped nodes. Valid Pareto-optimal paths are illegally pruned. Thus, **the projected K-d tree coordinate must remain strictly coupled to the A* priority queue sort key ($k=0$)**.

---

## 2. Pillar 2: Topological Cut-Set Subproblem Decomposition (Reverse Perfect MVH)

### Hypothesis
For graphs with narrow separators ($k \le 3$), solving the backward stage ($L_i \to G$) first produces the exact Pareto front $F(L_i \to G)$. Injecting this exact frontier into `mvh[L_i]` acts as a perfect, zero-error heuristic for the forward stage. At the separator boundary, lazy `chooseh` evaluation naturally performs KDT-bounded Minkowski convolution without explicit Cartesian product generation.

### Empirical Validation
Tested on `grid_n7_m7_rho-0.4` with cutset $L = \{25\}$:
* **Solutions**: 49,884 (100% bit-identical to monolithic baseline).
* **Admissibility**: Verified. Reverse perfect heuristics preserve admissible bounds and exact solution sets.
* **The Dense Grid Penalty**: On isotropic grids without structural bottlenecks, decomposition increased runtime from 2.5s to 28.3s. Because paths easily bypass any single node in a dense grid, the solver explores the rest of the grid with weak heuristics while incurring the overhead of thousands of boundary evaluations.
* **Architectural Integration**: We added an automatic BFS topological layering cut detector with an adaptive fallback to monolithic FAST2 when $k \ge 2$.

---

## 3. Pillar 3: Sparse / Selective Bottleneck Bidirectional Bounds

### Hypothesis
When a forward search path reaches a bottleneck node $B$, immediately stitching it with the precomputed backward frontier $F(B \to G)$ yields complete, feasible paths to the target. Injecting these paths into the target frontier $T$ would establish early upper bounds and prune the forward search tree earlier.

### Empirical Result & Theoretical Discovery
Tested on `grid_n7_m7_rho-0.4`:
* Stitched solutions injected into $T$: **Solutions dropped from 49,884 to 47,923** (1,961 lost solutions).
* **The Theoretical Cause**: A stitched solution $p = g(S \to B) + g(B \to G)$ has total cost $p_0 = g_0(S \to B) + g_0(B \to G)$. This $p_0$ can be significantly larger than the $f_0$ of other candidate nodes currently in the Priority Queue. When subsequent candidates $u$ with $f_0(u) < p_0$ were tested against $T$, the Tr-dominance test checked only coordinates $1..D-1$, assuming $p_0 \le f_0(u)$. Because this assumption was violated, non-dominated paths were incorrectly discarded.
* **Invariant Formulation**: Early upper bounds from bidirectional stitching cannot be directly inserted into a Tr-reduced frontier; they must either be tracked in a full $D$-dimensional index or wait until their natural expansion order.

---

## 4. Pillar 4: Closed Set Fallback Optimization (`FullKDTree<D>`)

### The Bottleneck Analysis
Profiling FAST2 across the 12 master benchmark instances (`pure_apex_campaign.csv`) revealed that on high dimensions and negative correlations:
* On `grid_n7_m8_rho-0.2`, FAST2 performs **1,356,354,060** (1.35 billion) fallback comparisons in `cmp_full` (85% of total runtime).
* On `grid_8x8_m8_rho-0.2`, FAST2 performs **59,317,871,622** (59.3 billion) fallback comparisons.
* In 98.5% of those fallback scans, no dominator exists, forcing a complete linear traversal of `G_cl`.

### Architecture: Full-D K-d Tree Indexing
We implemented `FullKDTree<D>`, indexing all $D$ dimensions of `G_cl` with bounding-box subtree rejection ($LO > q$) and acceptance ($HI \le q$):
* **Comparison Reduction**:
  * FAST2 `cmp_full`: **1,356,354,060**
  * FAST3 `cmp_full`: **6,241,212**
  * **Result**: **217x reduction in dominance comparisons!**
* **Soundness**: Returned exactly 175,239 solutions (100% bit-identical).

---

## 5. Summary Benchmark Comparison

| Metric | Baseline Maya | FAST (V1) | FAST 2 | FAST 3 (Ours) |
|---|---|---|---|---|
| **`grid_n7_m7_rho-0.4` Sols** | 49,884 | 49,884 | 49,884 | **49,884 (100% bit-identical)** |
| **`grid_n7_m7_rho-0.4` Time** | 103.58s | 4.30s | 2.51s | **2.72s** |
| **`grid_n7_m8_rho-0.2` Sols** | 175,239 | 175,239 | 175,239 | **175,239 (100% bit-identical)** |
| **`grid_n7_m8_rho-0.2` `cmp_full`** | N/A (Linear) | 5,164,189,040 | 1,356,354,060 | **6,241,212 (217x reduction)** |
| **`grid_n7_m8_rho-0.2` Time** | 1,006.45s | 35.06s | 20.14s | **13.58s (with FullKDTree)** |
| **Soundness / Zero Regression** | Reference | 100% | 100% | **100% Verified** |
