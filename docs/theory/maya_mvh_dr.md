# Multi-Valued Heuristics with Dimensionality Reduction (L-NAMOA*dr-mvh)

## 1. Executive Summary & Algorithmic Background

In Multi-Objective Shortest Path (MOSP) problems on directed graphs $G = \langle S, E, c \rangle$ with $d$-dimensional non-negative cost vectors $c: E \to \mathbb{R}^d_{\ge 0}$, the objective is to compute the cost-unique Pareto-optimal frontier:
$$\Pi^* = \mathrm{ND}(\{\mathbf{c}(\pi) \mid \pi \text{ is a path from } s_{\mathrm{start}} \text{ to } s_{\mathrm{goal}}\})$$
where vector dominance is defined as:
- Weak dominance: $\mathbf{x} \preceq \mathbf{y} \iff \forall i \in \{1, \dots, d\}, x_i \le y_i$
- Strict dominance: $\mathbf{x} \prec \mathbf{y} \iff \mathbf{x} \preceq \mathbf{y} \text{ and } \mathbf{x} \ne \mathbf{y}$

### Multi-Valued Heuristics (MVH)
Rather than mapping each state $s \in S$ to a single lower-bound vector $\mathbf{h}(s)$, a Multi-Valued Heuristic maps $s$ to a finite set of mutually non-dominated heuristic vectors:
$$H: S \to 2^{\mathbb{R}^d_{\ge 0}}, \quad H(s) = \{\mathbf{h}_1, \mathbf{h}_2, \dots, \mathbf{h}_{|H(s)|}\}$$
An MVH is **admissible** if for every state $s$ and every path $\pi$ from $s$ to $s_{\mathrm{goal}}$, there exists at least one $\mathbf{h} \in H(s)$ such that $\mathbf{h} \preceq \mathbf{c}(\pi)$.

### Dimensionality Reduction (DR)
Standard dimensionality reduction projects cost vectors by truncating the first objective coordinate:
$$\operatorname{Tr}(\mathbf{x}) = (x_2, x_3, \dots, x_d) \in \mathbb{R}^{d-1}_{\ge 0}$$
When search evaluates path cost $\mathbf{g}$ and heuristic vector $\mathbf{h}$, the truncated evaluation vector is:
$$\operatorname{Tr}(\mathbf{f}) = \operatorname{Tr}(\mathbf{g} + \mathbf{h}) = \operatorname{Tr}(\mathbf{g}) + \operatorname{Tr}(\mathbf{h})$$
In single-valued heuristic search with DR (e.g., NAMOA*dr, BOA*), if nodes are expanded in monotonic order of $g_1$ or $f_1$, checking dominance in the $(d-1)$-dimensional projected space is sufficient to establish dominance in the full $d$-dimensional space.

---

## 2. Incompatibility of MVH with Standard DR

When using Multi-Valued Heuristics, different paths reaching the same state $s$ or successive states can be associated with different heuristic vectors from $H(s)$. Because $\mathrm{OPEN}$ orders nodes lexicographically by $\mathbf{f} = \mathbf{g} + \mathbf{h}$, nodes reaching state $s$ are **not** guaranteed to be expanded in monotonically increasing order of their $g_1$ coordinates. 

Under naive DR, if a path arrives with lower $g_1$ after a path with higher $g_1$ has already closed the state, the earlier path cannot safely prune the later path based solely on $\operatorname{Tr}(\mathbf{g})$. Naive truncation therefore yields unsound pruning, dropping valid Pareto-optimal solutions.

---

## 3. The L-NAMOA*dr-mvh Mechanism

To preserve search correctness with arbitrary admissible MVHs without imposing prohibitive consistency constraints, **L-NAMOA*dr-mvh** (Wolff, Felner, and Salzman 2026) combines two core mechanisms:
1. **Lazy Node Expansion**: A physical path $\mathbf{g}$ is represented by a single node lazily bound to one heuristic at a time, ordered by $\mathbf{f}(n) = \mathbf{g}(n) + \mathbf{h}(n)$ in $\mathrm{OPEN}$.
2. **Optimistic DR with Fallback Verification**: 
   - State $s$ maintains two closed sets:
     - $G^{\mathrm{Tr}}_{\mathrm{cl}}(s)$: Truncated non-dominated cost vectors $\operatorname{Tr}(\mathbf{g})$.
     - $G_{\mathrm{cl}}(s)$: Full $d$-dimensional path-cost vectors $\mathbf{g}$.

### Local Dominance Check (`LOCALDOMCHECK`)
When testing a new arrival $\mathbf{g}$ at state $s$:
```
function LOCALDOMCHECK(s, g):
    // 1. Optimistic DR check in (d-1) dimensions
    if exists g' in G_Tr_cl(s) such that Tr(g') <= Tr(g):
        // Check monotonicity condition:
        if max_{g' in G_Tr_cl(s)} g'_1 <= g_1:
            return true  // Soundly dominated via t-discarding
        
        // Monotonicity violated: fallback to full d-dimensional check
        if exists g' in G_cl(s) such that g' <= g:
            return true  // Bad fallback (dominated in full space)
        return false     // Good fallback (salvaged valid path)
        
    return false
```

- **T-discarding**: If $\operatorname{Tr}(\mathbf{g}') \preceq \operatorname{Tr}(\mathbf{g})$ and $\max_{\mathbf{g}'} g'_1 \le g_1$, then $\mathbf{g}' \preceq \mathbf{g}$ holds unconditionally in all $d$ dimensions.
- **Fallback Verification**: If $\operatorname{Tr}(\mathbf{g}') \preceq \operatorname{Tr}(\mathbf{g})$ but $g_1 < \max_{\mathbf{g}'} g'_1$, the truncation check is inconclusive. The algorithm falls back to an exact full Pareto check against $G_{\mathrm{cl}}(s)$.

---

## 4. The Heuristic Selection Bottleneck: `CHOOSEH`

In Maya Wohlf's baseline (`baselines/bridging-mvh-dr`), whenever a node $n$ is generated, expanded, or re-inserted following global dominance pruning, the algorithm must select the next admissible heuristic vector:

```cpp
// From L_NAMOA_DR_MVH::get_first_undominated_heuristic_value
for (size_t idx = start_idx; idx < node_mvh.size(); ++idx) {
    const auto& heuristic_value = node_mvh[idx];
    bool dominated = false;
    for (const auto& truncated_g : truncated_non_dominated_g[target]) {
        bool is_dominated = true;
        for (size_t i = 1; i < heuristic_value.size(); i++) {
            if (heuristic_value[i] + g_value[i] < truncated_g[i]) {
                is_dominated = false;
                break;
            }
        }
        if (is_dominated) {
            dominated = true;
            break;
        }
    }
    if (!dominated) {
        return std::make_pair(heuristic_value, idx);
    }
}
return std::nullopt;
```

### Analysis of the Bottleneck
1. **Linear Scan over Heuristics**: If $|H(s)|$ heuristic vectors exist at state $s$, `CHOOSEH` performs a linear loop over all remaining heuristics.
2. **Nested Scan over Goal Frontier**: For every candidate $\mathbf{h} \in H(s)$, it iterates across all $|G^{\mathrm{Tr}}_{\mathrm{cl}}(s_{\mathrm{goal}})|$ solutions in the truncated goal frontier.
3. **Worst-Case Cost**: Testing a single node takes $\mathcal{O}(|H(s)| \cdot |G^{\mathrm{Tr}}_{\mathrm{cl}}(s_{\mathrm{goal}})| \cdot (d-1))$ comparisons.
4. **Repeated Calls**: In rich heuristic regimes (differential heuristics, grid maps with large heuristic closures), $|H(s)|$ ranges from hundreds to tens of thousands. With tens of thousands of node generations and goal frontier updates, `CHOOSEH` dominates over 70% to 90% of total search time.

Accelerating this heuristic selection without altering the returned heuristic index or search trajectory is the central mission of this project.
