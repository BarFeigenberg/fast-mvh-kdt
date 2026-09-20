# Geometric Indexing for Dominance Checking (Shahaf's Principles)

## 1. Theoretical Lower Bounds and Orthant-Emptiness

In multi-objective optimization with $M$ objectives, checking whether a newly generated candidate vector $\mathbf{g}$ is dominated by any vector in an antichain frontier $F$ of size $n$ corresponds to an **orthant-emptiness query**:
Does the dominance orthant $\mathcal{O}_{\le}(\mathbf{g}) = \{\mathbf{x} \in \mathbb{R}^M \mid \mathbf{x} \preceq \mathbf{g}\}$ contain any point of $F$?

### Asymptotic Separation: Total-Order vs. Geometric Indexing
- **Single-coordinate / Lexicographical Search Trees**:
  For $M \ge 4$, any total-order index (such as the lexicographically sorted AVL tree or balanced BST used in BOA* and EMOA*) must in the worst case perform $\Omega(n)$ dominance tests over an antichain of size $n$. Single-coordinate orderings cannot bound dominance in more than 3 dimensions without degenerating to a full scan.
- **Geometric Index (K-d Tree)**:
  By partitioning spatial coordinates across $d = M - 1$ dimensions simultaneously, a geometric index answers orthant-emptiness queries in:
  $$\mathcal{O}\left(n^{1 - \frac{1}{d}}\right)$$
  Dominance comparison counts empirically scale as $n^\alpha$ with $\alpha \in [0.38, 0.84]$ across $d = 2$ to $5$, dramatically below the linear baseline.

---

## 2. Corner-Minima (`lo`) Bounding and Pruning

In Shahaf's K-d tree frontier formulation:
- Each node $u$ stores a stored cost vector $\mathbf{p}_u$ and the **corner-minimum** $\mathbf{lo}_u$ of its entire subtree:
  $$\mathbf{lo}_u = \left(\min_{\mathbf{p} \in \operatorname{Subtree}(u)} p_1, \, \min_{\mathbf{p} \in \operatorname{Subtree}(u)} p_2, \, \dots, \, \min_{\mathbf{p} \in \operatorname{Subtree}(u)} p_d\right)$$
- The vector $\mathbf{lo}_u$ represents the nearest lower corner of the axis-aligned bounding box enclosing all points in the subtree.

### Dominance Check Pruning (`CHECK(g)`)
When querying whether $\exists \mathbf{p} \in \operatorname{Subtree}(u)$ such that $\mathbf{p} \preceq \mathbf{g}$:
- **Pruning Criterion**: If $\mathbf{lo}_u \not\preceq \mathbf{g}$ (i.e., $\exists k$ such that $\mathbf{lo}_{u, k} > g_k$):
  Since $\forall \mathbf{p} \in \operatorname{Subtree}(u), \, p_k \ge \mathbf{lo}_{u, k} > g_k$, no vector in $\operatorname{Subtree}(u)$ can possibly dominate $\mathbf{g}$.
  **The entire subtree is pruned in $\mathcal{O}(1)$ without visiting its children.**
- **Point Evaluation**: If $\mathbf{lo}_u \preceq \mathbf{g}$, the algorithm inspects the node's stored point $\mathbf{p}_u$. If $\mathbf{p}_u \preceq \mathbf{g}$, the query immediately returns `true` (dominated).
- Otherwise, search recurses on children whose bounding boxes intersect the dominance orthant.

---

## 3. High-Performance C++20 Memory Layout & Arena Allocation

Standard pointer-based tree structures (`std::shared_ptr`, heap-allocated individual `struct Node { Node *left, *right; }`) introduce severe performance degradation:
1. **Cache Locality Destruction**: Random heap allocations disperse tree nodes across disparate virtual memory pages. In high-frequency dominance checks running millions of times, pointer chasing results in recurring L1/L2/L3 cache misses.
2. **Allocation Overhead**: Dynamic allocation of small nodes during search overwhelms system allocators (`malloc`/`free`).
3. **Memory Footprint / Pointer Bloat**: 64-bit pointers (16 bytes for left/right children) add 50–100% memory overhead relative to the raw coordinate payload.

### Architectural Solution: Contiguous Flat Array + Memory Arena
Shahaf's principles dictate:
1. **Contiguous Node Pool (Arena Allocator)**:
   - Nodes are allocated sequentially from a pre-allocated chunk of contiguous memory (using `std::pmr::monotonic_buffer_resource` or a flat `std::vector<KDNode>`).
   - Memory reuse across search iterations without free-list fragmentation.
2. **Index-Based Child References**:
   - Replace 64-bit pointers with 32-bit integer indices: `uint32_t left_child`, `uint32_t right_child`, with a sentinel (e.g., `UINT32_MAX`) denoting `null`.
   - Halves pointer footprint and enables direct indexed addressing into the cache-friendly contiguous array.
3. **Structure of Arrays (SoA) vs. Packed Structs**:
   - Coordinate arrays aligned to 16/32-byte boundaries for SIMD vectorization (AVX2/AVX-512) during coordinate comparisons.
   - Separate hot traversal data (bounding boxes $\mathbf{lo}, \mathbf{hi}$, child indices) from cold metadata to maximize cache line utilization during pruning checks.
