#include "fast_mvh/mvh_kdtree.h"

namespace fast_mvh {

StaticHeuristicKDTree::StaticHeuristicKDTree(
    const std::vector<std::vector<size_t>>& h_vectors, size_t leaf_size)
    : raw_heuristics_(h_vectors), leaf_size_(leaf_size) {
    if (raw_heuristics_.empty()) return;

    dim_ = raw_heuristics_[0].size() > 1 ? raw_heuristics_[0].size() - 1 : 1;

    std::vector<std::pair<std::vector<size_t>, uint32_t>> items;
    items.reserve(raw_heuristics_.size());
    for (size_t i = 0; i < raw_heuristics_.size(); ++i) {
        std::vector<size_t> tr_h(dim_);
        for (size_t d = 0; d < dim_; ++d) {
            tr_h[d] = raw_heuristics_[i][d + 1];
        }
        items.emplace_back(std::move(tr_h), static_cast<uint32_t>(i));
    }

    nodes_.emplace_back();
    build_tree(items, 0, 0);
}

void StaticHeuristicKDTree::build_tree(
    std::vector<std::pair<std::vector<size_t>, uint32_t>>& items,
    uint32_t node_idx, size_t depth) {
    auto& node = nodes_[node_idx];
    node.min_bounds.assign(dim_, SIZE_MAX);
    node.max_bounds.assign(dim_, 0);

    for (const auto& [pt, idx] : items) {
        node.heuristic_indices.push_back(idx);
        for (size_t d = 0; d < dim_; ++d) {
            node.min_bounds[d] = std::min(node.min_bounds[d], pt[d]);
            node.max_bounds[d] = std::max(node.max_bounds[d], pt[d]);
        }
    }

    if (items.size() <= leaf_size_) {
        node.first_elem_idx = static_cast<uint32_t>(leaf_items_.size());
        node.count = static_cast<uint32_t>(items.size());
        for (auto& item : items) {
            leaf_items_.push_back(std::move(item));
        }
        return;
    }

    // Split on dimension with largest spread
    size_t split_dim = 0;
    size_t max_spread = 0;
    for (size_t d = 0; d < dim_; ++d) {
        size_t spread = node.max_bounds[d] - node.min_bounds[d];
        if (spread > max_spread) {
            max_spread = spread;
            split_dim = d;
        }
    }

    size_t mid = items.size() / 2;
    std::nth_element(items.begin(), items.begin() + mid, items.end(),
                     [split_dim](const auto& a, const auto& b) {
                         return a.first[split_dim] < b.first[split_dim];
                     });

    std::vector<std::pair<std::vector<size_t>, uint32_t>> left_items(items.begin(), items.begin() + mid);
    std::vector<std::pair<std::vector<size_t>, uint32_t>> right_items(items.begin() + mid, items.end());

    uint32_t left_idx = static_cast<uint32_t>(nodes_.size());
    nodes_.emplace_back();
    uint32_t right_idx = static_cast<uint32_t>(nodes_.size());
    nodes_.emplace_back();

    nodes_[node_idx].left_child = left_idx;
    nodes_[node_idx].right_child = right_idx;

    build_tree(left_items, left_idx, depth + 1);
    build_tree(right_items, right_idx, depth + 1);
}

std::optional<std::pair<std::vector<size_t>, size_t>>
StaticHeuristicKDTree::choose_h(
    const std::vector<size_t>& g,
    const std::vector<std::vector<size_t>>& truncated_target_frontier,
    size_t start_idx) const {
    // Fallback linear scan verification skeleton
    for (size_t idx = start_idx; idx < raw_heuristics_.size(); ++idx) {
        const auto& h_val = raw_heuristics_[idx];
        bool dominated = false;
        for (const auto& tr_g : truncated_target_frontier) {
            bool is_dom = true;
            for (size_t i = 1; i < h_val.size(); ++i) {
                if (h_val[i] + g[i] < tr_g[i]) {
                    is_dom = false;
                    break;
                }
            }
            if (is_dom) {
                dominated = true;
                break;
            }
        }
        if (!dominated) {
            return std::make_pair(h_val, idx);
        }
    }
    return std::nullopt;
}

} // namespace fast_mvh
