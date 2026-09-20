# System Architecture & C++20 Technical Contract

## 1. Architectural Overview

This project unifies three complementary multi-objective search paradigms into a high-performance C++20 framework:
1. **Maya Wohlf's Algorithmic Backbone** (`baselines/bridging-mvh-dr`): Sound integration of Multi-Valued Heuristics (MVH) with Dimensionality Reduction (DR) via optimistic truncation and fallback validation.
2. **Roi's Geometric Heuristic Selection** (`papers/mvh/Roi_MVH_KDTree_Dominance_Pruning.pdf`): Accelerating `CHOOSEH` from an $\mathcal{O}(|H(s)| \cdot |T|)$ linear bottleneck to a static $(d-1)$-dimensional K-d tree over truncated heuristics $\operatorname{Tr}(H(s))$ with aggregate bounding boxes ($N.\min, N.\max$) and strict lexicographical tie-breaking.
3. **Shahaf's Geometric Dominance Index** (`papers/geometric_indexing/Shahaf_Geometric_Index_Dominance_Checking.pdf`): Cache-optimized K-d tree representation utilizing flat contiguous arrays and arena allocation for $\mathcal{O}(n^{1 - 1/d})$ orthant-emptiness dominance checks.

```
+-------------------------------------------------------------------------------+
|                             L_NAMOA_DR_MVH_KDT                                |
|                                                                               |
|  +---------------------------+             +-------------------------------+  |
|  |    Static MVH K-d Tree    |             |    Dynamic Frontier Index     |  |
|  |  (Roi / Heuristic Index)  |             |     (Shahaf Frontier Tree)    |  |
|  |                           |             |                               |  |
|  | - Indexed over Tr(H(s))   |             | - Indexed over G_Tr_cl / G_cl |  |
|  | - (d-1) dimensions        |             | - Orthant-emptiness checking  |  |
|  | - Bounding boxes min/max  |             | - Contiguous Arena Storage    |  |
|  | - Aggregate prune/accept  |             | - cmpchk / cmpupd tracking    |  |
|  +---------------------------+             +-------------------------------+  |
|                ^                                          ^                   |
|                |                                          |                   |
|         CHOOSEH(s, g, T)                           LOCALDOMCHECK(s, g)        |
+-------------------------------------------------------------------------------+
```

---

## 2. High-Level C++20 Contracts

### 2.1 Static Heuristic K-d Tree (`mvh_kdtree.h`)
Designed for static indexing of $\operatorname{Tr}(H(s))$ per state:

```cpp
namespace fast_mvh {

template <size_t D>
struct BoundingBox {
    std::array<size_t, D> min_corner;
    std::array<size_t, D> max_corner;
    
    [[nodiscard]] constexpr bool is_dominated_by(const std::array<size_t, D>& target) const noexcept;
    [[nodiscard]] constexpr bool dominates(const std::array<size_t, D>& query) const noexcept;
};

struct KDNode {
    uint32_t left_child{UINT32_MAX};
    uint32_t right_child{UINT32_MAX};
    uint32_t first_idx{0};   // Index into flat element store
    uint32_t count{0};       // Number of heuristics in leaf (0 if internal)
    
    // Bounds for coordinates 1 .. d-1
    std::vector<size_t> min_bounds;
    std::vector<size_t> max_bounds;
    
    // Bitset or range of original lexicographical indices in H(s)
    std::vector<uint32_t> heuristic_indices;
    
    [[nodiscard]] bool is_leaf() const noexcept { return count > 0; }
};

class StaticHeuristicKDTree {
public:
    explicit StaticHeuristicKDTree(const std::vector<std::vector<size_t>>& h_vectors, size_t leaf_size = 8);
    
    // Core query matching Algorithm 3 & 4 in Roi's formulation
    // Returns index in original H(s) of lexicographically smallest non-dominated heuristic
    [[nodiscard]] std::optional<std::pair<std::vector<size_t>, size_t>>
    choose_h(const std::vector<size_t>& g,
             const std::vector<std::vector<size_t>>& truncated_target_frontier,
             size_t start_idx = 0) const;

private:
    std::vector<KDNode> nodes_;
    std::vector<std::pair<std::vector<size_t>, size_t>> leaf_heuristics_; // (Tr(h), original_index)
    size_t dim_{0};
    size_t leaf_size_{8};
};

} // namespace fast_mvh
```

### 2.2 Dynamic Frontier Dominance Index (`frontier_kdtree.h`)
Designed for incremental dominance checking (`CHECK`) and set updates (`UPDATE`) against evolving frontiers $G^{\mathrm{Tr}}_{\mathrm{cl}}(s)$:

```cpp
namespace fast_mvh {

class DynamicFrontierKDTree {
public:
    explicit DynamicFrontierKDTree(size_t dim);
    
    // Returns true if query is dominated by any vector in the frontier
    [[nodiscard]] bool check_dominated(const std::vector<size_t>& query,
                                       uint64_t& cmpchk_counter) const;
                                       
    // Inserts candidate and prunes existing vectors dominated by candidate
    void update(const std::vector<size_t>& candidate,
                uint64_t& cmpupd_counter);
                
    [[nodiscard]] size_t size() const noexcept;
    void clear() noexcept;

private:
    // Contiguous array and arena storage following Shahaf's principles
    struct Node {
        std::vector<size_t> point;
        std::vector<size_t> lo; // Subtree corner-minima
        uint32_t left{UINT32_MAX};
        uint32_t right{UINT32_MAX};
        bool deleted{false};
    };
    std::vector<Node> tree_arena_;
    uint32_t root_{UINT32_MAX};
};

} // namespace fast_mvh
```

---

## 3. Integration Plan & Milestones (Derived from Research Notes)

Derived from the project roadmap in `papers/notes/Research_Notes_ReadMe.docx`:

### Phase 1: Baseline Parity & Oracle Infrastructure
- Wrap Maya Wohlf's baseline solver in an automated harness.
- Establish bit-identical Pareto front validation against standard benchmark instances.
- Profile baseline dominance comparison counts (`cmpchk`, `cmpupd`) and timing bottlenecks in `CHOOSEH`.

### Phase 2: Static Heuristic K-d Tree (`CHOOSEH` Acceleration)
- Implement `StaticHeuristicKDTree` over truncated heuristics $\operatorname{Tr}(H(s))$.
- Integrate aggregated pruning rules:
  - Discard if $\operatorname{Tr}(\mathbf{g}) + N.\min$ dominated by $T$.
  - Accept if $\operatorname{Tr}(\mathbf{g}) + N.\max$ not dominated by $T$.
- Validate that `KD-ChooseH` produces the **exact same heuristic index and node order** as Maya's baseline across 100% of benchmark instances.

### Phase 3: Dynamic Frontier Indexing (Shahaf's Principles)
- Implement `DynamicFrontierKDTree` with contiguous memory arena and corner-minima `lo` pruning.
- Evaluate replacing linear scan in `LOCALDOMCHECK` and goal frontier dominance tests.
- Compare dual-tree configurations ($d-1$ for truncated vs. full $d$-dimensional).

### Phase 4: Local Ideal Point Heuristic Exploration
- Explore computing dynamic "local ideal points" $\mathbf{h}^*_{\mathrm{local}} = \min_{\mathbf{h} \in H(s)} \mathbf{h}$ to guide optimistic pruning.
- Measure impact on fallback frequency and expansion count.
