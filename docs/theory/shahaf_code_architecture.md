# Shahaf's Codebase Architecture Deep-Dive

## 1. Executive Summary & Code Location

Shahaf's codebase is located in `baselines/code-appendix/` (accompanying the paper *"A Geometric Index for Multi-Objective Dominance Checking"*). It provides an incremental, lazy-deletion K-d tree frontier (`kdinc`) designed as a drop-in replacement for the lexicographic AVL tree in EMOA*.

### Key Source Files in `baselines/code-appendix/`:
- **Interface**: `include/bench/ifrontier.hpp` (`IFrontier` interface: `Check`, `Update`, `Size`)
- **Concrete Frontier**: `include/bench/kd_inc_frontier.hpp` & `include/bench/kd_inc_frontier_t.hpp`
- **Memory Management**: `include/bench/node_arena.hpp` (`NodeArena<T>` bump allocator)
- **Vector Storage Policies**: `include/bench/cost_store.hpp` (`VecDouble`, `ArrDouble<D>`, `ArrInt<D>`)
- **Comparison & SIMD Predicates**: `include/bench/dom_cmp.hpp` (`CmpNaive`, `CmpFast`, `CmpBranchless`, `CmpSimd` with AVX2)
- **Search Integration**: `include/frontier_kdinc.hpp` (`FrontierKdInc` wrapping `KDIncFrontier`)

---

## 2. Memory Layouts & Data Structures

### 2.1 Node Representation (`Node`)
Each node in the K-d tree (`KDIncFrontierT<Store, Cmp, Arena, WidestAxis, OrderedDescent>`) stores:
```cpp
struct Node {
    int axis;     // Splitting coordinate dimension [0, dim)
    bool dead;    // Lazy tombstone flag (true if point was later dominated)
    Point pt;     // Stored cost vector (d-1 dimensions)
    Point lo;     // Component-wise minimum bounding corner of subtree
    Point hi;     // Component-wise maximum bounding corner of subtree
    Node* l;      // Pointer to left child (or 32-bit index in flat layout)
    Node* r;      // Pointer to right child (or 32-bit index in flat layout)
};
```

### 2.2 Memory Arena Bump Allocator (`NodeArena<T>`)
Rather than relying on `malloc` or `new` for each node, `NodeArena<T>` allocates fixed-size contiguous chunks (default `block = 4096` nodes):
- **Allocation**: Hands out `T*` sequentially via a bump pointer (`_used++`).
- **Reuse**: The `reset()` method rewinds allocation counters without freeing underlying OS memory blocks, eliminating memory churn and fragmentation across search states.
- **Cache Locality**: Tree nodes allocated in contiguous memory blocks enjoy dense spatial locality in CPU L1/L2 cache lines.

### 2.3 Vector Storage Policies (`CostStore`)
- **`VecDouble`**: Dynamic heap vector `std::vector<double>` (baseline compatibility).
- **`ArrDouble<D>`**: Contiguous stack/inline array `std::array<double, D>` (zero heap overhead, enabling trivially destructible nodes in `NodeArena`).
- **`ArrInt<D>`**: Contiguous integer array `std::array<int32_t, D>` for integer edge-cost graphs (cutting vector memory bandwidth in half).

---

## 3. Bounding Box Mechanics: Corner-Minima (`lo`) & Corner-Maxima (`hi`)

Every node $n$ maintains axis-aligned bounding boxes covering all points in its subtree:
$$\mathbf{lo}_n[d] = \min_{\mathbf{p} \in \operatorname{Subtree}(n)} p_d, \quad \mathbf{hi}_n[d] = \max_{\mathbf{p} \in \operatorname{Subtree}(n)} p_d$$

### Invariant Maintenance During Incremental Insertion
When a new point $\mathbf{g}$ is inserted into the tree:
1. Walk down the tree following coordinate comparisons `gp[axis] < n->pt[axis] ? left : right`.
2. Expand bounding box corners along the descent path:
   ```cpp
   for (size_t d = 0; d < _dim; ++d) {
       if (gp[d] < n->lo[d]) n->lo[d] = gp[d];
       if (gp[d] > n->hi[d]) n->hi[d] = gp[d];
   }
   ```
3. Attach $\mathbf{g}$ as a new leaf when a `null` child is encountered.

---

## 4. Query & Dominance Operations

### 4.1 Dominance Check Query (`Check(g)` / `_query(n, g)`)
Tests whether any live point in the tree weakly dominates the query vector $\mathbf{g}$ ($\exists \mathbf{p} \in \operatorname{Frontier} \text{ s.t. } \mathbf{p} \preceq \mathbf{g}$):

```cpp
bool _query(Node* n, const Point& g) const {
    if (!n) return false;
    
    // 1. O(1) Subtree Prune via Corner-Minimum (lo):
    // If subtree's minimum exceeds g on any coordinate, NO point in subtree can dominate g.
    for (size_t d = 0; d < _dim; ++d) {
        if (n->lo[d] > g[d]) return false;
    }
    
    // 2. Point Dominance Evaluation:
    if (!n->dead && Cmp::dominates(n->pt, g, _d)) {
        return true; // Early-exit on first dominator found
    }
    
    // 3. Child Traversal (Optionally Ordered Descent by smaller lo-corner):
    if (_query(n->l, g)) return true;
    return _query(n->r, g);
}
```

### 4.2 Lazy Deletion & Tombstoning (`Update(g)` / `_markDominated(n, gp)`)
When an undominated path vector $\mathbf{g}$ is added to the frontier, existing stored points dominated by $\mathbf{g}$ must be removed:
1. **Subtree Prune via Corner-Maximum (`hi`)**:
   If $\mathbf{hi}_n[d] < \mathbf{g}[d]$ for any coordinate $d$, no point in the subtree is $\ge \mathbf{g}[d]$, so none can be dominated by $\mathbf{g}$. The entire subtree is skipped in $\mathcal{O}(1)$.
2. **Tombstone**: If $\mathbf{g} \preceq n.\mathbf{pt}$, mark $n.\mathrm{dead} = \mathrm{true}$ and decrement `_live`. Bounding boxes ($\mathbf{lo}, \mathbf{hi}$) remain valid without expensive restructuring.

### 4.3 Amortized Rebuild (`_rebuild()`)
Tombstoned nodes and tree imbalance are corrected via amortized bulk rebuilding:
- **Trigger**: When total nodes exceed `REBUILD_FACTOR * _built + REBUILD_SLACK` (default factor 2).
- **Bulk Build**: Collects live nodes in $\mathcal{O}(n)$, resets the `NodeArena`, and constructs a balanced tree using `std::nth_element` median partitioning in $\mathcal{O}(n \log n)$.
- **Amortized Cost**: $\mathcal{O}(\log n)$ per update.

---

## 5. Architectural Takeaways for `fast_mvh`

1. **Exact Dominance Equivalency**: Shahaf's `kdinc` frontier produces the exact same decision as linear scans (`Check(g)` returns true $\iff$ some point dominates $g$), making it a drop-in replacement for Maya's closed set frontiers.
2. **Dual-Role Applicability**:
   - **Role A (Frontier Maintenance)**: Replace Maya's `std::vector<std::vector<size_t>> truncated_non_dominated_g` with an incremental `KDIncFrontier` to accelerate `LOCALDOMCHECK`.
   - **Role B (Heuristic Selection)**: Adapt the bounding box principles ($N.\min, N.\max$) to the static heuristic tree for `CHOOSEH` (Roi's formulation).
