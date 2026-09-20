#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <algorithm>

namespace fast_mvh {

/**
 * Node in the static KD-Tree indexing truncated heuristic vectors Tr(H(s)).
 */
struct StaticKDNode {
    uint32_t left_child{UINT32_MAX};
    uint32_t right_child{UINT32_MAX};
    uint32_t first_elem_idx{0};
    uint32_t count{0}; // >0 indicates leaf node

    std::vector<size_t> min_bounds; // N.min
    std::vector<size_t> max_bounds; // N.max
    std::vector<uint32_t> heuristic_indices; // N.I (lexicographical indices)

    [[nodiscard]] bool is_leaf() const noexcept {
        return count > 0;
    }
};

/**
 * Static KD-Tree for accelerating CHOOSEH over truncated heuristics.
 */
class StaticHeuristicKDTree {
public:
    explicit StaticHeuristicKDTree(const std::vector<std::vector<size_t>>& h_vectors, size_t leaf_size = 8);

    [[nodiscard]] std::optional<std::pair<std::vector<size_t>, size_t>>
    choose_h(const std::vector<size_t>& g,
             const std::vector<std::vector<size_t>>& truncated_target_frontier,
             size_t start_idx = 0) const;

    [[nodiscard]] size_t size() const noexcept { return raw_heuristics_.size(); }

private:
    void build_tree(std::vector<std::pair<std::vector<size_t>, uint32_t>>& items,
                    uint32_t node_idx, size_t depth);

    std::vector<StaticKDNode> nodes_;
    std::vector<std::vector<size_t>> raw_heuristics_;
    std::vector<std::pair<std::vector<size_t>, uint32_t>> leaf_items_;
    size_t dim_{0};
    size_t leaf_size_{8};
};

} // namespace fast_mvh
