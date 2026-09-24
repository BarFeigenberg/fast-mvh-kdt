#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <algorithm>
#include "fast_mvh/kdtree/dynamic_frontier_kdtree.h"

namespace fast_mvh {

/**
 * Node in the static KD-Tree indexing truncated heuristic vectors Tr(H(s)).
 * Flattened for data-oriented cache locality.
 */
struct StaticKDNode {
    uint32_t left_child{UINT32_MAX};
    uint32_t right_child{UINT32_MAX};
    uint32_t first_elem_idx{0};
    uint32_t count{0}; // >0 indicates leaf node
    uint32_t parent_idx{UINT32_MAX};

    uint32_t min_idx{UINT32_MAX};   // Minimum lexicographical index in subtree
    uint32_t max_idx{0};            // Maximum lexicographical index in subtree
    uint32_t indices_offset{0};     // Offset in all_node_indices
    uint32_t indices_count{0};      // Number of indices in all_node_indices

    [[nodiscard]] bool is_leaf() const noexcept {
        return count > 0;
    }
};

/**
 * Static KD-Tree for accelerating CHOOSEH over truncated heuristics.
 */
class StaticHeuristicKDTree {
public:
    StaticHeuristicKDTree() = default;
    explicit StaticHeuristicKDTree(const std::vector<std::vector<size_t>>& h_vectors, size_t leaf_size = 8);

    [[nodiscard]] std::optional<std::pair<std::vector<size_t>, size_t>>
    choose_h(const std::vector<size_t>& g,
             const std::vector<size_t>& flat_target_frontier,
             size_t start_idx = 0,
             uint64_t* cmp_counter = nullptr) const;

    [[nodiscard]] std::optional<std::pair<std::vector<size_t>, size_t>>
    choose_h_bottom_up(const std::vector<size_t>& g,
             const std::vector<size_t>& flat_target_frontier,
             size_t start_idx = 0,
             uint64_t* cmp_counter = nullptr) const;

    [[nodiscard]] std::optional<std::pair<std::vector<size_t>, size_t>>
    choose_h_dual(const std::vector<size_t>& g,
             const fast_mvh::DynamicFrontierKDTree<>& target_frontier_kdt,
             size_t start_idx = 0,
             uint64_t* cmp_counter = nullptr) const;

    [[nodiscard]] size_t size() const noexcept { return raw_heuristics_.size(); }
    [[nodiscard]] bool empty() const noexcept { return raw_heuristics_.empty(); }
    [[nodiscard]] const std::vector<std::vector<size_t>>& raw_heuristics() const noexcept { return raw_heuristics_; }

private:
    void build_tree(std::vector<std::pair<std::vector<size_t>, uint32_t>>& items,
                    uint32_t node_idx, size_t depth);

    std::vector<StaticKDNode> nodes_;
    std::vector<uint32_t> leaf_of_idx;
    std::vector<size_t> all_min_bounds;
    std::vector<size_t> all_max_bounds;
    std::vector<uint32_t> all_node_indices;

    std::vector<std::vector<size_t>> raw_heuristics_;
    std::vector<std::pair<std::vector<size_t>, uint32_t>> leaf_items_;
    size_t dim_{0};
    size_t leaf_size_{8};
};

} // namespace fast_mvh
