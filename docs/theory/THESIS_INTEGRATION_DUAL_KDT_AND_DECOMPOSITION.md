# Master's Thesis Integration: Differential Heuristics and Cut-Set Problem Decomposition

This monograph establishes the theoretical formulation, architectural mechanics, and empirical validation for two advanced scalability paradigms in Multi-Objective Shortest Path (MOSP) search: **Implicit Differential Heuristics** and **Problem Decomposition via Landmark Cut-Sets**. These concepts natively extend the **FAST-MVH** framework (Algorithms 1–3) and provide rigorous resolutions to the dimensionality and depth bottlenecks of Multi-Valued Heuristic (MVH) search.

> [!IMPORTANT]
> **Audit Status (October 2026)**: Conducted a comprehensive, skeptical empirical audit across five dedicated micro-experiments (`1A`, `1B`, `1C`, `2A`, `2B`) in `experiments/diff_heuristics_and_cutsets/`. All theoretical claims, quantifier semantics, data structure choices, and decomposition architectures documented below have been mathematically validated and empirically verified to preserve **100% bit-identical Pareto fronts** (0 lost solutions).

---

## PART I: Implicit Differential Heuristics in Multi-Objective Search

### 1. Theoretical Grounding & Quantifier Semantics
In single-objective search, a landmark $L$ provides the differential bound $h(u, t) = \max(0, \operatorname{dist}(u, L) - \operatorname{dist}(t, L))$ via the triangle inequality. Extending this principle to MOSP (as initiated by Han Zhang et al., AAAI/SoCS) requires reasoning over Pareto-optimal path sets $G_{u \to L}$ and $G_{t \to L}$.

#### The Exact Differential Bound
For any actual path $\pi$ from $u$ to $t$ with cost $c(u \to t)$, concatenating $\pi$ with **any** valid path from $t$ to $L$ with cost $g_{tL} \in G_{t \to L}$ yields a valid path from $u$ to $L$. Consequently, $c(u \to t) + g_{tL}$ must be weakly dominated by at least one Pareto-optimal path $g_{uL} \in G_{u \to L}$:
$$\forall g_{tL} \in G_{t \to L}, \quad \exists g_{uL} \in G_{u \to L} \quad \text{s.t.} \quad g_{uL} \preceq c(u \to t) + g_{tL}$$
Rearranging terms, the actual cost $c(u \to t)$ is constrained to lie in the intersection:
$$c(u \to t) \in \bigcap_{g_{tL} \in G_{t \to L}} \bigcup_{g_{uL} \in G_{u \to L}} \left\{ c \in \mathbb{R}_{\ge 0}^M \;\middle|\; c \ge \max(0, g_{uL} - g_{tL}) \right\}$$

#### Mathematical Clarification: Quantifiers and Pruning Sufficiency
*   **The Outer Quantifier is Universal ($\forall g_{tL}$)**: An admissible lower bound set $H(u; g_{tL})$ is formed by fixing **any single realizable path** $g_{tL} \in G_{t \to L}$:
    $$H(u; g_{tL}) = \left\{ \max(0, g_{uL} - g_{tL}) \;\middle|\; g_{uL} \in G_{u \to L} \right\}$$
    Because every actual path $\pi$ satisfies $c(\pi) \ge \max(0, g_{uL} - g_{tL})$ for some $g_{uL} \in G_{u \to L}$, the set $H(u; g_{tL})$ is a **100% admissible Multi-Valued Heuristic**.
*   **Sufficient Pruning Condition**: If there exists *even one* $g_{tL} \in G_{t \to L}$ for which all candidates $h \in H(u; g_{tL})$ are dominated by the goal frontier $T$ (i.e., $\forall g_{uL} \in G_{u \to L}, g(u) + \max(0, g_{uL} - g_{tL}) \in \operatorname{Dom}(T)$), then **node $u$ can be safely pruned**.
*   **Falsification of "Pairwise Early Acceptance" (Audit Finding 1A)**: Early acceptance based on a single pair $(N_u, N_t)$ having an undominated upper bound is unsound. Empirical testing (`exp_1a_quantifier.py`) revealed that pairwise early acceptance aborts prematurely, missing **14.29% of doomed nodes** that the universal quantifier correctly prunes.

---

### 2. Sign Asymmetry & Realizable Dijkstra Paths (Bypassing $G_{t \to L}$ MOSP)
A foundational dilemma of differential heuristics in high dimensions ($M \ge 4$) is that computing the full Pareto front $G_{t \to L}$ offline suffers from the same combinatorial explosion as offline APEX.

#### The Sign Asymmetry Principle (Audit Finding 1B)
In the term $\max(0, g_{uL} - g_{tL})$:
1.  **Positive Sign ($+g_{uL}$)**: $G_{u \to L}$ acts as a lower bound. Approximating or compressing $G_{u \to L}$ (e.g., via $\epsilon$-dominance or component-wise minima $g_{uL}^{lb} \le g_{uL}$) guarantees $\max(0, g_{uL}^{lb} - g_{tL}) \le \max(0, g_{uL} - g_{tL})$, **preserving admissibility**.
2.  **Negative Sign ($-g_{tL}$)**: Lower-bounding $g_{tL}$ (e.g., substituting the ideal point $g_{tL}^{ideal} \le g_{tL}$) flips the inequality:
    $$-g_{tL}^{ideal} \ge -g_{tL}^{real} \implies \max(0, g_{uL} - g_{tL}^{ideal}) \ge \max(0, g_{uL} - g_{tL}^{real})$$
    Subtracting a lower bound **overestimates** the heuristic, destroying admissibility! In empirical verification (`exp_1b_sign_asymmetry.py`), ideal-point relaxation of $g_{tL}$ lost **10 to 11 Pareto-optimal solutions** (up to 42% solution loss).
3.  **The Scalarized Dijkstra Resolution**: Conversely, any *realizable* path cost from $t$ to $L$ is a valid upper bound. We completely eliminate the offline MOSP precomputation for $G_{t \to L}$ by running $K \in \{M, 2M\}$ fast scalarized Dijkstra searches at query time (taking $< 1$ ms):
    $$\mathcal{P}_{real}(t \to L) = \left\{ \operatorname{Dijkstra}(t \to L; w^{(k)}) \;\middle|\; k = 1, \dots, K \right\}$$
    Empirical tests confirmed that $K=M$ scalarized paths achieve **100% bit-identical Pareto fronts** while matching the exact pruning power of the full MOSP front $G_{t \to L}$.

---

### 3. Data Structure Architecture: Flat Arrays with Redundancy Pointers Beat Dual K-d Trees
Section 5.3 of the FAST-MVH paper proved that static K-d trees over heuristic sets lose to flat arrays due to *Bounding Box Indecision* and *Oracle Cost Asymmetry*. In differential heuristics, this penalty is compounded: subtracting two bounding boxes $[LO(N_u) - HI(N_t), HI(N_u) - LO(N_t)]$ doubles the interval uncertainty.

#### Monotonicity of Redundancy Pointers under Dynamic Shifts
Let $\rho_u(i) = \max\{j < i \mid \operatorname{Tr}(g_{uL}^j) \preceq \operatorname{Tr}(g_{uL}^i)\}$ be FAST-MVH's offline precomputed redundancy pointers on $G_{u \to L}$ (Algorithm 3).
*   **Theorem (Shift Invariance)**: For any non-negative vector $g_{tL} \in \mathbb{R}_{\ge 0}^M$, the coordinate-wise scalar mapping $\phi_d(x) = \max(0, x - (g_{tL})_d)$ is monotonically non-decreasing. Therefore:
    $$\operatorname{Tr}(g_{uL}^j) \preceq \operatorname{Tr}(g_{uL}^i) \implies \operatorname{Tr}(\max(0, g_{uL}^j - g_{tL})) \preceq \operatorname{Tr}(\max(0, g_{uL}^i - g_{tL}))$$
*   **Result**: The offline precomputed redundancy pointers $\rho_u(i)$ remain **100% mathematically valid for any dynamically shifted vector $g_{tL}$ at runtime**, requiring zero recomputation!

#### Empirical Benchmark (Audit Finding 1C)
We implemented and benchmarked both architectures in C++20 (`-O3 -DNDEBUG`) across $M \in \{3, 4, 5, 6\}$ (`exp_1c_dual_kdt_vs_flat.cpp`):

| Dimension ($M$) | Dual K-d Tree Time | Flat Array + $\rho_u$ Time | Speedup (Flat vs KDT) | KDT `GoalDom` Calls | Flat `GoalDom` Calls | Flat Witness Hits | Flat $\rho$-Skips |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **$M = 3$** | 0.0237 ms | 0.0174 ms | **1.36x Faster** | 311 | 53 | 535 | 11,834 |
| **$M = 4$** | 0.1055 ms | 0.0461 ms | **2.29x Faster** | 873 | 138 | 4,988 | 5,359 |
| **$M = 5$** | 0.1415 ms | 0.0832 ms | **1.70x Faster** | 713 | 200 | 9,411 | 1,374 |
| **$M = 6$** | 0.1970 ms | 0.1414 ms | **1.39x Faster** | 1,036 | 361 | 7,888 | 1,223 |

**Conclusion**: Flat contiguous storage with precomputed redundancy pointers $\rho_u(i)$ and goal-witness caching decisively outperforms Dual K-d Trees, reducing `GoalDom` calls by $3.5\text{--}6.3\times$ and scalar comparisons by $3.3\text{--}6.1\times$.

---

## PART II: Cut-Set Problem Decomposition via FAST-MVH

### 1. The Dimensionality-Reduction Invariant Breakdown in Eager Stitching
In monolithic search, search depth triggers an exponential explosion of intermediate non-dominated paths $|F(v)|$. Partitioning the graph via a topological cut-set $\mathcal{L} = \{L_1, \dots, L_k\}$ decouples the search into two sequential stages:
1.  **Backward Stage ($L_i \to \text{Goal}$)**: Solved first, yielding exact Pareto frontiers $H^*(L_i) = F(L_i \to \text{Goal})$.
2.  **Forward Stage ($\text{Start} \to L_i$)**: Solved second, using cut-set nodes as boundary interfaces.

#### The Soundness Flaw in Naive Eager Stitching (FAST3 Pillar 3 Post-Mortem)
When a forward path $g(\text{Start} \to L_i)$ reaches a cut node, naive eager stitching immediately inserts all composite paths $f = g \oplus H^*(L_i)$ into the goal frontier $T$.
*   **The Invariant Violation**: FAST-MVH's dimensionality-reduced goal test $\mathrm{GoalDom}(f)$ (Algorithm 1, line 8) evaluates only $\operatorname{Tr}(p) \preceq \operatorname{Tr}(f)$. This test assumes $p_1 \le f_1$, which is guaranteed in monolithic search by $\mathrm{OPEN}$'s lexicographic $f_1$ extraction order.
*   **The Catastrophic Consequence**: Eagerly stitched paths have large $f_1$ values. Inserting them out-of-order into $T$ poisons the truncated goal frontier, falsely refuting valid paths in $\mathrm{OPEN}$ that have smaller $f_1$ but larger $\operatorname{Tr}(f)$. In historical evaluations, this bug caused the loss of 1,961 optimal solutions. In our micro-experiments (`exp_2a_cutset_architectures.py`), naive eager stitching collapsed completely, losing **100% of optimal solutions**.

---

### 2. Resolution: Two Sound Decomposition Architectures

We evaluated two competing architectures derived from FAST-MVH principles:

```mermaid
flowchart TD
    subgraph Arch1 ["Architecture 1: Lazy Terminal Sinks (Conservative)"]
        A1[Path arrives at cut node L_i] --> B1["Bind lazily to h^1(L_i)"]
        B1 --> C1["Insert (L_i, g, j=1) into OPEN"]
        C1 --> D1["Extract from OPEN in strict f_1 order"]
        D1 --> E1["Insert into T & Reinsert (L_i, g, j+1)"]
    end

    subgraph Arch2 ["Architecture 2: Witness-Checked T + Residue R(goal) (Aggressive)"]
        A2[Path arrives at cut node L_i] --> B2["Eagerly stitch all f = g + H*(L_i)"]
        B2 --> C2["Algorithm 1 Update on T: Evictions -> R(goal)"]
        C2 --> D2["GoalDom checks Full Witness (p_1 <= f_1)"]
        D2 --> E2["Fallback to exact R(goal) query if Tr-dominated"]
    end
```

#### Architecture 1 (Lazy Terminal Sinks)
*   Treats each cut node $L_i \in \mathcal{L}$ as an absorbing terminal sink in the forward search (no outgoing edges).
*   When a path $g$ reaches $L_i$, it is bound lazily via `ChooseH` to $h^1 \in H^*(L_i)$ and pushed into $\mathrm{OPEN}$ as a pseudo-node $\langle L_i, g, j=1 \rangle$.
*   Because extractions from $\mathrm{OPEN}$ occur in non-decreasing $f_1$ order, vectors enter $T$ in strict $f_1$ monotonicity. The standard $O(M-1)$ truncated test $\mathrm{GoalDom}(f)$ remains 100% sound with zero modifications.

#### Architecture 2 (Eager Stitching via Full Witness & Residue Forest $R(\text{goal})$)
*   Stitched vectors $f = g \oplus H^*(L_i)$ are eagerly injected into $T$ to maximize early pruning of competing paths.
*   To prevent out-of-order poisoning, $T$ is managed via FAST-MVH's exact `LocalDom` and `Update` pipeline (Algorithm 1):
    1.  **Full Witness Check**: A truncated dominator $p \in F(\text{goal})$ refutes $f$ if and only if $p_1 \le f_1$.
    2.  **Residue Preservation**: Any vector evicted from $F(\text{goal})$ with $p_1 < f_1$ is retained in the Bentley-Saxe residue forest $R(\text{goal})$.
    3.  **Exact Fallback**: If $\operatorname{Tr}(p) \preceq \operatorname{Tr}(f)$ but $p_1 > f_1$, the query falls back to $R(\text{goal})$ in full $M$ dimensions.

#### Empirical Comparison (Audit Finding 2A)
Evaluated on bottleneck DAG networks with negative edge correlation across $M \in \{3, 4\}$ (`exp_2a_cutset_architectures.py`):

| Instance ($M$, Seed) | Oracle Sols | Flawed Pillar 3 | Architecture 1 Time | Architecture 2 Time | Speedup (Arch 2 vs 1) | Arch 2 $R(\text{goal})$ Size |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **$M=3$, Seed 42** | 43 | 0 (Lost 43) | 26.95 ms | **10.00 ms** | **2.70x Faster** | 59 |
| **$M=3$, Seed 101** | 42 | 0 (Lost 42) | 24.61 ms | **8.20 ms** | **3.00x Faster** | 53 |
| **$M=3$, Seed 777** | 34 | 0 (Lost 34) | 15.51 ms | **8.76 ms** | **1.77x Faster** | 48 |
| **$M=4$, Seed 42** | 50 | 0 (Lost 50) | 41.87 ms | **17.03 ms** | **2.46x Faster** | 47 |
| **$M=4$, Seed 101** | 62 | 0 (Lost 62) | 67.35 ms | **19.98 ms** | **3.37x Faster** | 48 |
| **$M=4$, Seed 999** | 47 | 0 (Lost 47) | 37.35 ms | **12.45 ms** | **3.00x Faster** | 56 |

**Key Takeaways**:
1.  Both Architecture 1 and Architecture 2 achieved **100% bit-identical Pareto front recovery** across all seeds and dimensions.
2.  **Architecture 2 is 2.5x to 3.4x faster** than Architecture 1. Immediate eager injection provides an ultra-tight upper bound that aggressively prunes intermediate paths early, while the full witness check completely immunizes $T$ against false dominance.

---

### 3. Intermediate Search Guidance Across Multi-Node Cut-Sets ($|\mathcal{L}| = k > 1$)
When the cut-set contains multiple separator nodes $\mathcal{L} = \{L_1, \dots, L_k\}$, intermediate nodes $u \notin \mathcal{L}$ must be guided toward the separator without triggering combinatorial explosion.

#### The Compound Guidance Dilemma
*   Guiding intermediate nodes toward a single landmark $L_1$ is inadmissible: optimal paths routing through $L_j \neq L_1$ will be underestimated or misdirected.
*   Evaluating the full Minkowski sum $h_{base}(u \to L_i) \oplus H^*(L_i)$ across all cut nodes explodes the number of heuristic vectors, overwhelming `ChooseH`.

#### The Compact Admissible Union Bound (Audit Finding 2B)
For any intermediate node $u \notin \mathcal{L}$, let $h_{id}(u \to L_i)$ and $h_{id}(L_i \to \text{Goal})$ be the ideal-point (single-objective shortest path) distance vectors. Because any path from $\text{Start}$ to $\text{Goal}$ must traverse at least one separator $L_i \in \mathcal{L}$, the component-wise sum $v_i = h_{id}(u \to L_i) + h_{id}(L_i \to \text{Goal})$ is a valid lower bound for paths passing through $L_i$.

We define the **Compact Admissible Union Bound** as:
$$H_{guide}(u) = \operatorname{ParetoMax}_{L_i \in \mathcal{L}} \left\{ h_{id}(u \to L_i) + h_{id}(L_i \to \text{Goal}) \right\}$$

#### Theoretical Invariants
1.  **Admissibility**: Every actual solution cost $c(u \to \text{Goal})$ satisfies $c \ge v_i$ for the specific separator $L_i$ it traverses. Therefore, $H_{guide}(u)$ is an admissible MVH.
2.  **Strict Cardinality Bound**: $|H_{guide}(u)| \le k = |\mathcal{L}|$. For narrow cut-sets ($k \le 4$), `ChooseH` evaluates at most $k$ vectors, bounding heuristic evaluation to $O(k)$ operations.

#### Empirical Validation (`exp_2b_intermediate_guidance.py`)
Tested across $k \in \{1, 2, 3, 4\}$ separator nodes on layered DAGs with specialized trade-off costs:
*   **Soundness**: Returned **100% bit-identical Pareto fronts** (0 lost solutions) across all seeds and cut-set sizes.
*   **Efficiency**: Kept intermediate `ChooseH` calls strictly bounded ($O(k)$ per successor), providing directed search guidance toward the cut-set without the combinatorial overhead of full cross-sum materialization.

---

## Final Synthesis: Recommended System Architecture

```mermaid
graph LR
    subgraph Offline ["Offline Preprocessing"]
        P1["Select Landmarks / Cut-Set L"]
        P2["Compute G_{u -> L} & Precompute rho_u(i)"]
        P3["Compute Backward H*(L_i -> Goal)"]
    end

    subgraph Query ["Query Phase (FAST-MVH Engine)"]
        Q1["Extract K=M Realizable Dijkstra Paths g_{tL}"]
        Q2["Flat Contiguous Scan with rho_u & Witness Caching"]
        Q3["Eager Stitching at Cut-Sets into T"]
        Q4["Algorithm 1 Full Witness + R(goal) Protection"]
    end

    Offline --> Query
```

1.  **Implicit Differential Heuristics**:
    *   Store $G_{u \to L}$ as flat contiguous arrays with offline precomputed redundancy pointers $\rho_u(i)$ and goal-witness caching. Discard dual-tree bounding structures.
    *   Extract $K=M$ realizable Dijkstra paths $g_{tL}$ at query time. Never approximate $g_{tL}$ with lower bounds.
2.  **Cut-Set Problem Decomposition**:
    *   Deploy **Architecture 2** (Eager Stitching with Witness-Checked $T$ and Goal Residue $R(\text{goal})$).
    *   Guide intermediate nodes using the Compact Admissible Union Bound $H_{guide}(u)$ of cardinality $\le k$.
    *   Reserve decomposition for narrow topological bottlenecks ($k \le 3$), avoiding wide grid cuts where boundary maintenance exceeds monolithic search costs.
