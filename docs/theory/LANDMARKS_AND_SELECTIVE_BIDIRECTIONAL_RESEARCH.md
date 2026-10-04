# Research Report: Landmarks and Selective Bidirectional Search in MOSP

This report details the findings of an independent theoretical and empirical investigation into three advanced search principles for Multi-Objective Shortest Path (MOSP) problems. The findings dictate the feasibility and geometric dynamics of differential heuristics, problem decomposition, and selective bidirectional bounds.

---

## 1. Landmark-Based Differential Heuristics with Dual-Sided K-d Trees

### Concept
Extending Han et al.'s differential heuristics ($|dist(u, L) - dist(v, L)|$) to MOSP requires computing a valid lower bound from two sets of Pareto optimal paths: $G_{u \to L}$ and $G_{t \to L}$.

### Theoretical Derivation
For any actual cost $c$ from $u$ to $t$, the concatenation of $c$ with any path $g_{tL} \in G_{t \to L}$ yields a valid path from $u$ to $L$. Therefore, $c + g_{tL}$ must be dominated by (or equal to) some path $g_{uL} \in G_{u \to L}$.

Mathematically, $c$ must reside in the region:
$$ c \in \bigcap_{g_{tL} \in G_{t \to L}} \bigcup_{g_{uL} \in G_{u \to L}} \{ c \mid c \geq g_{uL} - g_{tL} \} $$

The exact differential heuristic set $H_{diff}(u, t)$ is the Pareto front of the element-wise maxima of the Cartesian product of the Minkowski differences.

### Empirical Findings
A synthetic benchmark (`scratchpad/exp_differential_h_3.py`) revealed a massive gap between ideal-point relaxation and the exact differential heuristic:
- **Ideal-Point Bound**: Subtracting the ideal point of $G_{t \to L}$ from the ideal point of $G_{u \to L}$ collapses the heuristic to a *single vector* (e.g., `[30, 30]`).
- **Exact Pareto Bound**: The exact intersection yields a rich, broad Pareto front of lower bounds (e.g., `[[30, 80], [40, 70], [50, 60], ..., [80, 30]]`). Every point in this front strictly dominates the single ideal-point vector.

### Conclusion & KDT Implications
Computing the exact Cartesian product is $O(|G_{u \to L}|^{|G_{t \to L}|})$, making it intractable. However, indexing both $G_{u \to L}$ and $G_{t \to L}$ using **Dual-Sided K-d Trees** allows us to query the intersection of these lower bounds efficiently without explicitly generating the Cartesian product, thereby recovering massive pruning power over naive ideal-point heuristics.

---

## 2. Problem Decomposition via Landmarks (Subproblem Search with KDT)

### Concept
Can we break a monolithic $S \to G$ search into smaller bidirectional segments $S \to L_{cut}$ and $L_{cut} \to G$ to limit the combinatorial explosion of intermediate frontiers?

### Empirical Findings
We simulated an $N$-layer DAG and decomposed the search at a specific cut-layer (a bottleneck cut-set where all paths must pass).
- **Monolithic Search**: Maintained a total of **411** intermediate paths across all nodes.
- **Bidirectional Decomposition**: Maintained only **221** intermediate paths.

### Conclusion
By forcing the forward and backward searches to meet exclusively at the $L_{cut}$ cut-set, we prevent the combinatorial merging of paths at every intermediate layer. The cross-sum of frontiers $F(S \to L) \bowtie F(L \to G)$ is computed strictly at the cut-set, resulting in substantially smaller memory footprints and fewer intermediate paths while yielding the exact same final Pareto front.

---

## 3. Sparse / Selective Bidirectional Information

### Concept
Instead of maintaining a full backward frontier at every node (which causes `cmpchk` and `cmpupd` to explode), we track bidirectional information only at a specific subset of nodes $S_{select}$ (e.g., high-degree bottlenecks).

### Empirical Findings
A network benchmark (`scratchpad/exp_selective_bi.py`) was executed to track expansions and dominance check counters:
1. **Monolithic Forward**: 137 expansions, 0 checks.
2. **Selective Bidirectional (Bottleneck Only)**: 130 expansions, **31 dominance checks**.
3. **Full Bidirectional (All Nodes)**: 120 expansions, **308 dominance checks**.

### Conclusion
Full bidirectional search reduces expansions slightly more but incurs a **10x penalty** in dominance checks (`cmpchk`), severely degrading wall-clock performance. By storing backward frontiers *only* at bottleneck states, we achieve a balanced "sweet spot"—pruning doomed paths before they diverge past the bottleneck while minimizing the exorbitant computational overhead of full meet-in-the-middle verification.

---

## Final Recommendations for FAST / MAYA Frameworks
1. **Implement Dual-Tree Heuristic Filtering**: Avoid ideal-point collapse in MOSP differential heuristics. KDTs should be configured to query dominance against the implicit Cartesian lower bounds.
2. **Bottleneck Targeting**: Do not deploy ubiquitous backward search. Pre-compute backward frontiers *only* from identified structural bottlenecks or landmark cut-sets, and restrict bounding checks (`better_local_dominance_check`) to those specific topological layers.
