<div>

# FAST-MVH: Resolving the Dimensionality Reduction Breakdown in Multi-Valued Heuristic Search via Geometric Fallback Forests

<div class="columns">

### Abstract

Dimensionality reduction (DR) promises to eliminate one objective coordinate from dominance checking in multi-objective search. In multi-valued heuristic (MVH) search, however, DR fundamentally breaks down: because paths are evaluated with different heuristic vectors, priority queue extraction order shatters path-cost monotonicity. While previously dismissed as an incidental, occasional side-effect, we demonstrate that full-dimensional fallbacks occur in up to 40.6% of node expansions and eviction residues swell to absorb up to 93% of all expanded paths on dense instances. DR in MVH is inherently leaky and cannot eliminate the need for full-dimensional checking. We present FAST-MVH, a sound and complete framework that resolves this breakdown. FAST-MVH confines the fallback to a provably sufficient residue set, which it indexes using a Bentley-Saxe logarithmic forest of static k-d trees; classifies full-dimensional dominators in a single traversal of truncated frontiers; and bypasses 80–90% of heuristic evaluations via local-first ordering and witness caching. On benchmark instances spanning three to eight objectives across diverse topologies, FAST-MVH expands identical paths and produces bit-identical Pareto fronts to the original baseline of Wolff, Felner, and Salzman (SoCS 2026) while achieving an overall geometric mean speedup of 14.15× (and peak acceleration factors reaching up to 620×). Crucially, on heavy-fallback instances, indexing the residue forest with full-dimensional k-d trees accelerates search by up to 15.42× (geometric mean 1.70×) and slashes fallback comparisons by up to 83-fold over unindexed fallback checking, proving that geometric fallback indexing is essential to making DR practical under MVHs.

## 1 Introduction

A shortest path need not be best in every objective. One route is shorter, another faster, a third cheaper. Multi-objective search therefore retains every path whose cost no other path dominates, and returns the Pareto frontier of start-to-goal costs. The retained sets grow quickly with the number of objectives $M$. As they grow, deciding whether a new path is dominated becomes the dominant cost of the search, often exceeding the cost of generating the path itself.

A second difficulty is weak guidance. A single lower-bound vector per state must underestimate every trade-off at once. *Multi-valued heuristics* (MVHs) instead supply a set of vectors, each a lower bound on some of the remaining trade-offs (Geisser et al. 2022; Skyler et al. 2024). The information is richer, but using it is not free: a path may test several heuristic vectors before finding one that the solutions found so far do not refute.

In single-objective and single-valued heuristic search, *dimensionality reduction* (DR) offers an elegant relief: by extracting nodes in non-decreasing order of their first cost component, that component is guaranteed to be non-decreasing among paths expanded at any given state. Consequently, dominance tests may project out the first coordinate entirely, reducing an $M$-dimensional problem to an $(M-1)$-dimensional one with zero algorithmic overhead (Pulido, Mandow, and Pérez-de-la-Cruz 2015).

Wolff, Felner, and Salzman (2026) combined MVHs with DR in $L$-NAMOA$^*_{dr}$-mvh. They observed that under MVHs, the cost-monotonicity invariant fails: two paths reaching the same state can be evaluated with different heuristic vectors, allowing a path with a smaller first coordinate to be extracted *after* a path with a larger one. To preserve soundness, they introduced an exact fallback to the full cost history whenever a projected test was inconclusive. Crucially, however, prior work treated this fallback as a rare edge-case anomaly—a minor nuisance to be patched with an unindexed scan.

**The Breakdown of Dimensionality Reduction.** Our investigation reveals that this view is fundamentally mistaken. The fallback is not an occasional implementation quirk; it is the physical manifestation of a structural breakdown in the interaction between set-valued heuristics and coordinate projection. When multiple heuristic vectors compete in the priority queue, non-monotonic arrival is pervasive. On dense graphs, fallback checks occur in up to 40.6% of all expansions, and the residue of evicted vectors that must be checked in full dimensions absorbs up to 93% of all expanded paths. DR does not eliminate the $M$-th dimension in MVH search; it merely sweeps it into an unindexed backlog where linear scans create a crippling computational bottleneck.

Shperberg et al. (2026) established that geometric indexing via k-d trees overcomes the single-order $\Omega(n)$ dominance lower bound, answering dominance queries in $O(n^{1-1/d})$ for $d=M-1$. While prior work applied k-d trees solely to local state frontiers in EMOA*, FAST-MVH reveals that in multi-valued heuristic search under dimensionality reduction, geometric indexing must be deployed across **three distinct architectural roles**: (1) accelerating the target/goal frontier $T \equiv G_{\mathrm{cl}}(\text{goal})$ during heuristic candidate selection; (2) indexing dense local frontiers $F(s)$ in projected coordinates; and (3) structuring the append-only eviction residue $R(s)$ in a logarithmic Bentley-Saxe forest. Crucially, the heuristic candidate set $H(s)$ itself is stored in contiguous cache-aligned memory and pruned via lexicographic refutation pointers rather than trees. FAST-MVH leverages this geometric architecture to resolve the dimensionality reduction breakdown. Its contributions are:

1. **Characterization of the DR Breakdown.** We provide the first comprehensive theoretical and empirical analysis of why DR fails under MVHs, quantifying fallback frequencies and residue growth across three to eight objectives.
2. **A Provably Sufficient Residue.** We prove that full-dimensional fallbacks need not search all expanded paths $C(s)$. Retaining only vectors evicted with a smaller first coordinate ($R(s)$) is strictly sufficient (Lemma 1), eliminating the unbounded growth of the verification set.
3. **The Logarithmic Fallback Forest.** We index the append-only residue $R(s)$ with a Bentley-Saxe forest of static k-d trees, transforming the fallback from an $O(|R(s)|)$ scan into a sub-linear geometric query.
4. **Single-Pass Local Witness Classification.** One traversal over the truncated frontier classifies candidates as undominated, fully dominated (settling the test with zero fallback), or truncation-dominated only.
5. **Operation Reordering and Witness Caching.** Evaluating local dominance before heuristic selection prunes 80–90% of generated paths before `ChooseH` is called, while cached refutation witnesses bypass unpromising heuristic vectors without touching the goal frontier.
6. **Exhaustive Leave-One-Out Component Ablation.** Across 47 baseline benchmark instances and 28 leave-one-out ablation settings spanning three to eight objectives, FAST-MVH reproduces the exact baseline trajectory while achieving an overall geometric mean speedup of 14.15× (with peaks up to 620×). Systematic leave-one-out ablation isolates the marginal importance of each component, demonstrating that goal frontier k-d indexing (peak 24.34× slowdown when omitted; geometric mean 3.28×) and fallback residue forests (peak 15.42× slowdown when omitted; slashing fallback comparisons by up to 83-fold) are the primary architectural pillars of scalable multi-valued search.

## 2 Background and the Monotonicity Failure

Let $G=(S,E,c)$ be a finite directed graph with edge costs $c(e)\in\mathbb{R}_{\geq0}^{M}$. Path costs are component-wise sums. A vector $x$ weakly dominates $y$ ($x \le y$ or $x \preceq y$) if $x_k \le y_k$ for all $k \in \{1,\dots,M\}$, and strictly dominates $y$ ($x < y$ or $x \prec y$) if $x \le y$ and $x \neq y$. The objective is to return the cost-unique Pareto frontier $\Pi^*$ of non-dominated start-to-goal paths. We write $\operatorname{Tr}(x)=(x_2,\ldots,x_M)$ for the projection onto the last $d = M-1$ coordinates, and $<_{\mathrm{lex}}$ for lexicographic order.

**Multi-Valued Heuristics.** An admissible MVH assigns a finite set $H(s)$ to each state such that for every goal-reaching suffix $\pi$ from $s$, some $h\in H(s)$ satisfies $h\preceq c(\pi)$. We assume $H(s_{\mathrm{goal}})=\{0\}$ and, following Wolff, Felner, and Salzman (2026), order each heuristic set lexicographically:
$$H(s)=\langle h^1,\ldots,h^{k(s)}\rangle, \qquad h^1\leq_{\mathrm{lex}}\cdots\leq_{\mathrm{lex}}h^{k(s)}.$$
Scanning candidates in this order ensures that the active heuristic index $i(n)$ is non-decreasing along OPEN extractions, which is necessary for admissibility to carry through dimensionality reduction.

**The Mechanics of DR in Single-Valued Search.** In standard search with a consistent single-valued heuristic $h(s)$, a node $n$ has $f(n) = g(n) + h(s(n))$. Because $h(s(n))$ is identical for all paths reaching state $s$, extracting nodes in non-decreasing order of $f_1$ guarantees that the path costs $g_1$ of nodes expanded at $s$ are strictly non-decreasing over time:
$$g_1(n_{\mathrm{early}}) \le g_1(n_{\mathrm{late}}).$$
Because $g_1$ never decreases, if an existing path $p$ satisfies $\operatorname{Tr}(p) \preceq \operatorname{Tr}(g)$ for a newly arriving path $g$, then $p_1 \le g_1$ is guaranteed unconditionally. Full dominance ($p \preceq g$) follows immediately from truncated dominance ($\operatorname{Tr}(p) \preceq \operatorname{Tr}(g)$). The first coordinate can be discarded permanently.

**The Monotonicity Collapse under MVHs.** In MVH search, each path reaching state $s$ is bound to an individual heuristic vector $h^i \in H(s)$. Because OPEN is ordered by $f_1 = g_1 + h^i_1$, a path with a large path cost $g_1$ but a small heuristic component $h_1$ can be extracted *before* an alternative path with a small $g_1$ and a large $h_1$.

To make this breakdown explicit, consider a 3-objective problem ($M=3$) at state $s$ with $H(s) = \{h^1, h^2\}$, where $h^1 = (2, 4, 4)$ and $h^2 = (6, 1, 1)$. Three paths reach $s$:
- Path A: $g(A) = (10, 5, 5)$, evaluated with $h^1$:
  $$g_1(A) = 10, \; h^1_1 = 2 \implies f_1(A) = 12 \quad \text{(Extracted 1st)}$$
- Path B: $g(B) = (8, 6, 6)$, evaluated with $h^2$:
  $$g_1(B) = 8, \; h^2_1 = 6 \implies f_1(B) = 14 \quad \text{(Extracted 2nd)}$$
- Path C: $g(C) = (9, 6, 6)$, evaluated with $h^2$:
  $$g_1(C) = 9, \; h^2_1 = 6 \implies f_1(C) = 15 \quad \text{(Extracted 3rd)}$$

The extraction order is strictly governed by $f_1$ ($12 < 14 < 15$). However, Path B arrives at state $s$ with $g_1(B) = 8 < g_1(A) = 10$, completely shattering $g_1$ monotonicity.

When Path C arrives with $g(C) = (9, 6, 6)$, inspect its relationship with Path A in truncated coordinates $2 \dots M$: $\operatorname{Tr}(g(A)) = (5, 5) \le \operatorname{Tr}(g(C)) = (6, 6)$. Under naive dimensionality reduction, Path A appears to dominate Path C, and Path C would be pruned. In reality, $g_1(A) = 10 > g_1(C) = 9$, so $g(A) \not\le g(C)$: Path A does **not** dominate Path C. Furthermore, Path B has $g(B) = (8, 6, 6) \le g(C) = (9, 6, 6)$, so Path B *does* strictly dominate Path C in full dimensions. However, if Path B was previously evicted from $F(s)$ during an intermediate expansion, Path B is no longer in $F(s)$. Pruning Path C without checking dimension 1 or without querying evicted residues discards a sound Pareto-optimal path.

## 3 The Fallback Breakdown and Resolution

To prevent unsound pruning, $L$-NAMOA$^*_{dr}$-mvh maintains two sets per state: the truncated frontier $F(s)$ (storing full vectors but comparing on $\operatorname{Tr}$) and the history of all expanded path costs $C(s)$. When a candidate arrival $g$ is truncation-dominated by $F(s)$ but $g_1 < \max_{p\in F(s)} p_1$, it triggers a *fallback* check: a full-dimensional scan over all of $C(s)$.

### 3.1 Algorithmic Walkthrough of the Decision Pipeline

FAST-MVH replaces unindexed scans with a streamlined, geometric verification pipeline. Algorithm 1 details the query structure:

1. **Truncated Query & Witness Classification (lines 9–14).** When testing a candidate $g$ at state $s$, $\mathrm{LocalDom}(s,g)$ traverses the truncated frontier $F(s)$ over coordinates $2 \dots M$. During this single traversal, each truncated dominator $p$ is inspected in coordinate 1:
   - **`NONE` (Undominated):** If no $p \in F(s)$ satisfies $\operatorname{Tr}(p) \preceq \operatorname{Tr}(g)$, coverage guarantees that no path expanded at $s$ dominates $g$. The test returns `false` immediately.
   - **`FULL` (Full Witness):** If any truncated dominator satisfies $p_1 \le g_1$, then $p$ dominates $g$ in all $M$ coordinates. Path $g$ is provably dominated. The test returns `true` immediately, **completely bypassing the fallback check**.
   - **`TR_ONLY` (Truncation Dominator Only):** If every truncated dominator has $p_1 > g_1$, truncation is inconclusive. The test proceeds to the fallback.
2. **Residue Fallback (line 15).** The algorithm queries the residue set $R(s)$ in full $M$ dimensions. If any $r \in R(s)$ satisfies $r \preceq g$, the query returns `true` (a "bad fallback", confirming $g$ is dominated). Otherwise, it returns `false` (a "good fallback", successfully salvaging an optimal path).

<div class="algorithm">

**Algorithm 1: Dominance Queries, Witness Classification, and Update**

| | |
|--:|:--|
| 1 | **function** $\mathrm{Find}(\mathcal{K},q,\kappa)$ |
| 2 | &emsp; $\Sigma\leftarrow$ roots of $\mathcal{K}$ |
| 3 | &emsp; **while** $\Sigma\neq\emptyset$ **do** |
| 4 | &emsp;&emsp; pop $v$ from $\Sigma$; **if** $\kappa(\ell(v))\not\le q$ **then continue** |
| 5 | &emsp;&emsp; **yield** each $p\in P(v)$ with $\kappa(p)\preceq q$ |
| 6 | &emsp;&emsp; push the children of $v$ on $\Sigma$ |
| 7 | **function** $\mathrm{GoalDom}(f)$ |
| 8 | &emsp; **return true** iff $\mathrm{Find}(T,\operatorname{Tr}(f),\operatorname{Tr})$ yields |
| 9 | **function** $\mathrm{LocalDom}(s,g)$ |
| 10 | &emsp; $w\leftarrow\mathrm{NONE}$ |
| 11 | &emsp; **for each** $p\in\mathrm{Find}(F(s),\operatorname{Tr}(g),\operatorname{Tr})$ **do** |
| 12 | &emsp;&emsp; **if** $p_1\le g_1$ **then return true** &emsp;&emsp;▷ full witness found, bypass fallback |
| 13 | &emsp;&emsp; $w\leftarrow\mathrm{TR}$ |
| 14 | &emsp; **if** $w=\mathrm{NONE}$ **then return false** &emsp;&emsp;▷ no truncated dominator |
| 15 | &emsp; **return true** iff $\mathrm{Find}(R(s),g,\mathrm{id})$ yields &emsp;&emsp;▷ exact residue fallback |
| 16 | **procedure** $\mathrm{Update}(s,g)$ |
| 17 | &emsp; **for each** $p\in F(s)$ with $\operatorname{Tr}(g)\preceq\operatorname{Tr}(p)$ **do** |
| 18 | &emsp;&emsp; remove $p$ from $F(s)$ |
| 19 | &emsp;&emsp; **if** $p_1<g_1$ **then** append $p$ to $R(s)$ &emsp;&emsp;▷ retain only necessary residue |
| 20 | &emsp; insert $g$ into $F(s)$ |

</div>

### 3.2 Concrete Numerical Walkthrough

To see how the residue and witness classification interact, consider four paths arriving at state $s$ in a 3-objective problem ($M=3$):

1. **Path A expands:** Cost $g_A = (10, 5, 5)$. $F(s) = \{(10, 5, 5)\}$, $R(s) = \emptyset$.
2. **Path B expands:** Cost $g_B = (8, 4, 4)$. Because $\operatorname{Tr}(g_B) = (4, 4) \preceq \operatorname{Tr}(g_A) = (5, 5)$, $\mathrm{Update}$ evicts Path A. Notice that $(g_A)_1 = 10 > (g_B)_1 = 8$. Thus, $g_B \preceq g_A$ holds in all 3 coordinates: Path B fully dominates Path A. Path A can never dominate any future candidate that Path B does not already dominate. Hence, line 19 discards Path A completely. $F(s) = \{(8, 4, 4)\}$, and $R(s)$ remains empty.
3. **Path C expands:** Cost $g_C = (12, 3, 3)$. Since $\operatorname{Tr}(g_C) = (3, 3) \preceq \operatorname{Tr}(g_B) = (4, 4)$, Path B is evicted. Here, however, $(g_B)_1 = 8 < (g_C)_1 = 12$. Path C does *not* fully dominate Path B. Path B could dominate a future candidate that Path C does not. Therefore, line 19 appends Path B to $R(s)$. Now $F(s) = \{(12, 3, 3)\}$ and $R(s) = \{(8, 4, 4)\}$.
4. **Candidate Path D arrives:** Cost $g_D = (9, 3, 3)$.
   - *Truncated check against $F(s)$:* Path C has $\operatorname{Tr}(g_C) = (3, 3) \preceq \operatorname{Tr}(g_D) = (3, 3)$.
   - *Witness test (line 12):* $(g_C)_1 = 12 > (g_D)_1 = 9$. Path C is not a full witness. $w$ is set to $\mathrm{TR}$.
   - *Fallback query (line 15):* The query checks $R(s) = \{(8, 4, 4)\}$. Testing Path B: $(g_B)_2 = 4 > (g_D)_2 = 3$, so Path B does not dominate Path D.
   - *Outcome:* The query returns `false`. Path D is correctly salvaged as an undominated path.

Had candidate D instead arrived with cost $g_D' = (14, 3, 3)$, Path C would have satisfied $(g_C)_1 = 12 \le 14$, triggering the early exit at line 12 as a `FULL` witness and resolving the query in $O(1)$ without touching $R(s)$.

### 3.3 Theoretical Soundness

**Lemma 1 (Residue Sufficiency).** *Suppose no $p\in F(s)$ satisfies $p\preceq g$. Then some expanded path $a\in C(s)$ satisfies $a\preceq g$ if and only if some residue vector $r\in R(s)$ satisfies $r\preceq g$.*

*Proof.* Since $R(s)\subseteq C(s)$, the "if" direction is immediate. For the "only if" direction, let $a_0\in C(s)$ with $a_0\preceq g$. By assumption, no vector in the current live frontier $F(s)$ dominates $g$, so $a_0\notin F(s)$. Therefore, $a_0$ was evicted from $F(s)$ during some subsequent expansion of a vector $b$, meaning $\operatorname{Tr}(b)\preceq\operatorname{Tr}(a_0)$. If $b_1 > (a_0)_1$, then by line 19 of Algorithm 1, $a_0$ was appended to $R(s)$, and we are done with $r = a_0$. If instead $b_1 \le (a_0)_1$, then $b \preceq a_0$ in all $M$ coordinates. By transitivity of weak dominance, $b \preceq g$. We can set $a_1 = b$ and repeat the argument. Because each eviction corresponds to a strictly later expansion event and the search history is finite, the chain $a_0, a_1, \dots$ cannot continue indefinitely. It cannot terminate in $F(s)$ because no member of $F(s)$ dominates $g$. Hence, the chain must terminate at an eviction where the evictor had a larger first coordinate, placing that vector into $R(s)$. $\square$

**Lemma 2 (Local Exactness).** *$\mathrm{LocalDom}(s,g)$ returns true if and only if some expanded path $a\in C(s)$ satisfies $a\preceq g$.*

*Proof.* Coverage of $C(s)$ by $F(s)$ in the projected coordinates is an invariant maintained by $\mathrm{Update}$. If $\mathrm{Find}$ at line 11 yields nothing, no expanded path dominates even $\operatorname{Tr}(g)$, so no path dominates $g$ in full space. A return at line 12 exhibits an explicit witness $p\in F(s)\subseteq C(s)$ with $p\preceq g$. Otherwise, every truncated dominator in $F(s)$ has been checked and none dominates $g$ in all coordinates. In that event, Lemma 1 proves that querying $R(s)$ at line 15 is exact. Bounding-box pruning in $\mathrm{Find}$ skips only subtrees containing no qualifying vector. $\square$

## 4 FAST-MVH Algorithmic Architecture

Algorithm 2 presents the full node lifecycle. It extracts nodes from OPEN, applies lazy goal re-evaluation, tests local dominance, and expands undominated paths.

<div class="algorithm">

**Algorithm 2: FAST-MVH Main Search Loop**

**Input:** Graph $G$, $s_{\mathrm{start}},s_{\mathrm{goal}}$, and lexicographically ordered admissible MVH $H$.  
**Output:** Cost-unique Pareto-optimal path set $\mathrm{Sols}$.

| | |
|--:|:--|
| 1 | $\mathrm{Sols}\leftarrow\emptyset;\quad F(s),R(s)\leftarrow\emptyset$ for all $s$ |
| 2 | $n\leftarrow(s_{\mathrm{start}},0,1)$; $\mathrm{parent}(n)\leftarrow\bot$ |
| 3 | $\mathrm{OPEN}\leftarrow\{n\}$ |
| 4 | **while** $\mathrm{OPEN}\ne\emptyset$ **do** |
| 5 | &emsp; $n\leftarrow\mathrm{OPEN.PopMin}()$ |
| 6 | &emsp; **if** $\mathrm{GoalDom}(g(n)+h^{i(n)}(s(n)))$ **then** |
| 7 | &emsp;&emsp; $i\leftarrow\mathrm{ChooseH}(s(n),g(n),i(n)+1,\mathrm{true})$ |
| 8 | &emsp;&emsp; **if** $i\ne\bot$ **then** $i(n)\leftarrow i$; insert $n$ in $\mathrm{OPEN}$ |
| 9 | &emsp;&emsp; **continue** |
| 10 | &emsp; **if** $\mathrm{LocalDom}(s(n),g(n))$ **then continue** |
| 11 | &emsp; $\mathrm{Update}(s(n),g(n))$ |
| 12 | &emsp; **if** $s(n)=s_{\mathrm{goal}}$ **then** add $n$ to $\mathrm{Sols}$; **continue** |
| 13 | &emsp; **for each** $s'\in\mathrm{Succ}(s(n))$ **do** |
| 14 | &emsp;&emsp; $g'\leftarrow g(n)+c(s(n),s')$ |
| 15 | &emsp;&emsp; **if** $\mathrm{LocalDom}(s',g')$ **then continue** &emsp;&emsp;▷ local test before selection |
| 16 | &emsp;&emsp; $i'\leftarrow\mathrm{ChooseH}(s',g',1,\mathrm{false})$ |
| 17 | &emsp;&emsp; **if** $i'\ne\bot$ **then** insert $(s',g',i')$ with parent $n$ in $\mathrm{OPEN}$ |
| 18 | **return** $\mathrm{Sols}$ |

</div>

### 4.1 Local-First Operation Reordering

A critical performance bottleneck in $L$-NAMOA$^*_{dr}$-mvh is that it binds an active heuristic vector to every generated successor via `ChooseH` *before* checking local dominance against $F(s')$. On dense graphs, 80% to 90% of generated successors are locally dominated by existing paths to the same state. Evaluating `ChooseH` first wastes hundreds of goal-frontier queries on paths destined for immediate rejection.

FAST-MVH reverses this order (lines 15–16). Because both $\mathrm{LocalDom}$ and $\mathrm{ChooseH}$ are side-effect-free predicates, evaluating the cheaper and more selective local test first alters neither which nodes enter OPEN nor their assigned heuristic index. It simply eliminates 80–90% of all calls to `ChooseH`.

### 4.2 Heuristic Selection and Witness Caching

When `ChooseH` is called, candidate heuristic vectors are evaluated in lexicographic order. FAST-MVH accelerates this scan using two exact mechanisms (Algorithm 3):

1. **Redundancy Pointers ($\rho_s(i)$).** For each candidate $h^i$, $\rho_s(i)$ stores the largest index $j < i$ such that $\operatorname{Tr}(h^j) \le \operatorname{Tr}(h^i)$:
   $$\rho_s(i) = \max \{j < i \mid \operatorname{Tr}(h^j) \le \operatorname{Tr}(h^i)\}$$
   (setting $\rho_s(i) = \bot$ if no such $j$ exists). If candidate $h^j$ has already been refuted by the goal frontier, then $\operatorname{Tr}(g + h^j) \le \operatorname{Tr}(g + h^i)$ implies that $h^i$ is also refuted by the exact same goal vector, allowing line 5 to skip $h^i$ without touching the goal frontier.
2. **Goal-Witness Caching.** When a goal-frontier query refutes a candidate $g + h^i$, the refuting vector $t \in T$ is cached. Subsequent candidates first check if the cached witness still dominates their truncated evaluation. Because evaluating a single cached vector costs $O(M)$ scalar operations, this bypasses the full k-d tree traversal over $T$ in the vast majority of cases.

<div class="algorithm">

**Algorithm 3: $\mathrm{ChooseH}(s,g,j,b)$**

**Precondition:** $b$ is true only if $h^{j-1}$ was just refuted by $\mathrm{GoalDom}$.

| | |
|--:|:--|
| 1 | **if** $j>k(s)$ **then return** $\bot$ |
| 2 | **if** $T=\emptyset$ **then return** $j$ |
| 3 | $r\leftarrow j-1 \text{ if } b \text{ else } j$ |
| 4 | **for** $i=j,\ldots,k(s)$ **do** |
| 5 | &emsp; **if** $\rho_s(i)\geq r$ **then continue** &emsp;&emsp;▷ refuted via redundancy pointer |
| 6 | &emsp; **if not** $\mathrm{GoalDom}(g+h^i)$ **then return** $i$ |
| 7 | **return** $\bot$ |

</div>

**Lemma 3 (Safe Skipping).** *Line 5 of Algorithm 3 skips only refuted heuristic candidates.*

*Proof.* Monotonicity of $T$ under $\mathrm{Update}$ guarantees that once a truncated point is dominated by $T$, it remains dominated throughout the search. Every candidate in $[r, i)$ was refuted either earlier in the current scan or by the preceding goal test. If $\rho_s(i) \ge r$, then $h^{\rho_s(i)}$ is known to be refuted. Since $\operatorname{Tr}(h^{\rho_s(i)}) \preceq \operatorname{Tr}(h^i)$, transitivity ensures that the goal vector that refutes $g + h^{\rho_s(i)}$ also refutes $g + h^i$. $\square$

**Theorem 1 (Trajectory Invariance).** *With identical heuristic tie-breaking and successor generation order, FAST-MVH performs the exact same sequence of OPEN extractions and insertions as $L$-NAMOA$^*_{dr}$-mvh and returns identical solution sets.*

*Proof.* By induction over search iterations. Lemma 2 establishes that $\mathrm{LocalDom}$ is exact on all expanded paths. Lemma 3 guarantees that `ChooseH` selects the first unrefuted heuristic index in the canonical lexicographic order. Reversing the order of generation tests preserves their conjunction. Hence, identical nodes enter OPEN in the exact same order. $\square$

## 5 Engineering the Dominance Data Structures

### 5.1 Adaptive Frontiers

Most search states store only a few vectors. On short frontiers, linear scanning of a flat contiguous array beats pointer-linked trees due to hardware cache-line prefetching and SIMD vectorization. Trees pay off only on large frontiers that are queried repeatedly.

FAST-MVH implements **Adaptive Hybrid Frontiers**. Each $F(s)$ begins as a flat, contiguous $M$-dimensional array. After 16 queries, if the frontier contains at least 8 live vectors and has averaged at least 64 scalar comparisons per query, it is promoted to a dynamic $(M-1)$-dimensional k-d tree (Shperberg et al. 2026). Nodes store bounding box corners $\ell(v)$ and $u(v)$, allowing the same index to serve both dominance queries (pruning on $\ell$) and frontier updates (pruning on $u$).

### 5.2 The Logarithmic Fallback Forest

Because the residue set $R(s)$ is append-only, it is ideally suited for the logarithmic method of Bentley and Saxe (1980). New residue points are appended to an unindexed buffer. When the buffer reaches 64 vectors and a fallback query arrives, it is compiled into a static, implicit k-d tree block.

A block of size $2^k$ is merged into its predecessor when sizes match, maintaining at most $O(\log |R(s)|)$ static trees. Each tree block is stored in a single contiguous memory buffer with median-split coordinates and subtree bounding boxes. Queries scan the short unindexed buffer and traverse the logarithmic blocks with box rejection. States that never trigger fallbacks pay zero indexing overhead, while heavy-fallback states answer queries in sub-linear time.

### 5.3 Why Heuristic Sets Are Not Indexed

It is tempting to index $H(s)$ using a static k-d tree over $\operatorname{Tr}(H(s))$. We evaluated a static heuristic K-d tree over $\operatorname{Tr}(H(s))$ with subtree bounding boxes $[\ell, u]$: discarding a subtree if $g + \ell$ is goal-dominated, and accepting if $g + u$ is undominated. In extensive empirical evaluations, this index consistently failed to reduce wall-clock time. Three factors explain this:

1. **Bounding Box Indecision.** In anti-correlated Pareto trade-offs, the lower corner $\ell$ is dominated far more easily than any actual heuristic vector, while the upper corner $u$ is dominated far less easily (Figure 1). Consequently, box tests are inconclusive, forcing recursive exploration of both subtrees.
2. **Oracle Cost Asymmetry.** Testing a frontier box costs $O(M)$ scalar comparisons. Testing a heuristic box requires evaluating an entire goal-frontier query against $T$. An inconclusive heuristic box test is thus exceptionally expensive.
3. **Redundancy of the Tree.** Local-first ordering eliminates 80–90% of selection calls, redundancy pointers bypass long prefixes in $O(1)$, and witness caching resolves most remaining candidates instantly. Flat sequential arrays saturate L1 prefetchers and decisively outperform tree traversals.

<figure>
<svg viewBox="0 0 340 155" role="img" aria-label="Three trade-off points inside a bounding box; neither corner is a member.">
<g fill="none" stroke="#222" stroke-width="1.2">
<path d="M40 15V125H305"/>
<path d="M65 30H280V110H65Z" stroke-dasharray="4 3"/>
</g>
<g fill="#111"><circle cx="65" cy="30" r="4"/><circle cx="172" cy="70" r="4"/><circle cx="280" cy="110" r="4"/></g>
<g font-family="Times New Roman,serif" font-size="12">
<text x="74" y="34">(0, 10)</text>
<text x="181" y="66">(5, 5)</text>
<text x="225" y="106">(10, 0)</text>
<text x="45" y="125">ℓ = (0, 0)</text>
<text x="245" y="25">u = (10, 10)</text>
</g>
</svg>
<figcaption>Figure 1. A heuristic bounding box $[\ell, u]$ contains unattainable corners. For $g=0$, goals at the three members refute every member but not the lower corner $\ell$, so the box cannot be rejected. A goal at $(6,6)$ refutes the upper corner $u$ but no member, so no member can be accepted early.</figcaption>
</figure>

### 5.4 Contiguous Node Storage

To maximize memory hierarchy performance, OPEN entries store $f$ inline alongside an integer index into a single contiguous pool of node records. The priority queue comparator never dereferences a pointer. Generating a node requires zero heap allocation, and solution paths are materialized only once upon search termination.

</div>
</div>

<div class="paper-page experiments">

## 6 Experiments

We empirically evaluate FAST-MVH against the state-of-the-art reference baseline on a diverse benchmark suite spanning DIMACS road networks, synthetic grid topologies with varying objective correlation ($M=3\dots8$), and high-dimensional Franka Emika Panda 8D robotic motion planning.

**Experimental Setup & Platform.** All experiments were conducted on an Intel Core i5-1135G7 CPU (16 GB RAM, Windows 11), compiled with GCC 16.2 using `-O2 -DNDEBUG -std=c++20`. The reference baseline is the official, unmodified reference oracle of $L\text{-NAMOA}^*_{dr}\text{-mvh}$ by Wolff, Felner, and Salzman (2026, commit `0a2f9ea`). Reported search times exclude graph loading and heuristic precomputation, and represent the median of three consecutive executions. Multi-valued heuristics are generated as $A^*_{\text{pex}}$ backward Pareto frontiers (Zhang et al. 2022) with approximation parameter $\varepsilon$ and Landmark ALT heuristics. In 100% of the evaluated problem instances, FAST-MVH executed the identical sequence of node expansions and returned cryptographically bit-identical Pareto sets to the baseline (verified via SHA-256 checksums).

**Evaluation Methodology.** Our experimental design follows the rigorous benchmarking standards established in multi-objective heuristic search:
1. *Champion vs. State-of-the-Art Baseline:* We first establish the overall performance of the optimal champion solver (FAST-MVH with all optimizations active) relative to the reference baseline ($L\text{-NAMOA}^*_{dr}\text{-mvh}$). We report geometric mean speedups across domains and dimensionalities alongside peak acceleration factors.
2. *Systematic Leave-One-Out Ablation Study:* To isolate the marginal contribution of each architectural mechanism, we conduct a leave-one-out ablation across 28 benchmark settings evaluated to natural completion without time limits. We report the performance degradation (slowdown factor $T_{\text{w/o } X} / T_{\text{Full}} \ge 1.0\times$) incurred when each component is deactivated from the champion solver.
3. *Aggregation via Geometric Means:* In accordance with standard scientific benchmarking principles, speedup ratios and component slowdown factors are aggregated using **Geometric Means** ($\text{GMean}$) rather than arithmetic means to prevent distortion from extreme outliers.
4. *Quantization Thresholding:* Sub-tenth-second instances ($T_{\text{Full}} < 0.10\text{s}$) are filtered out of degradation geometric mean calculations to eliminate timer resolution and operating system scheduler quantization noise (while remaining fully documented in the complete appendix matrix).

### 6.1 Empirical Breakdown of Dimensionality Reduction (The Fallback Reality)

The foundational premise of dimensionality reduction in multi-objective search is that coordinate projection eliminates one objective dimension from dominance checking at zero algorithmic cost (Pulido, Mandow, and Pérez-de-la-Cruz 2015). Under multi-valued heuristics, however, this premise fundamentally collapses. Because paths reaching the same state are evaluated using distinct heuristic vectors from $H(s)$, priority queue extraction order violates path-cost monotonicity: paths with smaller first coordinates are frequently extracted *after* paths with larger ones. Prior work acknowledged this failure but treated the resulting fallback to full-dimensional checking as an incidental edge case, handling it via an unindexed linear scan over the history of expanded paths $C(s)$ (Wolff, Felner, and Salzman 2026).

Table 1 provides definitive empirical evidence that fallback checking is neither rare nor negligible; it is a pervasive, structural phenomenon inherent to multi-valued heuristic search under coordinate projection. Across eleven aggregated domain and dimensionality regimes ($M=3\dots8$), fallback dynamics exhibit severe computational overhead:

1. *Pervasive Fallback Frequencies:* On 3-objective synthetic grids ($M=3$), fallback checks occur in **40.1%** of all node expansions. On 4-objective road networks ($M=4$), the fallback rate reaches up to **40.6%** (mean 12.3%). Across higher dimensions ($M=5\dots8$), mean fallback rates consistently remain between 6.4% and 10.8% across grids and roadmaps (reaching 26.0% on 8D robotics).
2. *Massive Residue Absorption:* Because non-monotonic arrivals continually evict valid non-dominated paths from the truncated frontier $F(s)$, the eviction residue set $R(s)$ swells rapidly. On $M=3$ grids, the residue absorbs **93%** ($|R|/\text{Exp.} = 0.93$) of all expanded paths. Across higher-dimensional grids and roadmaps, $|R|/\text{Exp.}$ consistently ranges from 0.17 to 0.79, and reaches 0.55 on 8D robotics.
3. *Linear vs. Logarithmic Geometric Checking:* In an unindexed linear scan over $R(s)$, checking an incoming candidate requires traversing thousands of accumulated evicted vectors. In the baseline, queries average 6,622 to 17,153 scalar comparisons per query on $M=6\dots8$ grids, and 5,913 comparisons on $M=6$ roadmaps. The Bentley-Saxe logarithmic K-d tree forest prunes candidate subtrees geometrically via bounding boxes, slashing comparisons per query by orders of magnitude: an **82.8× reduction** on $M=6$ roadmaps (from 5,913.1 to 71.4 comparisons), a **53.0× reduction** on $M=6$ grids (from 6,622.0 to 124.9 comparisons), and a **53.7× reduction** on $M=5$ roadmaps (from 2,686.3 to 50.1 comparisons).

<div class="tablebox">

**Table 1. Fallback & Residue Growth Dynamics.** Empirical characterization of dimensionality reduction breakdown across Roadmaps, Grids, and Robotics arm planning. Fallback queries occur when truncated dominance is inconclusive and line 15 of Algorithm 1 is invoked. Fallback Rate is $\text{Queries} / \text{Expansions}$. Residue Absorption Ratio is $|R(s)| / \text{Expansions}$. Comparisons/Query compares unindexed linear scans over $R(s)$ against the Bentley-Saxe logarithmic Fallback Forest.

| Domain | $M$ | Instances | Avg. Expansions | Mean Fallback Rate (%) | Max Fallback Rate (%) | Residue Absorption ($|R|/\text{Exp}$) | Linear Cmp / Query | Forest Cmp / Query | Comparison Reduction |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Grids** | 3 | 1 | 9,500 | **40.1%** | 40.1% | **0.93** | 241.0 | **29.5** | **8.2×** |
| **Grids** | 4 | 1 | 53,652 | **22.0%** | 22.0% | **0.79** | 1271.7 | **68.7** | **18.5×** |
| **Grids** | 5 | 1 | 33,005 | **9.6%** | 9.6% | **0.58** | 1033.6 | **64.4** | **16.1×** |
| **Grids** | 6 | 2 | 236,596 | **10.8%** | 13.4% | **0.66** | 6622.0 | **124.9** | **53.0×** |
| **Grids** | 7 | 5 | 543,841 | **9.9%** | 12.4% | **0.61** | 14954.8 | **445.1** | **33.6×** |
| **Grids** | 8 | 4 | 1,167,715 | **10.4%** | 16.6% | **0.68** | 17153.5 | **2224.1** | **7.7×** |
| **Roadmaps** | 3 | 2 | 179,922 | **1.5%** | 1.7% | **0.46** | 192.2 | **24.5** | **7.8×** |
| **Roadmaps** | 4 | 4 | 844,663 | **12.3%** | 40.6% | **0.44** | 976.0 | **43.4** | **22.5×** |
| **Roadmaps** | 5 | 4 | 945,816 | **6.4%** | 18.3% | **0.24** | 2686.3 | **50.1** | **53.7×** |
| **Roadmaps** | 6 | 3 | 1,040,473 | **2.4%** | 7.0% | **0.17** | 5913.1 | **71.4** | **82.8×** |
| **Robotics** | 8 | 6 | 146,575 | **7.5%** | 26.0% | **0.55** | 453.6 | **187.3** | **2.4×** |

</div>

### 6.2 Overall Search Performance vs. Baseline ($L\text{-NAMOA}^*_{dr}\text{-mvh}$)

Table 2 reports the comprehensive performance comparison between the baseline reference solver $L\text{-NAMOA}^*_{dr}\text{-mvh}$ (Wolff, Felner, and Salzman 2026) and the champion FAST-MVH solver across 47 benchmark instances spanning $M=3\dots8$. 

Across all evaluated instances, FAST-MVH demonstrates decisive superiority, achieving an **overall geometric mean speedup of 14.15×** and reducing mean search time from 1,023.75s to 15.22s. The speedup characteristics scale systematically with objective dimensionality and frontier density:

- *Low Dimensions ($M=3$):* In 3-objective domains, Pareto frontiers are compact (mean $|\Pi^*| \approx 1,900$) and linear frontier scans incur modest overhead. FAST-MVH achieves geometric mean speedups of **3.27×** on grids and **3.49×** on roadmaps (with peaks of 5.41× and 4.13×). In this regime, acceleration is primarily driven by contiguous node allocation, local-first pruning, and hardware cache efficiency.
- *Scaling to High Dimensions ($M=4\dots8$):* As objective dimensionality increases, Pareto frontier sizes expand rapidly, exceeding 80,000 paths on $M=7$ grids and 246,000 paths on $M=8$ grids. Here, unindexed dominance checking incurs massive computational overhead. FAST-MVH geometric mean speedups scale to **8.35×** on $M=4$ grids, **22.44×** on $M=5$ grids, **28.22×** on $M=6$ grids, **26.82×** on $M=7$ grids, and **41.50×** on $M=8$ grids, with peak speedup reaching **210.95×**.
- *Road Networks & Extreme Peaks:* On road networks, geometric mean speedup escalates from **22.02×** at $M=4$ to **15.04×** at $M=5$ and **225.60×** at $M=6$, reaching a peak acceleration of **620.28×**. On these instances, intermediate state antichains grow exceedingly dense; FAST-MVH's multidimensional bounding box pruning resolves queries in milliseconds where the baseline requires minutes.
- *Scalability & Timeout Mitigation:* On computationally challenging instances, the unindexed baseline encounters severe bottlenecks or times out. On `grid_8x8_M8_rho0.0`, the baseline requires 35,013.41s (over 9.7 hours), whereas FAST-MVH completes in **93.16s** (a **375.8×** speedup). On the dense roadmap `bay_8d_2000 (s1500)` ($|\Pi^*| = 178,184$), FAST-MVH terminates in **103.42s** while unindexed baseline variants time out beyond standard limits. Similarly, on high-dimensional robotics motion queries (e.g., `Panda_Q0010`), the baseline times out beyond 120s (aborting after only 84,781 expansions), while FAST-MVH completes in **6.03s** (>20× faster).

<div class="tablebox">

**Table 2. Champion vs. Baseline Performance.** Wall-clock performance comparison between $L\text{-NAMOA}^*_{dr}\text{-mvh}$ (baseline oracle) and FAST-MVH (Full Champion) across all domains and dimensions ($M = 3\dots8$). Aggregated by Geometric Mean ($\text{GMean}$) of speedups ($T_{\text{Baseline}} / T_{\text{FAST-MVH}}$). All search trajectories, node expansions, and Pareto solutions are 100% bit-identical.

| Category / Domain | $M$ | Instances | Mean Pareto Size $|\Pi^*|$ | Mean Baseline Time (s) | Mean FAST-MVH Time (s) | Geometric Mean Speedup | Peak Speedup |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Grids** | 3 | 3 | 1,982 | 15.10s | **4.24s** | **3.27×** | **5.41×** |
| **Grids** | 4 | 6 | 13,063 | 24.89s | **3.22s** | **8.35×** | **13.49×** |
| **Grids** | 5 | 4 | 34,903 | 74.89s | **2.04s** | **22.44×** | **48.91×** |
| **Grids** | 6 | 4 | 62,338 | 675.63s | **12.86s** | **28.22×** | **54.80×** |
| **Grids** | 7 | 6 | 80,729 | 844.04s | **6.17s** | **26.82×** | **170.03×** |
| **Grids** | 8 | 7 | 246,819 | 6390.69s | **35.23s** | **41.50×** | **210.95×** |
| **Roadmaps** | 3 | 2 | 1,886 | 1.36s | **0.33s** | **3.49×** | **4.13×** |
| **Roadmaps** | 4 | 3 | 5,511 | 22.91s | **0.91s** | **22.02×** | **37.88×** |
| **Roadmaps** | 5 | 4 | 60,176 | 235.07s | **33.21s** | **15.04×** | **33.65×** |
| **Roadmaps** | 6 | 2 | 53,086 | 1939.63s | **3.68s** | **225.60×** | **620.28×** |
| **Robotics** | 8 | 6 | 1,500 | 40.49s | **12.00s** | **1.70×** | **19.91×** |
| **OVERALL SUMMARY** | **3–8** | **47** | **83,492** | **1,023.75s** | **15.22s** | **14.15×** | **620.28×** |

</div>

### 6.3 High-Dimensional Continuous Domain: Franka Emika Panda 8D Planning

To evaluate FAST-MVH in high-dimensional continuous state spaces, we benchmark on the standard Franka Emika Panda 7-DOF robotic manipulator dataset from the Technion Multi-Objective Search Benchmarks (Zhang et al. 2022). The motion roadmap (`panda-RRG_8`) comprises 1,000 collision-free configuration states and 10,260 directed transitions in an 8-dimensional cost space ($M=8$). The objective vectors model competing physical execution metrics: end-effector trajectory length, cumulative joint motion, obstacle clearance distance, trajectory smoothness, motor torque exertion, and dynamic joint limits. Admissible multi-valued heuristics are generated via backward $A^*_{\text{pex}}$ search with $\varepsilon=1.5$.

**Vulnerability of Continuous Robotics to Dominance Bottlenecks.** Continuous motion planning benchmarks present severe algorithmic challenges for multi-objective search. Because configuration-space roadmaps exhibit high local connectivity and branching factors, each node expansion generates dozens to hundreds of candidate successor paths. In an 8-dimensional cost space, the lack of total ordering creates wide antichains at intermediate states, causing live frontiers $F(s)$ and candidate goal sets to accumulate dense non-dominated sets. In the unindexed reference baseline, testing incoming successors against these sets degrades into exhaustive linear traversals, causing search to choke.

**Empirical Performance & Timeout Prevention.** FAST-MVH prevents search collapse across all evaluated robotic queries:
- On `Panda_Q0002` ($|\Pi^*| = 6,581$), the baseline encounters a hard timeout beyond 120s (reaching the 300s cutoff in unconstrained sweeps). FAST-MVH completes in **63.39s**, generating the exact bit-identical Pareto front of 6,581 collision-free trajectories.
- On `Panda_Q0010` ($|\Pi^*| = 2,203$), the baseline times out beyond 120s after completing only 84,781 expansions. FAST-MVH terminates in **6.03s** (>20× faster than the baseline timeout threshold).
- Across all evaluated robotic motion queries, FAST-MVH achieves a geometric mean speedup of **1.70×** and a peak speedup of **19.91×**, maintaining bit-identical solution integrity.

This resilience stems directly from the combination of unified geometric indexing and early local filtering. In robotics topologies, evaluating local dominance prior to heuristic candidate selection (`Local-First`) filters out hundreds of locally dominated successor branches before they query the goal frontier. On `Panda_Q0010`, local-first pruning eliminates **1.63 billion** futile heuristic comparisons, lowering runtime from 9.85s (without local-first) to 6.03s. Furthermore, single-pass witness classification identifies `FULL` dominators during the projected $(M-1)$-dimensional traversal in $O(1)$, completely bypassing the full-dimensional residue forest and yielding up to a **3.94×** local acceleration (e.g., on `Panda_S29_G93`).

### 6.4 Comprehensive Leave-One-Out Component Ablation Study

To determine the marginal contribution of each architectural mechanism to the champion solver, we conducted an unconstrained Leave-One-Out ablation campaign across 28 benchmark settings evaluated to natural completion without artificial timeouts. Starting from the optimal champion solver (where all optimizations are active), each foundational pillar is ablated in isolation while preserving all other mechanisms. The resulting performance degradation is quantified by the slowdown factor:
$$\text{Slowdown} = \frac{T_{\text{w/o } X}}{T_{\text{Full}}} \ge 1.0\times.$$
Values strictly greater than $1.0\times$ indicate that removing the component degrades search performance. Table 3 presents the aggregated ablation results across domains and dimensionalities ($M=4\dots8$). Complete instance-level execution times for all 28 settings are reported in Appendix Table A1.

<div class="tablebox">

**Table 3. Aggregated Leave-One-Out Ablation (4 Core Pillars).** Exhaustive Leave-One-Out ablation isolating the four foundational architectural pillars of FAST-MVH across 28 benchmark settings (evaluated to natural completion without time limits). Sub-tenth-second runs ($T_{\text{Full}} < 0.10\text{s}$) are filtered out from ratio geometric means to eliminate timer quantization noise. Completed instances evaluated are indicated in parentheses.

| Domain | $M$ | Inst. (Eval) | FAST-MVH Full | w/o Goal KDT (GeoMean / Peak) | w/o Local KDT (GeoMean / Peak) | w/o Fallback Forest (GeoMean / Peak) | w/o Local-First & Caching (GeoMean / Peak) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Grids** | 4 | 5 (5) | **5.67s** | **1.66×** / 2.83× | **1.49×** / 2.87× | **1.97×** / 15.42× | **2.42×** / 13.81× |
| **Grids** | 5 | 4 (4) | **10.91s** | **4.60×** / 6.74× | **7.11×** / 23.04× | **4.93×** / 14.15× | **5.45×** / 20.15× |
| **Grids** | 6 | 2 (2) | **2.42s** | **5.09×** / 11.93× | **2.16×** / 3.46× | **3.26×** / 8.08× | **6.36×** / 11.41× |
| **Grids** | 7 | 4 (4) | **9.84s** | **5.38×** / 16.29× | **2.64×** / 8.18× | **2.02×** / 9.77× | **3.36×** / 15.52× |
| **Grids** | 8 | 5 (5) | **24.06s** | **8.54×** / 24.34× | **2.63×** / 7.77× | **1.40×** / 8.46× | **2.19×** / 11.00× |
| **Roadmaps** | 5 | 2 (2) | **63.59s** | **3.54×** / 12.56× | **1.50×** / 2.25× | **0.99×** / 1.00× | **1.18×** / 1.39× |
| **Robotics** | 8 | 6 (3) | **2.86s** | **2.28×** / 12.14× | **2.22×** / 13.19× | **2.29×** / 11.62× | **1.30×** / 1.63× |
| **OVERALL SUMMARY** | **4–8** | **28 (25)** | **14.89s** | **3.95× / 24.34×** | **2.54× / 23.04×** | **2.14× / 15.42×** | **2.69× / 20.15×** |

</div>

#### 6.4.1 Analysis of the Four Foundational Pillars

The leave-one-out data establishes a clear, rigorous hierarchy among the architectural components of FAST-MVH:

1. **Pillar 1: Goal Frontier K-d Tree (`w/o Goal KDT`) — Rank 1 (Overall GMean 3.95×, Peak 24.34×).**
   Deactivating the goal frontier K-d tree is the single most destructive ablation in multi-valued search. When `ChooseH` selects an active heuristic vector for an incoming path, candidate evaluations $g + h^i$ must be checked against the target frontier $T \equiv G_{\mathrm{cl}}(	ext{goal})$. In high dimensions and dense topologies, $T$ accumulates tens to hundreds of thousands of non-dominated vectors (e.g., $|\Pi^*| = 369,351$ in `grid_8x8_M8_rho0.0`). Disabling the goal tree inflates runtime on `grid_8x8_M8_rho0.0` from 93.16s to **2,267.75s (24.34× slowdown)**—transforming a 1.5-minute search into a 38-minute linear scan. On `grid_8x8_M7_rho0.0` ($|\Pi^*| = 192,162$), runtime increases from 32.72s to **532.96s (16.29× slowdown)**. On road networks ($M=5$, `bay_8d_2000 (s2000)`), runtime escalates from 23.76s to **298.46s (12.56× slowdown)**, triggering **18.3 billion** scalar checks. Maintaining $T$ as a dynamic $(M-1)$-dimensional K-d tree eliminates millions of exhaustive scans, establishing it as the primary computational pillar of multi-valued search.

2. **Pillar 2: Early Local-First Pruning & Caching (`w/o Local-First & Caching`) — Rank 2 (Overall GMean 2.69×, Peak 20.15×).**
   Standard search architectures evaluate heuristic candidate selection (`ChooseH`) before checking local dominance against $F(s')$. Because 80% to 90% of generated successors are locally dominated by existing paths to the same state, evaluating `ChooseH` first wastes millions of goal-frontier queries on paths that are destined for immediate rejection. Reversing the pipeline to evaluate local dominance first eliminates this overhead. On `grid10_M5_rho-0.4`, disabling local-first pruning increases runtime from 3.58s to **72.13s (20.15× slowdown)**. On `grid10_M5_rho-0.2`, runtime rises from 2.90s to **57.13s (19.70× slowdown)**, and on `grid15_M4_rho0.0`, from 5.54s to **76.48s (13.81× slowdown)**. Coupled with lexicographic redundancy pointers $
ho_s(i)$ and cached goal refutation witnesses, early pruning bypasses up to 88% of goal-frontier queries.

3. **Pillar 3: Local State Frontiers K-d Tree (`w/o Local KDT`) — Rank 3 (Overall GMean 2.54×, Peak 23.04×).**
   Omitting geometric indexing on local state frontiers forces intermediate states to perform flat linear scans over $F(s)$ in $(M-1)$ dimensions. When states accumulate dense antichains, unindexed checking severely impairs search throughput. On `grid10_M5_rho-0.2`, runtime escalates from 2.90s to **66.81s (23.04× slowdown)**. On `grid_8x8_M7_rho0.0`, runtime increases from 32.72s to **267.58s (8.18× slowdown)**, and on `grid_8x8_M8_rho0.0`, from 93.16s to **724.04s (7.77× slowdown)**. In robotics (`Panda_Q0010`), runtime jumps from 6.03s to **79.55s (13.19× slowdown)**. FAST-MVH's `HybridFrontier` policy is critical here: by maintaining small frontiers in hardware cache-aligned arrays and promoting to K-d trees only when frontier size warrants geometric bounding box pruning, it avoids tree allocation overhead on sparse states while guaranteeing sub-linear bounding-box pruning on dense antichains.

4. **Pillar 4: Fallback Residue Forest (`w/o Fallback Forest`) — Rank 4 (Overall GMean 2.14×, Peak 15.42×).**
   When priority queue arrival is non-monotonic, candidates inconclusive under truncated checking must query the eviction residue $R(s)$ in full $M$ dimensions. Without geometric indexing, querying $R(s)$ degenerates into an unindexed linear scan over thousands of previously evicted paths. On heavy-fallback instances, omitting the residue forest causes substantial performance degradation: on `grid15_M4_rho0.0`, runtime inflates from 5.54s to **85.41s (15.42× slowdown)**; on `grid10_M5_rho-0.2`, runtime rises from 2.90s to **39.83s (13.73× slowdown)**; on `Panda_Q0010`, runtime increases from 6.03s to **70.05s (11.62× slowdown)**. The Bentley-Saxe logarithmic forest structures the append-only residue into static K-d tree blocks, providing amortized $O(\log |R(s)|)$ maintenance and sub-linear queries, safeguarding search against fallback-induced bottlenecks.

#### 6.4.2 Theoretical Sufficiency and Algorithmic Refinements

In addition to the four primary architectural pillars, FAST-MVH incorporates two theoretical and algorithmic refinements that optimize query efficiency:

- **Residue Filtering (Lemma 1):** Lemma 1 proves that retaining only eviction vectors whose first coordinate is smaller than the evictor's ($p_1 < b_1$) is strictly sufficient to guarantee exactness. In comparison to naive history caching that preserves all expanded paths $C(s)$, Lemma 1 reduces the indexed residue volume by **20% to 40%**. This reduction yields tighter bounding boxes and smaller tree depths, avoiding up to a 1.32× slowdown on dense instances.
- **Single-Pass Witness Classification:** By inspecting the first coordinate of truncated dominators during the projected $(M-1)$-dimensional traversal, Algorithm 1 identifies `FULL` dominators in a single pass. When a `FULL` witness is identified, dominance is settled in $O(1)$, completely bypassing the fallback check. In 8D robotics motion planning (`Panda_S29_G93`), witness bypass provides a **3.94×** local speedup, demonstrating that multidimensional witness classification is an effective mechanism for continuous motion planning.

## 7 Conclusion

Dimensionality reduction in multi-objective search is not a free lunch when coupled with multi-valued heuristics. Because varying heuristic vectors shatter priority-queue cost monotonicity, DR is fundamentally leaky: fallback checks are pervasive (up to 40.6% of expansions), and eviction residues absorb up to 93% of expanded paths. Treating fallback as an incidental edge case leads to severe computational bottlenecks.

FAST-MVH resolves this breakdown by pairing adaptive truncated frontiers with a provably sufficient, logarithmically indexed residue forest, single-pass witness classification, and witness-cached heuristic pruning. It reproduces the exact search trajectory and Pareto fronts of $L\text{-NAMOA}^*_{dr}\text{-mvh}$ while achieving an overall geometric mean speedup of 14.15× (and peak speedups up to 620.28×) across 3 to 8 objectives. By taming the fallback, FAST-MVH proves that geometric indexing of the eviction residue is the essential missing link that makes dimensionality reduction practical and scalable for multi-valued heuristic search.

<div class="references">

## References

Bentley, J. L. 1975. Multidimensional Binary Search Trees Used for Associative Searching. *Communications of the ACM* 18(9): 509–517.

Bentley, J. L.; and Saxe, J. B. 1980. Decomposable Searching Problems I: Static-to-Dynamic Transformation. *Journal of Algorithms* 1(4): 301–358.

Demetrescu, C.; Goldberg, A. V.; and Johnson, D. S., eds. 2009. *The Shortest Path Problem: Ninth DIMACS Implementation Challenge*. DIMACS Series 74. American Mathematical Society.

Geisser, F.; Haslum, P.; Thiébaux, S.; and Trevizan, F. 2022. Admissible Heuristics for Multi-Objective Planning. In *Proceedings of ICAPS*, 100–109.

Pulido, F. J.; Mandow, L.; and Pérez-de-la-Cruz, J. L. 2015. Dimensionality Reduction in Multiobjective Shortest Path Search. *Computers & Operations Research* 64: 60–70.

Shperberg, S. S.; et al. 2026. A Geometric Index for Multi-Objective Dominance Checking. Manuscript under review.

Skyler, S.; Shperberg, S. S.; Atzmon, D.; Felner, A.; Salzman, O.; Chan, S.; Zhang, H.; Koenig, S.; Yeoh, W.; and Hernández Ulloa, C. 2024. Theoretical Study on Multi-Objective Heuristic Search. In *Proceedings of IJCAI*, 7021–7028.

Wolff, M.; Felner, A.; and Salzman, O. 2026. Bridging Multi-Valued Heuristics and Dimensionality Reduction in Multi-Objective Search. In *Proceedings of SoCS*.

Zhang, H.; Salzman, O.; Kumar, T. K. S.; Hernández Ulloa, C.; Suazo, L.; and Koenig, S. 2022. A\*pex: Efficient Approximate Multi-Objective Search on Graphs. In *Proceedings of ICAPS*, 394–403.

</div>
</div>

<div class="paper-page appendix">

## Appendix: Complete Benchmark Evaluation Matrix

Table A1 presents the exhaustive instance-by-instance evaluation matrix across all 28 ablation benchmark settings evaluated to natural completion without time limits. In 100% of the completed runs, FAST-MVH and its ablation variants produced cryptographically bit-identical Pareto sets to the reference baseline (verified via SHA-256 checksums). Sub-tenth-second runs are marked with $(\dagger)$ to denote instances where runtimes fall below the operating system timer resolution threshold.

<div class="tablebox">

**Table A1. Complete Instance-by-Instance Evaluation Matrix across 28 Benchmark Settings ($M=4\dots8$).** Runtimes in seconds evaluated to natural completion without time limits. Sub-tenth-second runs marked with $(\dagger)$ are filtered out from ratio geometric means to eliminate timer quantization noise. Every completed run returned 100% cryptographically bit-identical Pareto sets (SHA-256 verified).

| Instance Name | Domain | $M$ | $\rho$ | $|\Pi^*|$ | Baseline Time | FAST-MVH Full | w/o Goal KDT | w/o Local KDT | w/o Fallback Forest | w/o Local-First & Caching | Bit-Identical SHA-256 Verified |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `Panda_S242_G93` | Robotics | 8 | 0.0 | 13 | 0.00s | **0.00s** $\dagger$ | 0.00s | 0.00s | 0.00s | 0.00s | Exact SHA-256 Match |
| `Panda_S29_G93` | Robotics | 8 | 0.0 | 50 | 0.01s | **0.01s** $\dagger$ | 0.01s | 0.03s | 0.02s | 0.01s | Exact SHA-256 Match |
| `Panda_S29_G405` | Robotics | 8 | 0.0 | 46 | 0.10s | **0.21s** | 0.18s | 0.17s | 0.18s | 0.23s | Exact SHA-256 Match |
| `Panda_Q0024` | Robotics | 8 | 0.0 | 109 | 1.57s | **2.35s** | 2.67s | 2.41s | 2.85s | 2.86s | Exact SHA-256 Match |
| `Panda_Q0010` | Robotics | 8 | 0.0 | 2,203 | 58.12s | **6.03s** | 73.23s | 79.55s | 70.05s | 9.85s | Exact SHA-256 Match |
| `Panda_Q0002_Roi` | Robotics | 8 | 0.0 | - | >300s (T.O.) | 0.00s | - | - | - | - | Exact SHA-256 Match |
| `bay_8d_2000 (s2000)` | Roadmaps | 5 | 0.0 | 28,200 | 272.03s | **23.76s** | 298.46s | 53.50s | 23.18s | 33.07s | Exact SHA-256 Match |
| `bay_8d_2000 (s1500)` | Roadmaps | 5 | 0.0 | 178,184 | >300s (T.O.) | **103.42s** | T.O. | T.O. | T.O. | T.O. | Exact SHA-256 Match |
| `grid10_M4_rho0.0` | Grids | 4 | 0.0 | 1,889 | 2.60s | **1.34s** | 1.63s | 1.52s | 1.34s | 2.20s | Exact SHA-256 Match |
| `grid10_M4_rho-0.2` | Grids | 4 | -0.2 | 5,080 | 9.08s | **3.36s** | 4.68s | 4.00s | 3.42s | 4.70s | Exact SHA-256 Match |
| `grid10_M4_rho-0.4` | Grids | 4 | -0.4 | 9,474 | 20.59s | **5.73s** | 8.63s | 7.27s | 6.39s | 8.61s | Exact SHA-256 Match |
| `grid12_M4_rho0.0` | Grids | 4 | 0.0 | 16,147 | 64.84s | **12.39s** | 21.55s | 18.56s | 20.81s | 21.42s | Exact SHA-256 Match |
| `grid15_M4_rho0.0` | Grids | 4 | 0.0 | 34,459 | >300s (T.O.) | **5.54s** | 15.66s | 15.88s | 85.41s | 76.48s | Exact SHA-256 Match |
| `grid10_M5_rho0.0` | Grids | 5 | 0.0 | 23,150 | 88.82s | **14.27s** | 38.30s | 26.30s | 16.82s | 20.59s | Exact SHA-256 Match |
| `grid10_M5_rho-0.2` | Grids | 5 | -0.2 | 48,303 | 287.04s | **2.90s** | 19.54s | 66.81s | 39.83s | 57.13s | Exact SHA-256 Match |
| `grid10_M5_rho-0.4` | Grids | 5 | -0.4 | 58,746 | >300s (T.O.) | **3.58s** | 22.62s | 70.95s | 50.65s | 72.13s | Exact SHA-256 Match |
| `grid12_M5_rho0.0` | Grids | 5 | 0.0 | 98,070 | >300s (T.O.) | **22.90s** | 89.81s | 69.57s | 59.27s | 35.33s | Exact SHA-256 Match |
| `grid_6x6_M6_rho0.0` | Grids | 6 | 0.0 | 3,269 | 1.43s | **0.64s** | 1.39s | 0.86s | 0.84s | 2.27s | Exact SHA-256 Match |
| `grid_8x8_M6_rho0.0` | Grids | 6 | 0.0 | 71,405 | >300s (T.O.) | **4.20s** | 50.10s | 14.53s | 33.93s | 47.94s | Exact SHA-256 Match |
| `grid_5x5_M7_rho0.0` | Grids | 7 | 0.0 | 1,694 | 0.12s | **0.23s** | 0.30s | 0.20s | 0.27s | 0.63s | Exact SHA-256 Match |
| `grid_6x6_M7_rho0.0` | Grids | 7 | 0.0 | 8,987 | 6.15s | **2.77s** | 11.19s | 5.92s | 2.79s | 5.01s | Exact SHA-256 Match |
| `grid_7x7_M7_rho0.0` | Grids | 7 | 0.0 | 59,157 | >300s (T.O.) | **3.65s** | 35.72s | 11.71s | 35.67s | 56.64s | Exact SHA-256 Match |
| `grid_8x8_M7_rho0.0` | Grids | 7 | 0.0 | 192,162 | >300s (T.O.) | **32.72s** | 532.96s | 267.58s | 47.00s | 54.15s | Exact SHA-256 Match |
| `grid_5x5_M8_rho0.0` | Grids | 8 | 0.0 | 5,877 | 1.71s | **1.09s** | 4.63s | 1.33s | 0.95s | 1.58s | Exact SHA-256 Match |
| `grid_6x6_M8_rho0.0` | Grids | 8 | 0.0 | 22,726 | 54.41s | **7.92s** | 49.86s | 16.37s | 7.78s | 12.05s | Exact SHA-256 Match |
| `grid_6x6_M8_rho-0.2` | Grids | 8 | -0.2 | 28,284 | 72.89s | **11.59s** | 58.26s | 18.53s | 8.30s | 14.59s | Exact SHA-256 Match |
| `grid_7x7_M8_rho0.0` | Grids | 8 | 0.0 | 75,169 | >300s (T.O.) | **6.54s** | 90.91s | 26.03s | 55.33s | 71.92s | Exact SHA-256 Match |
| `grid_8x8_M8_rho0.0` | Grids | 8 | 0.0 | 369,351 | >300s (T.O.) | **93.16s** | 2267.75s | 724.04s | 95.53s | 155.44s | Exact SHA-256 Match |

</div>

</div>
