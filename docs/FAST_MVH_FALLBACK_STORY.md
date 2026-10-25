# The MVH Fallback Story: A Step-by-Step Algorithmic Trace

## Abstract & Overview

In Multi-Objective Search (MOS) using Lexicographical $A^*$ ($L\text{-NAMOA}^*_{dr}\text{-mvh}$ by Maya Wohlf), **Dimensionality Reduction ($t$-discarding)** omits the primary objective $g_1$ to accelerate local dominance checks against the closed set. It checks only the truncated cost vector $\operatorname{Tr}(g) = (g_2, \dots, g_M)$.

When combining Dimensionality Reduction with **Multi-Value Heuristics (MVH)**, nodes arrive with different heuristic estimates from $H(s)$. Because the priority queue $\text{OPEN}$ orders nodes strictly by $f_1 = g_1 + h_1$, this introduces an **Order Violation**: an inferior past path ($g_1 = 10$) can be popped *before* a superior candidate path ($g_1 = 8$).

Without a safety guard, the truncated check suffers from **False Dominance**: it falsely assumes the past path dominates the candidate, illegally pruning a Pareto-optimal solution. Maya's algorithm detects this through the monotonicity condition ($\max g'_1 \le g_1$). When this fails, the algorithm triggers a **Fallback** to a full $M$-dimensional Pareto check.

In **FAST2**, this fallback was scanned linearly over an unindexed `std::vector`, generating over 1.35 billion comparisons and consuming 85%+ of execution time. In **FAST3**, `ArenaGcl` uses a full $M$-dimensional KD-Tree (`FullKDTree<M>`) with bounding box pruning, reducing query complexity from $O(N)$ to $O(\log N)$ and unlocking up to $8.97\times$ speedups.

---

## 1. Concrete Numerical Scenario ($M=2$)

To make the geometric and algorithmic principles crystal clear, consider a 2-objective search ($M=2$):
* Primary objective: $g_1$ (cost / omitted in $\operatorname{Tr}$).
* Truncated objective: $g_2$ (time / $\operatorname{Tr}(g) = g_2$).
* $\text{OPEN}$ Min-Heap key: $f_1 = g_1 + h_1$.

### Search State at Node $s$:
1. **Known Goal Solution:** A complete path previously reached the destination with cost:
   $$T = G_{cl}^{\operatorname{Tr}}(s_{\text{goal}}) = \{6\} \quad (\text{in } g_2)$$
2. **Candidate Heuristics at State $s$:**
   $$H(s) = \left[ h^{(1)} = (1, 3), \quad h^{(2)} = (5, 1) \right]$$
   Sorted ascending by $h_1$: $1 < 5$.

---

## 2. Step-by-Step Algorithmic Execution Trace

Below is the line-by-line debugger trace matching Maya Wohlf's algorithm ($L\text{-NAMOA}^*_{dr}\text{-mvh}$ Algorithm 2).

### Step 1: Generation & Evaluation of Path 1
* **Arrival:** Path 1 arrives at state $s$ with cost $g^{(1)} = (10, 2)$.
* **`CHOOSEH` Execution:**
  * Tests $h^{(1)} = (1, 3)$: $f = (10+1, 2+3) = (11, 5)$. Truncated cost: $\operatorname{Tr}(f) = 5$.
  * Dominance test vs Goal: Does $T \preceq \operatorname{Tr}(f)$? Is $6 \le 5$? **False!** (Goal does not dominate).
  * $h^{(1)}$ is **Accepted**!
* **Enqueue:** Path 1 enters $\text{OPEN}$ with $f^{(1)} = (11, 5)$ and priority key **$f_1 = 11$**.

### Step 2: Generation & Evaluation of Path 2 (Heuristic Pruned!)
* **Arrival:** Path 2 arrives at state $s$ with cost $g^{(2)} = (8, 4)$.
  *(Notice: Path 2 is superior in $g_1$ [$8 < 10$], but worse in $g_2$ [$4 > 2$]).*
* **`CHOOSEH` Execution:**
  * Tests $h^{(1)} = (1, 3)$ first: $f = (8+1, 4+3) = (9, 7)$. Truncated cost: $\operatorname{Tr}(f) = 7$.
  * Dominance test vs Goal: Does $T \preceq \operatorname{Tr}(f)$? Is $6 \le 7$? **True! (Dominated by Goal!)**
  * **$h^{(1)}$ is Pruned and Discarded!** Completion via $h^{(1)}$ is guaranteed inferior to a known goal solution.
  * Forced to test $h^{(2)} = (5, 1)$: $f = (8+5, 4+1) = (13, 5)$. Truncated cost: $\operatorname{Tr}(f) = 5$.
  * Dominance test vs Goal: Is $6 \le 5$? **False! (Survives).**
  * $h^{(2)}$ is **Accepted**!
* **Enqueue:** Path 2 enters $\text{OPEN}$ with $f^{(2)} = (13, 5)$ and priority key **$f_1 = 13$**.

```
State of OPEN Heap:
┌────────────────────────────────────────────────────────┐
│ Top:   Path 1 | g=(10, 2) | h=(1, 3) | Key f1 = 11      │
│ Next:  Path 2 | g=(8,  4) | h=(5, 1) | Key f1 = 13      │
└────────────────────────────────────────────────────────┘
```

---

### Step 3: Popping Path 1 & The Origin of Order Violation
* **Queue Pop:** $\text{OPEN}$ pops by minimum $f_1$. Because **$11 < 13$**, Path 1 is popped first!
* **Commit to State $s$:**
  * $G_{cl}(s) = \{(10, 2)\}$
  * $G_{cl}^{\operatorname{Tr}}(s) = \{2\}$
  * Register update: $\max g_1(s) = 10$.

> [!NOTE] The MVH Paradox
> Path 1 was popped *before* Path 2 because its estimated total cost was smaller ($f_1 = 11 < 13$), even though its **actual accumulated cost is worse** ($g_1 = 10 > 8$).

---

### Step 4: Popping Path 2 & The Truncated False Dominance Trap
* **Queue Pop:** Path 2 is popped next ($f_1 = 13$) and arrives at state $s$ with $g = (8, 4)$.
* **Order Violation:**
  The arrival sequence of $g_1$ at state $s$ dropped from $10 \to 8$. Monotonicity has collapsed!
* **`LOCALDOMCHECK` Step 1 (Truncated Check):**
  * Does $\operatorname{Tr}(g^{(1)}) \le \operatorname{Tr}(g^{(2)})$?
  * Substitute: Is $2 \le 4$? $\implies$ **True!**
* **The Danger:**
  Looking only at $g_2$, Path 1 *falsely appears to dominate* Path 2. If the algorithm pruned here, Path 2 would be illegally lost!

---

### Step 5: The Monotonicity Safety Guard & Fallback Trigger
* **`LOCALDOMCHECK` Step 2 (Monotonicity Check, Maya Line 43):**
  $$\max_{g' \in G_{cl}(s)} g'_1 \le g_1^{(2)} \iff 10 \le 8$$
* **Result:** **False! (Order Violation Detected!)**
* **Consequence:**
  The truncated check is declared **untrustworthy and invalid**. The algorithm aborts the truncated shortcut and initiates **Fallback**.

---

### Step 6: Fallback Executed — Full Pareto Check Saves Path 2!
* **`LOCALDOMCHECK` Step 3 (Full Fallback, Maya Line 45):**
  Compare $(10, 2)$ vs $(8, 4)$ on all $M=2$ dimensions:
  * Dim 1: Is $10 \le 8$? $\implies$ **False!** (Path 2 is better by 2 units)
  * Dim 2: Is $2 \le 4$? $\implies$ **True!** (Path 1 is better by 2 units)
* **Verdict:** **Neither dominates!**
  Path 2 is a legitimate Pareto trade-off. It **survives** and is committed to:
  $$G_{cl}(s) = \{(10, 2), (8, 4)\}$$
* **Outcome:** Exact Pareto optimality is preserved bit-identically.

---

## 3. The Engineering Bottleneck in FAST2 vs FAST3 Resolution

| Feature / Metric | FAST2 (V5 Architecture) | FAST3 (ArenaGcl Architecture) | Impact & Rationale |
| :--- | :--- | :--- | :--- |
| **$G_{cl}(s)$ Storage** | Flat `std::vector<CostVector>` | Balanced `FullKDTree<M>` (chunked) | Flat array vs Spatial index |
| **Non-dominated Fallback Rate** | **98.5%** of all fallbacks | **98.5%** of all fallbacks | Empirical profiling finding |
| **Early Break Potential** | **0%** (must scan all past paths) | Prunes subtrees in $O(1)$ | Non-dominance requires proving NO past path dominates |
| **Query Complexity** | $O(N)$ linear scan | $O(\log N)$ spatial branch rejection | $lo[k] > q[k]$ bounding box test |
| **`cmp_full` Count (grid_n7_m8)**| **1,350,000,000** comparisons | **6,200,000** comparisons | **$217\times$ fewer operations** |
| **Runtime (grid_8x8_m6)** | 382.14 seconds | 42.61 seconds | **$8.97\times$ wall-clock speedup** |

---

## 4. Deliverable Files Available in Workspace

1. **Interactive Standalone Presentation (Offline HTML):**
   * Path: [`presentations/FAST_MVH_Fallback_Story.html`](file:///c:/Users/barf9/Desktop/FAST%20MVH-KDT/presentations/FAST_MVH_Fallback_Story.html)
   * Open directly in any browser (Chrome, Edge, Safari on iPad/Laptop). Contains live canvas, playback slider, Hebrew/English toggle, and code trace.
2. **PowerPoint Slide Deck (.pptx):**
   * Path: [`presentations/FAST_MVH_Fallback_Story.pptx`](file:///c:/Users/barf9/Desktop/FAST%20MVH-KDT/presentations/FAST_MVH_Fallback_Story.pptx)
   * 10 widescreen (16:9) slides with high-resolution plots, memory tables, pseudocode, and speaking notes ready for meetings.
3. **Animated GIF:**
   * Path: [`scratchpad/fallback_mechanism.gif`](file:///c:/Users/barf9/Desktop/FAST%20MVH-KDT/scratchpad/fallback_mechanism.gif)
   * Crisp animated loop through all 7 steps.
