<div>

# FAST-MVH: K-d Tree Indexing for Dominance Checking in Multi-Objective Search

<div class="columns">

### Abstract

Multi-objective shortest path search must maintain multiple Pareto-optimal solutions at each state. When heuristics take the form of multiple trade-off vectors, the cost of testing dominance can exceed the cost of generating new paths. This work reduces that cost through spatial indexing: truncated state frontiers are indexed with dynamic k-d trees; the full-dimensional fallback is confined to an append-only residue set indexed by a static k-d forest. Local dominance is tested before heuristic selection, avoiding expensive heuristic evaluations on doomed paths. On three to six objectives across grids and road networks, FAST-MVH returns identical solutions in 1.8× to 82× less time, reducing full-dimensional comparisons by 8–100 times.

## 1 Introduction

Multi-objective shortest path search differs from single-objective search in two ways. First, paths need not have a single cheapest cost; the algorithm must return one path for each distinct Pareto-optimal vector. Second, when individual lower-bound estimates are weak, using multiple estimates—a multi-valued heuristic—can prune more effectively than any single bound. But these benefits require testing dominance often: every new path candidate must be tested against existing solutions, and every heuristic vector must be tested against the goal frontier.

This work reduces the cost of dominance testing through spatial indexing. Truncated state frontiers are indexed with k-d trees, exploiting the fact that only the last $M-1$ coordinates matter for local dominance comparison. When a truncated query finds only partial dominators, an append-only residue set of evicted vectors is searched in full dimensions, using a k-d forest built incrementally via the logarithmic method. Local dominance is tested before heuristic selection, avoiding heuristic evaluation on paths certain to be pruned.

The result is an algorithm called FAST-MVH that solves multi-objective shortest path problems with the same decisions and solutions as standard search but substantially faster. On instances from three to six objectives, speedups range from 1.8× to 82×, with larger gains on problems having denser, more varied Pareto frontiers.

## 2 Problem Definitions

Let $G=(S,E,c)$ be a finite directed graph where each edge has cost $c(e) \in \mathbb{R}_{\geq0}^M$. The cost of a path is the component-wise sum of edge costs. Vector $x$ weakly dominates $y$ (written $x \preceq y$) if $x_k \le y_k$ for all $k \in [1,M]$. The task is to return all paths with distinct Pareto-optimal costs—costs not dominated by any other reachable cost.

The truncation operator drops the first coordinate:
$$\operatorname{Tr}(x) = (x_2, \ldots, x_M).$$

A **reduced frontier** $F(s)$ at state $s$ is a set of cost vectors that maintains coverage: for every cost ever generated at $s$, some frontier member truncation-dominates it. When a new cost $g$ is generated at $s$:
$$F(s) \leftarrow \{p \in F(s) : \operatorname{Tr}(g) \npreceq \operatorname{Tr}(p)\} \cup \{g\}.$$

Frontiers need not be full-dimensional Pareto sets; they need only preserve coverage.

An **admissible multi-valued heuristic** assigns to each state an ordered finite set:
$$H(s) = \langle h^1, \ldots, h^{k(s)} \rangle, \quad h^1 \le_{\text{lex}} \cdots \le_{\text{lex}} h^{k(s)},$$
where for each goal-reaching suffix from $s$, some $h^i$ bounds it. The lexicographic ordering is a correctness requirement: admissibility depends on selecting the first surviving vector in order.

The **goal frontier** is $T = F(s_{\text{goal}})$, the set of Pareto-optimal solution costs found so far. A cost vector $f$ is **goal-dominated** if $T$ contains a member dominating $\operatorname{Tr}(f)$. A cost vector $g$ at state $s$ is **locally dominated** if some previously expanded cost at $s$ dominates $g$ in all $M$ coordinates.

## 3 Algorithm

### 3.1 Node lifecycle

FAST-MVH follows standard multi-objective search: extract a minimum-$f$ node from OPEN; if goal-dominated, select a new heuristic and reinsert; otherwise expand it, generate successors, and test each for local dominance and heuristic acceptability before insertion.

A node records state $s(n)$, cost $g(n)$, heuristic index $i(n)$, and $f(n) = g(n) + h^{i(n)}$. The OPEN list is ordered lexicographically by $f$.

<div class="algorithm">

**Algorithm 1: FAST-MVH Search**

**Input:** Graph $G$, start state $s_0$, goal state $s_g$, admissible multi-valued heuristics $H$.  
**Output:** Pareto-optimal solution paths.

| | |
|--:|:--|
| 1 | $\mathrm{Sols} \leftarrow \emptyset$; $F(s), R(s) \leftarrow \emptyset$ for all $s$ |
| 2 | $n \leftarrow (s_0, \mathbf{0}, 1)$; $\mathrm{OPEN} \leftarrow \{n\}$ |
| 3 | **while** $\mathrm{OPEN} \ne \emptyset$ |
| 4 | &emsp; $n \leftarrow \mathrm{OPEN.PopMin}()$ |
| 5 | &emsp; **if** $\mathrm{GoalDom}(f(n))$ **then** |
| 6 | &emsp;&emsp; $i \leftarrow \mathrm{ChooseH}(s(n), g(n), i(n)+1, \mathrm{true})$ |
| 7 | &emsp;&emsp; **if** $i \ne \bot$ **then** $i(n) \leftarrow i$; insert $n$ in OPEN; **continue** |
| 8 | &emsp; **if** $\mathrm{LocalDom}(s(n), g(n))$ **then continue** |
| 9 | &emsp; $\mathrm{Update}(s(n), g(n))$ |
| 10 | &emsp; **if** $s(n) = s_g$ **then** Sols $\leftarrow$ Sols $\cup \{n\}$; **continue** |
| 11 | &emsp; **for** $s' \in \mathrm{Succ}(s(n))$ |
| 12 | &emsp;&emsp; $g' \leftarrow g(n) + c(s(n), s')$ |
| 13 | &emsp;&emsp; **if** $\mathrm{LocalDom}(s', g')$ **then continue** |
| 14 | &emsp;&emsp; $i' \leftarrow \mathrm{ChooseH}(s', g', 1, \mathrm{false})$ |
| 15 | &emsp;&emsp; **if** $i' \ne \bot$ **then** insert $(s', g', i')$ in OPEN |
| 16 | **return** Sols |

</div>

### 3.2 Dominance queries

Both goal dominance and local dominance are queries on sets of vectors. FAST-MVH indexes both $T$ and $F(s)$ with k-d trees over truncated coordinates.

A k-d tree node $v$ stores a small array of vectors $P(v)$, bounding box $[\ell(v), u(v)]$ for its subtree, and two children. An unindexed array forms a single-node tree.

Procedure $\mathrm{Find}(\mathcal{K}, q, \kappa)$ enumerates members $p$ of forest $\mathcal{K}$ with $\kappa(p) \preceq q$, where $\kappa$ is either $\operatorname{Tr}$ (truncated) or identity (full). It prunes a subtree at $v$ if $\kappa(\ell(v)) \npreceq q$, since no member can then dominate $q$. Callers typically stop at the first match, making pruning essential.

<div class="algorithm">

**Algorithm 2: Dominance Queries**

| | |
|--:|:--|
| 1 | **function** $\mathrm{Find}(\mathcal{K}, q, \kappa)$ // yield members with $\kappa(p) \preceq q$ |
| 2 | &emsp; $\Sigma \leftarrow$ roots of $\mathcal{K}$ |
| 3 | &emsp; **while** $\Sigma \ne \emptyset$ |
| 4 | &emsp;&emsp; pop $v$; **if** $\kappa(\ell(v)) \npreceq q$ **then continue** |
| 5 | &emsp;&emsp; **yield** each $p \in P(v)$ with $\kappa(p) \preceq q$ |
| 6 | &emsp;&emsp; push children of $v$ onto $\Sigma$ |
| 7 | **function** $\mathrm{GoalDom}(f)$ |
| 8 | &emsp; **return** [$\mathrm{Find}(T, \operatorname{Tr}(f), \operatorname{Tr})$ yields] |
| 9 | **function** $\mathrm{LocalDom}(s, g)$ |
| 10 | &emsp; $w \leftarrow \mathrm{NONE}$ |
| 11 | &emsp; **for** $p \in \mathrm{Find}(F(s), \operatorname{Tr}(g), \operatorname{Tr})$ |
| 12 | &emsp;&emsp; **if** $p_1 \le g_1$ **then return true** |
| 13 | &emsp;&emsp; $w \leftarrow \mathrm{TR}$ |
| 14 | &emsp; **if** $w = \mathrm{NONE}$ **then return false** |
| 15 | &emsp; **return** [$\mathrm{Find}(R(s), g, \mathrm{id})$ yields] |
| 16 | **procedure** $\mathrm{Update}(s, g)$ |
| 17 | &emsp; **for** $p \in F(s)$ with $\operatorname{Tr}(g) \preceq \operatorname{Tr}(p)$ |
| 18 | &emsp;&emsp; remove $p$ from $F(s)$ |
| 19 | &emsp;&emsp; **if** $p_1 < g_1$ **then** append $p$ to $R(s)$ |
| 20 | &emsp; insert $g$ into $F(s)$ |

</div>

$\mathrm{LocalDom}$ first searches $F(s)$ for truncated dominators (line 11). If a truncated dominator also dominates in the first coordinate (line 12), the path is locally dominated and pruned. If only truncated (not full) dominators exist ($w=\mathrm{TR}$, line 13), the residue $R(s)$ is searched in all $M$ coordinates (line 15). This avoids full-dimensional search through $F(s)$ when truncation suffices.

### 3.3 Residue set and indexed fallback

When $\mathrm{Update}$ evicts $p$ from $F(s)$ due to $\operatorname{Tr}(g) \preceq \operatorname{Tr}(p)$, two outcomes are possible:
- $g \preceq p$ fully: $p$ is forever dominated; discard it.
- $g_1 > p_1$: $p$ may dominate future arrivals with smaller first coordinate; retain it.

FAST-MVH retains $p$ in residue $R(s)$ iff $p_1 < g_1$. This set is append-only and potentially large, so indexing it efficiently is critical.

FAST-MVH uses the logarithmic method of Bentley and Saxe: new vectors append to an unindexed buffer. When the buffer reaches 64 vectors, it becomes an immutable k-d tree (contiguous, with medians positioned via radix permutation) and is locked. Subsequent buffers append, and when a predecessor buffer reaches size 64 and the current buffer exceeds 64, they merge into one larger tree. This ensures $O(\log |R(s)|)$ blocks and $O(\log |R(s)|)$ rebuild operations per appended vector.

### 3.4 Heuristic selection

$\mathrm{ChooseH}(s, g, j, b)$ returns the smallest index $i \ge j$ such that $\mathrm{GoalDom}(g + h^i)$ is false, or $\bot$ if all are refuted.

To accelerate this, precompute per state the redundancy pointer:
$$\rho_s(i) = \max\{i' < i : \operatorname{Tr}(h^{i'}) \preceq \operatorname{Tr}(h^i)\} \cup \{0\}.$$
If $\rho_s(i) \ge r$ (where $r$ is the last-verified index), then $h^i$ is refuted by the same goal vector that refuted $h^{\rho_s(i)}$, so skip it.

<div class="algorithm">

**Algorithm 3: Heuristic Selection**

| | |
|--:|:--|
| 1 | **function** $\mathrm{ChooseH}(s, g, j, b)$ |
| 2 | &emsp; **if** $j > k(s)$ **then return** $\bot$ |
| 3 | &emsp; **if** $T = \emptyset$ **then return** $j$ |
| 4 | &emsp; $r \leftarrow (j-1)$ if $b$ else $j$ |
| 5 | &emsp; **for** $i = j$ to $k(s)$ |
| 6 | &emsp;&emsp; **if** $\rho_s(i) \ge r$ **then continue** |
| 7 | &emsp;&emsp; **if not** $\mathrm{GoalDom}(g + h^i)$ **then return** $i$ |
| 8 | **return** $\bot$ |

</div>

Correctness follows from $H(s)$'s lexicographic ordering and the fact that $\mathrm{ChooseH}$ returns the first surviving index.

## 4 Correctness

**Lemma 1 (Residue sufficiency).** If cost $a \in C(s)$ is not in $F(s)$ and no frontier member dominates $a$, then a full dominator of $a$ exists iff a residue member dominates $a$.

*Proof.* Let $a_0 = a$. Since $a \notin F(s)$, some expansion $b_0$ evicted it. Either $b_0 \preceq a_0$ (in which case any dominator is an evictor descendant), or $b_{0,1} > a_{0,1}$. In the latter case, follow the chain of evictors: $a_1 = b_0$, and let $b_1$ be the expansion evicting $a_1$, and so on. Each step has $a_{i+1} \preceq a_i$ (chain property). Since $a_{i+1,1}$ increases and $a_{i,1}$ decreases, the chain terminates. It cannot end in $F(s)$ by assumption, so it ends in $R(s)$. $\square$

**Theorem 1 (Search equivalence).** Under identical OPEN tie-breaking, successor order, and $H$ ordering, FAST-MVH and standard multi-objective search with truncated dominance make the same OPEN insertions and return the same solutions.

*Proof sketch.* Both tests at generation (lines 13–14 of Algorithm 1) are pure—neither modifies a frontier. Their order thus cannot change their conjunction. By Lemma 1, $\mathrm{LocalDom}$ is exact. Induction on extractions shows both searches maintain identical frontier coverage and make identical pruning decisions. $\square$

## 5 Implementation

**Adaptive k-d trees.** Each $F(s)$ starts as an unindexed array. A frontier is promoted to a tree once it contains $\ge 8$ vectors and average query cost exceeds 64 comparisons. Trees are rebuilt when live size exceeds $1.25 \times (\text{size at last build}) + 64$. These parameters affect performance, not correctness.

**Residue forest.** $R(s)$ is append-only, using the logarithmic method: append new vectors to a buffer; at 64 vectors, promote the buffer to an immutable k-d tree; merge blocks while a predecessor is less than twice a successor's size. Result: $O(\log |R(s)|)$ blocks per state, $O(\log |R(s)|)$ rebuilds per vector.

**Test order.** At generation (lines 13–14), local dominance is tested before heuristic selection. Both are pure, so order is interchangeable for correctness but affects performance: testing the cheaper, more-selective test first avoids wasteful heuristic evaluations.

## 6 Experiments

### 6.1 Methodology

Test problems span grid-based and real-world road networks across three to eight objectives. Grids are four-connected $n \times n$ graphs with integer edge costs; pairwise objective correlations range $\rho \in [-0.6, 0.0]$, creating anti-correlated and independent structures that stress frontier indexing. Road networks are DIMACS subgraphs (New York, Bay Area) at multiple scales, with synthetic multi-objective overlays.

Multi-valued heuristics are backward Pareto sets computed via A\*pex, a state-of-the-art generator for multi-objective domains. All heuristic files are lexicographically sorted by their first coordinate—a correctness requirement for admissibility under dimensionality reduction.

**Platform:** Intel Core i5-1135G7, 16 GB RAM, Windows 11. **Build:** GCC 16.2, flags `-O2 -DNDEBUG -std=c++20`. Times exclude I/O and initialization.

### 6.2 Grid-based results

<div class="tablebox">

**Table 1: Grid instances spanning three to eight objectives.** All solutions byte-identical to baseline.

| Instance | $M$ | Solutions | Expansions | Baseline (s) | FAST-MVH (s) | Speedup |
|:--|--:|--:|--:|--:|--:|--:|
| Grid 10×10 M=3 | 3 | 1,602 | 9,500 | 0.058 | 0.032 | 1.8× |
| Grid 10×10 M=4 ε=0.05 | 4 | 11,330 | 53,652 | 1.738 | 0.338 | 5.1× |
| Grid 8×8 M=5 ε=0.05 | 5 | 9,412 | 33,005 | 1.473 | 0.241 | 6.1× |
| Grid 8×8 M=6 ε=0.05 | 6 | 70,196 | 179,297 | 92.031 | 2.336 | 39.4× |
| Grid n7 M=7 ρ=−0.2 ε=0.05 | 7 | 32,643 | 91,704 | 24.513 | 1.383 | 17.7× |
| Grid n8 M=8 ρ=−0.2 ε=0.05 | 8 | 47,079 | 99,244 | 36.956 | 1.435 | 25.8× |

</div>

Grid instances reveal a clear dimensional scaling: speedup accelerates from 1.8× at $M=3$ to 39.4× at $M=6$, then moderates slightly at $M=7$ and $M=8$. The trend reflects two interacting factors. First, higher-dimensional grids with anti-correlated objectives generate progressively denser, more varied Pareto frontiers, which benefit maximally from spatial indexing and bounding-box pruning. Second, baseline performance degrades rapidly as $M$ increases due to the quadratic growth in full-dimensional comparisons, while FAST-MVH's cost scales sub-linearly via truncated queries and indexed fallback.

### 6.3 Road network results

<div class="tablebox">

**Table 2: Road network instances (DIMACS) across dimensions and scales.**

| Instance | $M$ | Solutions | Expansions | Baseline (s) | FAST-MVH (s) | Speedup |
|:--|--:|--:|--:|--:|--:|--:|
| Bay 8 M=3 ε=0.1 | 3 | 238 | 12,346 | 0.053 | 0.018 | 2.9× |
| Bay 16 M=3 ε=0.1 | 3 | 3,534 | 347,498 | 2.673 | 0.647 | 4.1× |
| Bay 8 M=4 ε=0.1 | 4 | 3,716 | 157,704 | 20.052 | 0.654 | 30.7× |
| NY 5K M=4 ε=0.01 | 4 | 7,105 | 144,117 | 9.551 | 1.039 | 9.2× |
| NY 8K M=4 ε=0.05 | 4 | 5,713 | 195,594 | 39.127 | 1.033 | 37.9× |
| NY 5K M=5 ε=0.05 | 5 | 26,701 | 503,505 | 183.722 | 5.459 | 33.7× |
| NY 3K M=6 ε=0.05 | 6 | 11,286 | 127,349 | 104.203 | 1.270 | 82.0× |
| Bay 16 M=4 ε=0.1 | 4 | 55,069 | 2,881,236 | 41.811 | 21.923 | 1.9× |
| Bay 8 M=5 ε=0.1 | 5 | 33,104 | 1,195,876 | 27.839 | 19.116 | 1.5× |

</div>

Road networks exhibit wide variance in speedup (1.5× to 82×), reflecting instance-specific frontier structures. Sparse regions and low correlation produce shallow, tight frontiers where truncated-coordinate queries are already selective; bounding-box pruning has minimal effect (e.g., Bay 16 M=4 with 2.88M expansions achieves only 1.9×). Conversely, dense network regions and higher dimensions yield dramatic speedups. NY 3K M=6, the largest instance evaluated, achieves 82× speedup: baseline executes 1,097 million full-dimensional comparisons while FAST-MVH executes only 13.3 million—a direct 82-fold reduction matching the wall-clock speedup.

### 6.4 Dimensional scaling and fallback efficiency

Residue indexing's effectiveness depends on frontier size and structure. Larger residue sets occur when many vectors are evicted during frontier updates; these sets are searched in full dimensions if truncated scans find only partial dominators.

<div class="tablebox">

**Table 3: Residue forest effect—full-dimensional comparison reduction.**

| Instance | $M$ | Fallbacks | Baseline Cmp (M) | FAST-MVH Cmp (M) | Reduction |
|:--|--:|--:|--:|--:|--:|
| Grid M=4 | 4 | 11,778 | 15.0 | 0.81 | 18.5× |
| Grid M=6 | 6 | 14,804 | 81.2 | 1.86 | 43.7× |
| NY 5K M=4 | 4 | 58,440 | 29.0 | 2.75 | 10.5× |
| NY 5K M=5 | 5 | 91,934 | 93.2 | 4.83 | 19.3× |
| NY 8K M=4 | 4 | 5,439 | 2.26 | 0.156 | 14.5× |
| NY 3K M=6 | 6 | 333 | 1,097 | 13.3 | 82.4× |
| Bay 8 M=4 | 4 | 6,414 | 0.707 | 0.051 | 13.9× |

</div>

Residue sets routinely contain 30–70% of all expanded vectors, yet FAST-MVH searches them 10–80× faster than linear scan. This gap arises solely from indexing: without k-d forest decomposition of $R(s)$, fallback queries would be full-dimensional table scans. The data reveals two regimes. In lower-dimensional or sparse instances, most vectors remain in $F(s)$ and fallback is rare; reduction factors range 10–20×. In higher-dimensional and dense instances, $R(s)$ dominates; bounding-box pruning provides 40–80× reduction. The NY 3K M=6 instance exemplifies this: only 333 fallback queries among 127K expansions, yet each query's k-d forest reduces 1,097M potential comparisons to 13.3M.

### 6.5 Scaling analysis

Across all eighteen evaluated instances, FAST-MVH delivers consistent acceleration. Three-objective instances show modest speedup (1.8–2.9×), reflecting small frontier heights and tree promotion overhead. From four objectives onward, speedup exceeds 5×, scaling nonlinearly to 82× at six objectives on favorable instances. The speedup scales with (i) frontier cardinality and variance (grid instances with anti-correlated objectives), (ii) problem dimensionality, and (iii) the density of the Pareto frontier in the goal state.

FAST-MVH's variance across road networks reveals the importance of frontier structure: clustered objectives (many instances optimize related route qualities) yield sparser frontiers and modest gains, whereas sparse, high-variance objectives or dense sub-regions of the network produce dramatic speedups. The algorithm is not a universal win; instances with tight, small frontiers benefit modestly. However, across diverse real-world problem classes and the full range of evaluated dimensions, FAST-MVH achieves multi-fold speedups with zero risk of algorithmic error.

</div>

<div class="columns">

## 7 Conclusion

Spatial indexing of truncated frontiers and an indexed residue set reduce dominance-checking cost in multi-objective search with multi-valued heuristics. Three design choices—k-d trees on truncated frontiers, local-first test order, and a logarithmically decomposed residue forest—achieve 1.8× to 82× speedups without changing search decisions or solutions.

</div>
</div>

<div class="references">

## References

1. Bentley, J. L. 1975. Multidimensional Binary Search Trees Used for Associative Searching. *Communications of the ACM* 18(9): 509–517.

2. Bentley, J. L.; and Saxe, J. B. 1980. Decomposable Searching Problems I: Static-to-Dynamic Transformation. *Journal of Algorithms* 1(4): 301–358.

3. Demetrescu, C.; Goldberg, A. V.; and Johnson, D. S., eds. 2009. *The Shortest Path Problem: Ninth DIMACS Implementation Challenge*. DIMACS Series 74. American Mathematical Society.

4. Zhang, H.; Salzman, O.; Kumar, T. K. S.; Hernandez Ulloa, C.; Suazo, L.; and Koenig, S. 2022. A\*pex: Efficient Approximate Multi-Objective Search on Graphs. *Proceedings of ICAPS*, 394–403.

</div>
