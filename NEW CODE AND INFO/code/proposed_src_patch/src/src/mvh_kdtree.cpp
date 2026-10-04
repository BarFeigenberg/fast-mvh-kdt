#include "fast_mvh/mvh_kdtree.h"
#include <limits>
#include <algorithm>

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
    // Reserve flat bounds to avoid reallocations
    all_min_bounds.resize(dim_);
    all_max_bounds.resize(dim_);
    leaf_of_idx.resize(raw_heuristics_.size(), UINT32_MAX);

    build_tree(items, 0, 0);
}

void StaticHeuristicKDTree::build_tree(
    std::vector<std::pair<std::vector<size_t>, uint32_t>>& items,
    uint32_t node_idx, size_t depth) {
    
    // Ensure bounds arrays are large enough
    if (all_min_bounds.size() < (node_idx + 1) * dim_) {
        all_min_bounds.resize((node_idx + 1) * dim_);
        all_max_bounds.resize((node_idx + 1) * dim_);
    }

    size_t min_offset = node_idx * dim_;
    size_t max_offset = node_idx * dim_;

    for (size_t d = 0; d < dim_; ++d) {
        all_min_bounds[min_offset + d] = std::numeric_limits<size_t>::max();
        all_max_bounds[max_offset + d] = 0;
    }
    
    auto& node = nodes_[node_idx];
    node.min_idx = std::numeric_limits<uint32_t>::max();
    node.max_idx = 0;

    node.indices_offset = static_cast<uint32_t>(all_node_indices.size());
    node.indices_count = static_cast<uint32_t>(items.size());

    // Temporary array to hold and sort indices for this node
    std::vector<uint32_t> current_indices;
    current_indices.reserve(items.size());

    for (const auto& [pt, idx] : items) {
        current_indices.push_back(idx);
        node.min_idx = std::min(node.min_idx, idx);
        node.max_idx = std::max(node.max_idx, idx);
        for (size_t d = 0; d < dim_; ++d) {
            all_min_bounds[min_offset + d] = std::min(all_min_bounds[min_offset + d], pt[d]);
            all_max_bounds[max_offset + d] = std::max(all_max_bounds[max_offset + d], pt[d]);
        }
    }

    std::sort(current_indices.begin(), current_indices.end());
    all_node_indices.insert(all_node_indices.end(), current_indices.begin(), current_indices.end());

    if (items.size() <= leaf_size_) {
        node.first_elem_idx = static_cast<uint32_t>(leaf_items_.size());
        node.count = static_cast<uint32_t>(items.size());
        for (auto& item : items) {
            leaf_of_idx[item.second] = node_idx;
            leaf_items_.push_back(std::move(item));
        }
        return;
    }

    // Split on dimension with largest coordinate spread
    size_t split_dim = 0;
    size_t max_spread = 0;
    for (size_t d = 0; d < dim_; ++d) {
        size_t spread = all_max_bounds[max_offset + d] - all_min_bounds[min_offset + d];
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

    // Re-acquire node reference since emplace_back might have invalidated it
    nodes_[node_idx].left_child = left_idx;
    nodes_[node_idx].right_child = right_idx;
    nodes_[left_idx].parent_idx = node_idx;
    nodes_[right_idx].parent_idx = node_idx;

    build_tree(left_items, left_idx, depth + 1);
    build_tree(right_items, right_idx, depth + 1);
}

std::optional<std::pair<std::vector<size_t>, size_t>>
StaticHeuristicKDTree::choose_h(
    const std::vector<size_t>& g,
    const std::vector<size_t>& flat_target_frontier,
    size_t start_idx,
    uint64_t* cmp_counter) const {
    
    if (raw_heuristics_.empty() || start_idx >= raw_heuristics_.size()) {
        return std::nullopt;
    }

    // Fast-path: if goal frontier is empty, no vector can dominate Tr(g) + Tr(h)
    if (flat_target_frontier.empty()) {
        return std::make_pair(raw_heuristics_[start_idx], start_idx);
    }

    size_t best_valid_idx = std::numeric_limits<size_t>::max();
    size_t num_obj = dim_ + 1;

    auto query_node = [&](auto self, uint32_t node_idx) -> void {
        if (node_idx >= nodes_.size()) return;
        const auto& node = nodes_[node_idx];

        // Rule 0: Index bounds check
        if (node.max_idx < start_idx || node.min_idx >= best_valid_idx) {
            return;
        }

        const size_t* min_b = &all_min_bounds[node_idx * dim_];
        const size_t* max_b = &all_max_bounds[node_idx * dim_];

        // Rule 1: Subtree Discard (Aggregate Minimum)
        // If any vector in T dominates Tr(g) + N.min, then all heuristics in N are dominated.
        bool subtree_dominated = false;
        for (size_t i = 0; i < flat_target_frontier.size(); i += num_obj) {
            if (cmp_counter) (*cmp_counter)++;
            bool dom = true;
            for (size_t d = 0; d < dim_; ++d) {
                if (g[d + 1] + min_b[d] < flat_target_frontier[i + d + 1]) {
                    dom = false;
                    break;
                }
            }
            if (dom) {
                subtree_dominated = true;
                break;
            }
        }
        if (subtree_dominated) return; // Entire subtree is dominated, prune traversal

        // Rule 2: Subtree Acceptance (Aggregate Maximum)
        // If no vector in T dominates Tr(g) + N.max, then no vector in T can dominate any heuristic in N.
        bool max_dominated = false;
        for (size_t i = 0; i < flat_target_frontier.size(); i += num_obj) {
            if (cmp_counter) (*cmp_counter)++;
            bool dom = true;
            for (size_t d = 0; d < dim_; ++d) {
                if (g[d + 1] + max_b[d] < flat_target_frontier[i + d + 1]) {
                    dom = false;
                    break;
                }
            }
            if (dom) {
                max_dominated = true;
                break;
            }
        }

        if (!max_dominated) {
            // Entire subtree is guaranteed undominated by all vectors in T!
            auto it = std::lower_bound(
                all_node_indices.begin() + node.indices_offset,
                all_node_indices.begin() + node.indices_offset + node.indices_count,
                static_cast<uint32_t>(start_idx));
                
            if (it != (all_node_indices.begin() + node.indices_offset + node.indices_count)) {
                if (*it < best_valid_idx) {
                    best_valid_idx = *it;
                }
            }
            return; // Prune traversal of this subtree
        }

        // Rule 3: Recursive Refinement
        if (node.is_leaf()) {
            for (uint32_t i = 0; i < node.count; ++i) {
                const auto& [tr_h, idx] = leaf_items_[node.first_elem_idx + i];
                if (idx < start_idx || idx >= best_valid_idx) continue;

                // Check individual heuristic against goal frontier
                bool dominated = false;
                for (size_t fi = 0; fi < flat_target_frontier.size(); fi += num_obj) {
                    if (cmp_counter) (*cmp_counter)++;
                    bool dom = true;
                    for (size_t d = 0; d < dim_; ++d) {
                        if (g[d + 1] + tr_h[d] < flat_target_frontier[fi + d + 1]) {
                            dom = false;
                            break;
                        }
                    }
                    if (dom) {
                        dominated = true;
                        break;
                    }
                }
                if (!dominated) {
                    best_valid_idx = std::min(best_valid_idx, static_cast<size_t>(idx));
                }
            }
        } else {
            // Visit child with smaller min_idx first to find small lexicographical indices earlier
            uint32_t first = node.left_child;
            uint32_t second = node.right_child;
            if (nodes_[second].min_idx < nodes_[first].min_idx) {
                std::swap(first, second);
            }
            self(self, first);
            self(self, second);
        }
    };

    query_node(query_node, 0);

    if (best_valid_idx != std::numeric_limits<size_t>::max()) {
        return std::make_pair(raw_heuristics_[best_valid_idx], best_valid_idx);
    }
    return std::nullopt;
}


std::optional<std::pair<std::vector<size_t>, size_t>>
StaticHeuristicKDTree::choose_h_bottom_up(
    const std::vector<size_t>& g,
    const std::vector<size_t>& flat_target_frontier,
    size_t start_idx,
    uint64_t* cmp_counter) const {
    // The previous version returned a synthetic heuristic (Tr = subtree N.min, h_1 = 0) as soon as an
    // ancestor's min-box was undominated. That vector is not a member of H(s) and changes f = g + h,
    // hence the OPEN order (observed: 43420 vs 41129 expansions). In a spatial tree the ancestors of
    // leaf(start_idx) do not bound the index range [start_idx, K), so a true bottom-up search is not
    // defined here. Sound replacement: test h[start_idx] directly (fast path), then run the exact
    // top-down search from start_idx + 1.
    if (raw_heuristics_.empty() || start_idx >= raw_heuristics_.size()) return std::nullopt;
    if (flat_target_frontier.empty()) return std::make_pair(raw_heuristics_[start_idx], start_idx);

    const size_t num_obj = dim_ + 1;
    const auto& h0 = raw_heuristics_[start_idx];
    for (size_t i = 0; i < flat_target_frontier.size(); i += num_obj) {
        if (cmp_counter) (*cmp_counter)++;
        bool dom = true;
        for (size_t d = 0; d < dim_; ++d) {
            if (g[d + 1] + h0[d + 1] < flat_target_frontier[i + d + 1]) {
                dom = false;
                break;
            }
        }
        if (dom) {
            return choose_h(g, flat_target_frontier, start_idx + 1, cmp_counter);
        }
    }
    return std::make_pair(h0, start_idx);
}

std::optional<std::pair<std::vector<size_t>, size_t>>
StaticHeuristicKDTree::choose_h_dual(
    const std::vector<size_t>& g,
    const fast_mvh::DynamicFrontierKDTree<>& target_frontier_kdt,
    size_t start_idx,
    uint64_t* cmp_counter) const {
    
    if (raw_heuristics_.empty() || start_idx >= raw_heuristics_.size()) {
        return std::nullopt;
    }

    if (target_frontier_kdt.size() == 0) {
        return std::make_pair(raw_heuristics_[start_idx], start_idx);
    }

    size_t best_valid_idx = std::numeric_limits<size_t>::max();
    // One reusable query buffer per call (was: three heap-allocated vectors per visited node).
    std::vector<size_t> query(dim_ + 1, 0);

    auto query_node = [&](auto self, uint32_t node_idx) -> void {
        if (node_idx >= nodes_.size()) return;
        const auto& node = nodes_[node_idx];

        if (node.max_idx < start_idx || node.min_idx >= best_valid_idx) {
            return;
        }

        const size_t* min_b = &all_min_bounds[node_idx * dim_];
        const size_t* max_b = &all_max_bounds[node_idx * dim_];

        // Rule 1: Is g + min_b dominated by the target frontier KDT?
        for(size_t d=0; d<dim_; ++d) query[d+1] = g[d+1] + min_b[d];
        if (target_frontier_kdt.check_dominated(query, *cmp_counter)) {
            return; // Pruned
        }

        // Rule 2: Is g + max_b guaranteed undominated by the target frontier KDT?
        for(size_t d=0; d<dim_; ++d) query[d+1] = g[d+1] + max_b[d];
        if (!target_frontier_kdt.check_dominated(query, *cmp_counter)) {
            auto it = std::lower_bound(
                all_node_indices.begin() + node.indices_offset,
                all_node_indices.begin() + node.indices_offset + node.indices_count,
                static_cast<uint32_t>(start_idx));
                
            if (it != (all_node_indices.begin() + node.indices_offset + node.indices_count)) {
                if (*it < best_valid_idx) best_valid_idx = *it;
            }
            return;
        }

        if (node.is_leaf()) {
            uint32_t start = node.first_elem_idx;
            uint32_t end = start + node.count;
            for (uint32_t i = start; i < end; ++i) {
                const auto& item = leaf_items_[i];
                if (item.second >= start_idx && item.second < best_valid_idx) {
                    for(size_t d=0; d<dim_; ++d) query[d+1] = g[d+1] + item.first[d];
                    if (!target_frontier_kdt.check_dominated(query, *cmp_counter)) {
                        best_valid_idx = item.second;
                    }
                }
            }
        } else {
            // Same child order as choose_h: smaller min_idx first tightens best_valid_idx earlier.
            uint32_t first = node.left_child;
            uint32_t second = node.right_child;
            if (nodes_[second].min_idx < nodes_[first].min_idx) std::swap(first, second);
            self(self, first);
            self(self, second);
        }
    };

    query_node(query_node, 0);

    if (best_valid_idx != std::numeric_limits<size_t>::max()) {
        return std::make_pair(raw_heuristics_[best_valid_idx], best_valid_idx);
    }
    return std::nullopt;
}

} // namespace fast_mvh
