#ifndef FAST_MVH_DYNAMIC_FRONTIER_KDTREE_H
#define FAST_MVH_DYNAMIC_FRONTIER_KDTREE_H

#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace fast_mvh {

enum class MemoryBackend {
    HEAP,
    EXPONENTIAL_ARENA,
    VECTOR_ARENA
};

// A trait class to select the default backend. We can change this for benchmarking.
constexpr MemoryBackend DEFAULT_MEMORY_BACKEND = MemoryBackend::VECTOR_ARENA;

template<MemoryBackend Backend = DEFAULT_MEMORY_BACKEND>
class DynamicFrontierKDTree {
public:
    // We expect full d-dimensional vectors. `proj_dim` specifies how many dimensions 
    // to use for the K-d tree (e.g. d-1 for DR). We skip the first element (index 0).
    explicit DynamicFrontierKDTree(size_t proj_dim);
    ~DynamicFrontierKDTree();

    DynamicFrontierKDTree(const DynamicFrontierKDTree&) = delete;
    DynamicFrontierKDTree& operator=(const DynamicFrontierKDTree&) = delete;

    DynamicFrontierKDTree(DynamicFrontierKDTree&&) = default;
    DynamicFrontierKDTree& operator=(DynamicFrontierKDTree&&) = default;

    // Check if `query` is dominated by any vector in the tree
    // Uses dimensions 1 to proj_dim.
    bool check_dominated(const std::vector<size_t>& query, uint64_t& cmpchk_counter) const;

    // Update the frontier: tombstone points dominated by `candidate` and insert `candidate`.
    void update(const std::vector<size_t>& candidate, uint64_t& cmpupd_counter);

    size_t size() const { return live_; }
    void clear();

    // To support the DR monotonicity condition, we keep track of the max g[0] 
    // of all live points in this frontier.
    size_t get_max_g0() const;

private:
    static constexpr size_t MAX_DIM = 10;
    struct Node {
        uint32_t axis;
        bool dead;
        std::array<size_t, MAX_DIM> pt;
        std::array<size_t, MAX_DIM> lo;
        std::array<size_t, MAX_DIM> hi;
        
        // Used in HEAP and EXPONENTIAL_ARENA
        Node* l_ptr{nullptr};
        Node* r_ptr{nullptr};

        // Used in VECTOR_ARENA
        uint32_t l_idx{UINT32_MAX};
        uint32_t r_idx{UINT32_MAX};

        Node() : axis(0), dead(false) {}
    };

    size_t proj_dim_;
    size_t d_ = 0; // Total dimension of vectors
    
    // HEAP / EXPONENTIAL_ARENA root
    Node* root_ptr_ = nullptr;
    // VECTOR_ARENA root
    uint32_t root_idx_ = UINT32_MAX;

    size_t live_ = 0;
    size_t total_ = 0;
    size_t built_ = 0;
    size_t max_g0_ = 0;

    static constexpr int REBUILD_FACTOR = 2;
    static constexpr int REBUILD_SLACK = 8;

    // Memory Backend: EXPONENTIAL_ARENA
    struct ArenaBlock {
        std::vector<Node> nodes;
        size_t used = 0;
        explicit ArenaBlock(size_t capacity) : nodes(capacity) {}
    };
    std::vector<ArenaBlock> arena_blocks_;
    
    // Memory Backend: VECTOR_ARENA
    std::vector<Node> tree_nodes_;

    // Helpers
    void init_leaf(Node& n, const std::vector<size_t>& pt, uint32_t axis);
    Node* alloc_node_ptr();
    uint32_t alloc_node_idx();
    void free_tree_ptr(Node* n);

    void insert_ptr(const std::vector<size_t>& pt);
    void insert_idx(const std::vector<size_t>& pt);

    bool query_ptr(Node* n, const std::vector<size_t>& query, uint64_t& cmpchk_counter) const;
    bool query_idx(uint32_t n_idx, const std::vector<size_t>& query, uint64_t& cmpchk_counter) const;

    void mark_dominated_ptr(Node* n, const std::vector<size_t>& pt, uint64_t& cmpupd_counter);
    void mark_dominated_idx(uint32_t n_idx, const std::vector<size_t>& pt, uint64_t& cmpupd_counter);

    void collect_live_ptr(Node* n, std::vector<std::vector<size_t>>& out) const;
    void collect_live_idx(uint32_t n_idx, std::vector<std::vector<size_t>>& out) const;

    Node* build_ptr(std::vector<std::vector<size_t>>& pts, size_t lo, size_t hi, int depth);
    uint32_t build_idx(std::vector<std::vector<size_t>>& pts, size_t lo, size_t hi, int depth);

    void rebuild();
    void update_max_g0();
};

} // namespace fast_mvh

#include "dynamic_frontier_kdtree_impl.hpp"

#endif // FAST_MVH_DYNAMIC_FRONTIER_KDTREE_H
