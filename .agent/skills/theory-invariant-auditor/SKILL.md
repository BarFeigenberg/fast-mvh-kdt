---
name: theory-invariant-auditor
description: Audit any proposed search logic against core multi-objective search theorems and literature specifications in docs/theory/.
---

# Theory Invariant Auditor Skill

## Purpose
Acts as a rigorous mathematical and algorithmic gatekeeper before any code changes are proposed or applied to `src/`. It audits data structures, dominance checks, pruning rules, and search trajectory operations against the published theorems and formal specifications established in `docs/theory/`.

---

## Mandatory Invariants to Audit

### 1. Antichain & Admissibility Invariant
- **Admissibility**: Every heuristic set $H(s)$ must satisfy $\forall \pi \text{ from } s \text{ to } s_{\mathrm{goal}}, \, \exists \mathbf{h} \in H(s) \text{ s.t. } \mathbf{h} \preceq \mathbf{c}(\pi)$.
- **Antichain Property**: Fronts $G_{\mathrm{cl}}(s)$, $G^{\mathrm{Tr}}_{\mathrm{cl}}(s)$, and solution sets $\Pi^*$ must strictly form antichains (no vector weakly dominates another in the same set).
- **No False Pruning**: Pruning a node or subtree is permitted **if and only if** every represented path is provably weakly dominated by an already discovered path or known solution.

### 2. Lexicographic vs. Spatial Separation (Shahaf Theorem 1)
- **Lower Bound Respect**: For objective counts $M \ge 4$, total-order indices (single-coordinate lexicographical BSTs / AVLs) are asymptotically bounded by $\Omega(n)$ dominance tests over an antichain of size $n$.
- **Spatial Multi-Coordinate Partitioning**: Any index aiming for sub-linear orthant-emptiness $\mathcal{O}(n^{1 - 1/d})$ (where $d = M - 1$) must partition coordinates simultaneously across multiple spatial dimensions (via K-d trees or spatial bounding boxes).
- **Correct Pruning Invariant**: In spatial index queries, subtrees can be skipped only if the corner-minimum satisfies $\mathbf{lo}_u \not\preceq \mathbf{g}$. If $\mathbf{lo}_u \preceq \mathbf{g}$, the orthant may contain dominators and the subtree must not be prematurely discarded.

### 3. Optimistic DR Fallback Condition (Maya Wolff, SoCS 2026)
- **Dimensionality Reduction via Truncation**: $\operatorname{Tr}(\mathbf{x}) = (x_2, \dots, x_d)$ operates in $(d-1)$ dimensions.
- **T-Discarding Condition**: Truncated dominance $\operatorname{Tr}(\mathbf{g}') \preceq \operatorname{Tr}(\mathbf{g})$ guarantees full $d$-dimensional dominance $\mathbf{g}' \preceq \mathbf{g}$ **if and only if** monotonicity holds:
  $$\max_{\mathbf{g}' \in G^{\mathrm{Tr}}_{\mathrm{cl}}(s)} g'_1 \le g_1$$
- **Mandatory Fallback**: If $\operatorname{Tr}(\mathbf{g}') \preceq \operatorname{Tr}(\mathbf{g})$ but $g_1 < \max_{\mathbf{g}'} g'_1$, the dimensionality reduction is inconclusive. The algorithm **must fall back** to an exact full Pareto dominance check against $G_{\mathrm{cl}}(s)$. Pruning without this fallback produces an unsound search that discards valid Pareto-optimal solutions.

### 4. Single Search Node Representation & Expansion Invariant
- **Single Node Per Path**: A physical search path reaching state $s$ with cost $\mathbf{g}$ must be represented by a **single search node** in $\mathrm{OPEN}$ at any given time.
- **Lazy Heuristic Association**: Search nodes must lazily associate with one heuristic vector $\mathbf{h} \in H(s)$ at a time, ordered by $\mathbf{f} = \mathbf{g} + \mathbf{h}$.
- **Re-evaluation on Goal Dominance**: When the currently selected heuristic evaluation $\operatorname{Tr}(\mathbf{f})$ is dominated by a newly discovered goal solution, the node must not be permanently discarded; it must be re-evaluated using `CHOOSEH` for the next undominated heuristic in $H(s)$ and re-inserted into $\mathrm{OPEN}$.
- **Tie-Breaking Fidelity**: Heuristic selection in `CHOOSEH` or `KD-ChooseH` must strictly return the **lowest lexicographical index** $h_i \in H(s)$ among surviving candidates, guaranteeing identical tie-breaking and node expansion sequences as Maya's baseline.

---

## Audit Checklist for Proposed Changes

Before presenting any code modification for user sign-off, verify:
- [ ] Does the change alter the heuristic selection order or tie-breaking in `CHOOSEH`? (Must be: **NO**)
- [ ] Does any pruning rule discard a candidate when it could be non-dominated in the full $d$-dimensional space? (Must be: **NO**)
- [ ] Is the optimistic DR fallback check preserved without shortcuts? (Must be: **YES**)
- [ ] Does the proposed index use contiguous flat memory without pointer bloat? (Must be: **YES**)
- [ ] Will the resulting Pareto front remain 100% bit-identical on benchmark fixtures? (Must be: **YES**)
