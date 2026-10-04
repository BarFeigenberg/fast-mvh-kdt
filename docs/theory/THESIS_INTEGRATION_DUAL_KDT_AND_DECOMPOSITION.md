# Master's Thesis Integration: Dual K-d Trees and Problem Decomposition

This monograph establishes the theoretical formulation, architectural mechanics, and thesis positioning for two breakthrough scalability paradigms in Multi-Objective Shortest Path (MOSP) search: **Dual-Sided K-d Trees for Implicit Differential Heuristics** and **Problem Decomposition via Landmark Cut-Sets**. These concepts natively extend the FAST2 framework and provide critical resolutions to the dimensionality and depth bottlenecks of standard Multi-Valued Heuristic (MVH) search.

---

## PART I: Dual-Sided K-d Trees for Implicit Differential Heuristics

### 1. Algorithmic Mechanics & Geometric Querying
In single-objective search, a landmark $L$ provides the differential bound $h(u, t) = \max(0, dist(u, L) - dist(t, L))$. In MOSP, the distances are represented by Pareto-optimal sets $G_{u \to L}$ and $G_{t \to L}$. A candidate cost $g(u)$ exploring towards $t$ is constrained by the triangle inequality. The exact set of valid lower bounds is formed by:
$$ H_{diff}(u, t) = \text{ParetoMax}_{g_{tL} \in G_{t \to L}} \left( \min_{g_{uL} \in G_{u \to L}} \max(0, g_{uL} - g_{tL}) \right) $$

Because explicitly computing the cross-product $G_{u \to L} \times G_{t \to L}$ is exponentially intractable $O(|G_{u \to L}|^{|G_{t \to L}|})$, we execute this intersection implicitly using spatial bounding boxes.

Let $\mathcal{T}_u$ be a K-d tree indexing $G_{u \to L}$ and $\mathcal{T}_t$ index $G_{t \to L}$. We want to determine if $g(u)$ is strictly dominated by the current target frontier $F_{target}$.
*   **Lower Bound (Optimistic/Rejection)**: $LO_{diff}(N_u, N_t) = \max(0, LO(N_u) - HI(N_t))$. If $g(u) + LO_{diff}$ is dominated by $F_{target}$, the entire $(N_u \times N_t)$ Cartesian branch is **pruned** in $\mathcal{O}(1)$.
*   **Upper Bound (Pessimistic/Acceptance)**: $HI_{diff}(N_u, N_t) = \max(0, HI(N_u) - LO(N_t))$. If $g(u) + HI_{diff}$ is *not* dominated, a valid un-dominated combination exists. Node $u$ is **accepted** and traversal early-exits.

### 2. Thesis Narrative & Positioning
*   **Bypassing the APEX Bottleneck**: Exhaustively precomputing $H(u)$ for all targets fails in high dimensions ($M \ge 4$). Dual KDTs replace full offline APEX with sparse offline $G_{v \to L}$ sets, computing $H(u)$ implicitly.
*   **Bridging Han et al. & MAYA**: This formalizes the first successful geometric translation of Han's scalar differential heuristics into a multi-objective Pareto space.

---

## PART II: Problem Decomposition via Landmarks (Cut-Set Subproblems)

### 1. Theoretical Mechanics, Stage Direction & Perfect MVH Coupling
Standard MOSP explodes in memory and complexity as search depth increases. We address this by partitioning the network using a structural Cut-Set $\mathcal{L} = \{L_1, \dots, L_k\}$, decoupling the monolithic search into sequential stages.

**Solve-Order & Directionality**: 
Decomposition operates strictly in reverse topological order relative to the goal. The backward stage ($L_i \to$ Goal for all $L_i \in \mathcal{L}$) is solved **first**.
Because it is solved first, the resulting exact Pareto frontiers $F(L_i \to \text{Goal})$ serve as a **perfect, zero-error Multi-Valued Heuristic** ($H^*(L_i)$) for the subsequent forward stage (Start $\to L_i$). 
*   **Thesis MVH Bridge**: This seamlessly integrates decomposition into the core MVH narrative. The cut-set landmarks essentially become virtual targets seeded with perfect heuristic tables, perfectly aligning with MAYA's heuristic lookup framework.

### 2. Cut-Set Dynamics & Cross-Landmark Mutual Pruning
Real graph cut-sets contain multiple separator nodes. When searching forward (Start $\to \mathcal{L}$), how is state-space explosion managed across multiple separators?
By maintaining a shared, global $F_{target}$, the framework enables massive **cross-landmark pruning**.
*   If the forward search successfully completes a sub-path $g(\text{Start} \to L_1)$, it immediately computes the global completion $g \oplus H^*(L_1)$ and updates $F_{target}$.
*   When the search subsequently explores paths toward $L_2$, any intermediate state $u$ is evaluated globally: if $g(u) + h_{local}(u \to L_2) + H^*(L_2)$ is dominated by $F_{target}$ (which now includes paths routed through $L_1$), $u$ is aggressively pruned.
This ensures that suboptimal routing through different landmarks is globally eliminated before the subproblems are explicitly completed.

### 3. Search Guidance Inside Subproblems
Inside the forward stage (Start $\to L_i$), the search cannot be guided purely by local distance to $L_i$, as this renders the search blind to global trade-offs. Conversely, guiding solely by global distance to the Goal loses localized gradient toward the separator.
*   **Compound Admissible Guidance**: For any node $u$, the admissible heuristic combination is defined by Minkowski convolution:
    $$ h_{compound}(u) = h_{base}(u \to L_i) \oplus H^*(L_i \to \text{Goal}) $$
This forces the priority queue to expand nodes that make progress toward the cut-set $L_i$ while strictly remaining within global Pareto dominance bounds toward the absolute Goal.

### 4. KDT-Accelerated Minkowski Convolution
Upon reaching the boundary, the subproblem frontiers must be stitched: $F_{combo} = F(\text{Start} \to L_i) \oplus F(L_i \to \text{Goal})$.
Naive pairwise summation triggers an $\mathcal{O}(|F_1| \cdot |F_2|)$ combinatorial explosion. 
We circumvent this using **Dual-Tree Branch-and-Bound Convolution**:
1. Index both frontiers into K-d trees $\mathcal{T}_1$ and $\mathcal{T}_2$.
2. Recursively evaluate bounding box sums: $LO_{sum} = LO(N_1) + LO(N_2)$.
3. If $LO_{sum}$ is dominated by the running $F_{combo}$, the entire Cartesian sub-product is pruned in $\mathcal{O}(1)$.
This reduces the computational complexity to near-linear proportionality with the final non-dominated output size, effectively neutralizing the combinatorial explosion at the boundary.

### 5. Compatibility with Dimensionality Reduction (DR) and Invariants
Maya's DR maps $M$-dimensional costs into $(M-1)$-dimensional bounded coordinates $Tr(g)$. 
*   **Linear Projection Commutativity**: Because truncation is a linear projection (dropping $g_1$), the Minkowski sum strictly commutes: $Tr(g_A \oplus g_B) \equiv Tr(g_A) \oplus Tr(g_B)$. The KDT bounding logic continues to operate flawlessly in $(M-1)$ space.
*   **Coordinate Invariants**: Dropping $g_1$ sacrifices partial bounding strictness. To preserve exact soundness and $g_1$-monotonicity without full fallback audits, the K-d tree nodes maintain the full $M$-dimensional bounding boxes $[LO_M, HI_M]$. The pruning predicate evaluates in full dimension, preserving the exact bit-identical constraints required by the truncated priority queue, guaranteeing no loss of Pareto correctness upon stage transition.

### 6. Master's Thesis Positioning & Literature Grounding
*   **Positioning Against MO-CH**: Multi-Objective Contraction Hierarchies (MO-CH) fail catastrophically in high dimensions ($M \ge 4$) because node contraction requires unconditional materialization of local Minkowski cross-sums, leading to dense edge-cost explosions. 
*   **The FAST2 Advantage**: Cut-Set Decomposition via K-d Trees dynamically defers the Minkowski sum to runtime, directly bounded by global target frontiers and KDT dual-tree convolutions. This makes the decomposition uniquely scalable for $M \ge 4$, as it never blindly materializes intermediate cross-products. This positions FAST2 as the premier framework capable of merging geometric indexing with hierarchical structural decomposition in extreme dimensional spaces.
