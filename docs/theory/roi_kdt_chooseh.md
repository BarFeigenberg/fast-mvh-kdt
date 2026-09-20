# Roi's Geometric Acceleration: K-d Tree Pruning for `CHOOSEH`

## 1. Motivation and Core Idea

In multi-objective search with multi-valued heuristics (L-NAMOA*dr-MVH), each state $s$ has a static heuristic set $H(s)$. The heuristic selection operation `CHOOSEH(s, g, T)` finds the first heuristic vector $h \in H(s)$ (in lexicographical order) whose projected evaluation:
$$\operatorname{Tr}(\mathbf{f}) = \operatorname{Tr}(\mathbf{g}) + \operatorname{Tr}(\mathbf{h})$$
is **not** dominated by any known solution in the truncated goal frontier $T = G^{\mathrm{Tr}}_{\mathrm{cl}}(s_{\mathrm{goal}})$.

While the goal frontier $T$ grows dynamically during search as new Pareto-optimal solutions are discovered, the heuristic set $H(s)$ for any state $s$ is **static** and known ahead of time. Roi's insight is to pre-index the truncated heuristics $\operatorname{Tr}(H(s))$ into a static $(d-1)$-dimensional K-d tree.

---

## 2. K-d Tree Structure and Node Attributes

For each state $s \in S$, we build a static $(d-1)$-dimensional K-d tree over the truncated heuristics:
$$X = \{(\operatorname{Tr}(\mathbf{h}_i), i) \mid \mathbf{h}_i \in H(s), i \in \{1, \dots, |H(s)|\}\}$$

Each node $N$ in the tree represents a subset of heuristics $H_N \subseteq H(s)$ and stores:
1. **Lexicographic Index Set $N.I$**: The set of indices of all heuristics contained in the subtree rooted at $N$:
   $$N.I = \{i \mid \mathbf{h}_i \in H_N\}$$
2. **Component-wise Minimum Bounding Box $N.\min$**:
   $$N.\min = \left(\min_{\mathbf{h} \in H_N} h_2, \min_{\mathbf{h} \in H_N} h_3, \dots, \min_{\mathbf{h} \in H_N} h_d\right)$$
3. **Component-wise Maximum Bounding Box $N.\max$**:
   $$N.\max = \left(\max_{\mathbf{h} \in H_N} h_2, \max_{\mathbf{h} \in H_N} h_3, \dots, \max_{\mathbf{h} \in H_N} h_d\right)$$
4. **Leaf Flag and Elements**:
   - If $|H_N| \le B$ (where $B$ is the leaf capacity threshold), $N$ is a leaf node storing its elements $N.X$.
   - Otherwise, $N$ is an internal node with left and right children $N.\mathrm{left}$ and $N.\mathrm{right}$.

For every heuristic $\mathbf{h} \in H_N$, the invariant holds:
$$N.\min \preceq \operatorname{Tr}(\mathbf{h}) \preceq N.\max$$

---

## 3. Tree Construction (`BuildKDTree`)

Splits are chosen along the dimension of maximum spread:
1. If $|X| \le B$, mark as leaf and terminate.
2. Select splitting coordinate:
   $$j = \arg\max_{m \in \{1, \dots, d-1\}} \left(\max_{(\mathbf{x}, i) \in X} x_m - \min_{(\mathbf{x}, i) \in X} x_m\right)$$
3. Find median element along coordinate $j$ and partition $X$ into $X_{\mathrm{left}}$ and $X_{\mathrm{right}}$.
4. Recursively build left and right subtrees.

---

## 4. Aggregate Dominance Pruning Rules

Let $T = G^{\mathrm{Tr}}_{\mathrm{cl}}(s_{\mathrm{goal}})$ be the current truncated goal frontier, and let $\operatorname{Dom}(T, \mathbf{q}) \iff \exists \mathbf{t} \in T \text{ s.t. } \mathbf{t} \preceq \mathbf{q}$.

For a search path reaching state $s$ with accumulated path cost $\mathbf{g}$:

### Rule 1: Subtree Discard (Aggregate Minimum)
- Compute the lower bound on all truncated evaluations in the subtree:
  $$\mathbf{q}_{\min} = \operatorname{Tr}(\mathbf{g}) + N.\min$$
- If $\operatorname{Dom}(T, \mathbf{q}_{\min})$ is **true**:
  $$\exists \mathbf{t} \in T \text{ s.t. } \mathbf{t} \preceq \mathbf{q}_{\min} \implies \forall \mathbf{h} \in H_N, \, \mathbf{t} \preceq \operatorname{Tr}(\mathbf{g}) + \operatorname{Tr}(\mathbf{h})$$
- **Action**: All heuristic vectors in subtree $H_N$ are guaranteed to be dominated by $T$. Mark all indices in $N.I$ as dominated ($D \leftarrow D \cup N.I$) and **prune the entire subtree immediately without further traversal**.

### Rule 2: Subtree Acceptance (Aggregate Maximum)
- Compute the upper bound on all truncated evaluations in the subtree:
  $$\mathbf{q}_{\max} = \operatorname{Tr}(\mathbf{g}) + N.\max$$
- If $\operatorname{Dom}(T, \mathbf{q}_{\max})$ is **false**:
  $$\neg \exists \mathbf{t} \in T \text{ s.t. } \mathbf{t} \preceq \mathbf{q}_{\max} \implies \forall \mathbf{h} \in H_N, \, \neg \exists \mathbf{t} \in T \text{ s.t. } \mathbf{t} \preceq \operatorname{Tr}(\mathbf{g}) + \operatorname{Tr}(\mathbf{h})$$
- **Action**: None of the heuristic vectors in subtree $H_N$ can be dominated by $T$. **Accept the entire subtree immediately without further traversal**. None of its elements are added to $D$.

### Rule 3: Recursive Refinement
- If neither test is decisive:
  - If $N$ is a leaf, test each heuristic $(\operatorname{Tr}(\mathbf{h}_i), i) \in N.X$ individually against $T$. If dominated, add $i \in D$.
  - If $N$ is internal, recursively visit $N.\mathrm{left}$ and $N.\mathrm{right}$.

---

## 5. Selection Rule: Strictly Preserving Maya's Semantics

Maya Wohlf's algorithm requires that the heuristic chosen is the **lexicographically smallest** surviving heuristic in $H(s)$ to maintain exact tie-breaking, search trajectories, and node expansion order.

After `KD-Prune` populates the dominated index set $D$:
```cpp
// KD-ChooseH(s, g, T)
for (size_t i = 0; i < |H(s)|; ++i) {
    if (!D.contains(i)) {
        return h_i;
    }
}
return std::nullopt;
```

### Soundness & Equivalence Theorem
- Every heuristic pruned by `KD-Prune` is provably dominated by some $\mathbf{t} \in T$.
- Every heuristic accepted by `KD-Prune` is provably non-dominated by all $\mathbf{t} \in T$.
- The first non-dominated index in $\{0, \dots, |H(s)|-1\} \setminus D$ is identical to the first index found by linear scanning in Maya's `get_first_undominated_heuristic_value`.
- Therefore, substituting `KD-ChooseH` produces an identically behaving search with bit-identical Pareto fronts while skipping the vast majority of individual vector comparisons.
