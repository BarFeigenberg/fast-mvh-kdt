import os
import re

def patch_main():
    with open("src/src/main.cpp", "r") as f:
        code = f.read()
    code = code.replace(
        'algorithm == "L_NAMOA_KDT_V2" || algorithm == "L_NAMOA_KDT_V3") {',
        'algorithm == "L_NAMOA_KDT_V2" || algorithm == "L_NAMOA_KDT_V3" || algorithm == "L_NAMOA_KDT_V5") {'
    )
    code = code.replace(
        'else if (algorithm == "L_NAMOA_KDT_V3") solver.variant = L_NAMOA_KDT_CHOOSEH::Variant::V3;',
        'else if (algorithm == "L_NAMOA_KDT_V3") solver.variant = L_NAMOA_KDT_CHOOSEH::Variant::V3;\n            else if (algorithm == "L_NAMOA_KDT_V5") solver.variant = L_NAMOA_KDT_CHOOSEH::Variant::V5;'
    )
    with open("src/src/main.cpp", "w") as f:
        f.write(code)

def patch_header():
    with open("src/include/fast_mvh/solvers/l_namoa_kdt_chooseh.h", "r") as f:
        code = f.read()
    if 'V5' not in code:
        code = code.replace('V3\n    };', 'V3,\n        V5\n    };')
        code = code.replace('if (variant == Variant::V3) return "L_NAMOA_KDT_V3";', 'if (variant == Variant::V3) return "L_NAMOA_KDT_V3";\n        if (variant == Variant::V5) return "L_NAMOA_KDT_V5";')
        # Add dynamic kdtree include
        if 'dynamic_frontier_kdtree.h' not in code:
            code = code.replace('#include "fast_mvh/mvh_kdtree.h"', '#include "fast_mvh/mvh_kdtree.h"\n#include "fast_mvh/kdtree/dynamic_frontier_kdtree.h"')
        # Add dynamic KDT vector
        if 'truncated_non_dominated_kdt' not in code:
            code = code.replace(
                'std::vector<std::vector<size_t>> truncated_non_dominated_g_flat;',
                'std::vector<std::vector<size_t>> truncated_non_dominated_g_flat;\n    std::vector<fast_mvh::DynamicFrontierKDTree<>> truncated_non_dominated_kdt;'
            )
    with open("src/include/fast_mvh/solvers/l_namoa_kdt_chooseh.h", "w") as f:
        f.write(code)

def patch_cpp():
    with open("src/src/solvers/l_namoa_kdt_chooseh.cpp", "r") as f:
        code = f.read()
        
    # Init dynamic kdtree
    init_block = '''        for (auto& list : pareto_list) {
            list.clear();
        }
        if (variant == Variant::V5) {
            size_t num_obj = truncated_non_dominated_g_flat.size() > 0 ? (truncated_non_dominated_g_flat.capacity() > 0 ? 3 : 3) : 3;
            // Actually, we can resize in the constructor or just clear here. We'll init in operator() when adj_matrix size is known
        }
    }'''
    
    # Wait, better to init in constructor.
    # L_NAMOA_KDT_CHOOSEH::L_NAMOA_KDT_CHOOSEH(const AdjacencyMatrix& adj_matrix, const EPS& eps)
    cons = '''    pareto_list.resize(adj_matrix.size() + 1);
}'''
    if 'truncated_non_dominated_kdt.emplace_back' not in code:
        code = code.replace(cons, '''    pareto_list.resize(adj_matrix.size() + 1);
    truncated_non_dominated_kdt.reserve(adj_matrix.size() + 1);
    size_t num_obj = adj_matrix.num_of_objectives;
    for (size_t i = 0; i <= adj_matrix.size(); ++i) {
        truncated_non_dominated_kdt.emplace_back(num_obj > 1 ? num_obj - 1 : 1);
    }
}''')

    # Update logic
    update_str = '''        // 2. Insert node->g
        for (size_t i = 0; i < num_obj; i++) {
            flat_list.push_back(node->g[i]);
        }'''
    if 'variant == Variant::V5' not in update_str and 'truncated_non_dominated_kdt[node->id].update' not in code:
        code = code.replace(update_str, update_str + '\n        if (variant == Variant::V5) {\n            truncated_non_dominated_kdt[node->id].update(node->g, cmp_chooseh);\n        }')

    # In get_first_undominated_heuristic_value
    kdt_call = '''            return heuristic_kdtrees[state_id].choose_h(
                g_value, truncated_non_dominated_g_flat[target], start_idx, &cmp_chooseh);'''
    new_kdt_call = '''            if (variant == Variant::V5) {
                return heuristic_kdtrees[state_id].choose_h_dual(
                    g_value, truncated_non_dominated_kdt[target], start_idx, &cmp_chooseh);
            }
            return heuristic_kdtrees[state_id].choose_h(
                g_value, truncated_non_dominated_g_flat[target], start_idx, &cmp_chooseh);'''
    if 'choose_h_dual' not in code:
        code = code.replace(kdt_call, new_kdt_call)

    # In better_local_dominance_check
    # V5 should use KDT for expansion, and for generation... wait.
    # The prompt for Dual Tree was "combining Shahaf's state-frontier KDT with Variant 3's heuristic KDT".
    # So V5 uses V3's ExpandOnly fallback logic!
    gen_call = '(variant == Variant::V3)'
    new_gen_call = '(variant == Variant::V3 || variant == Variant::V5)'
    code = code.replace(gen_call, new_gen_call)

    with open("src/src/solvers/l_namoa_kdt_chooseh.cpp", "w") as f:
        f.write(code)
        
def patch_kdtree_h():
    with open("src/include/fast_mvh/mvh_kdtree.h", "r") as f:
        code = f.read()
    if 'choose_h_dual' not in code:
        if 'fast_mvh/kdtree/dynamic_frontier_kdtree.h' not in code:
            code = code.replace('#include <algorithm>', '#include <algorithm>\n#include "fast_mvh/kdtree/dynamic_frontier_kdtree.h"')
            
        func = '''    choose_h_bottom_up(const std::vector<size_t>& g,
             const std::vector<size_t>& flat_target_frontier,
             size_t start_idx = 0,
             uint64_t* cmp_counter = nullptr) const;'''
        new_func = func + '''\n\n    [[nodiscard]] std::optional<std::pair<std::vector<size_t>, size_t>>
    choose_h_dual(const std::vector<size_t>& g,
             const fast_mvh::DynamicFrontierKDTree<>& target_frontier_kdt,
             size_t start_idx = 0,
             uint64_t* cmp_counter = nullptr) const;'''
        code = code.replace(func, new_func)
    with open("src/include/fast_mvh/mvh_kdtree.h", "w") as f:
        f.write(code)

def patch_kdtree_cpp():
    with open("src/src/mvh_kdtree.cpp", "r") as f:
        code = f.read()
    if 'choose_h_dual' not in code:
        # Add the dual tree implementation
        # Because we need to be careful with namespace closing, we'll strip the last bracket, append, then close.
        code = code.replace('\n} // namespace fast_mvh', '')
        
        dual_impl = '''
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

    auto query_node = [&](auto self, uint32_t node_idx) -> void {
        if (node_idx >= nodes_.size()) return;
        const auto& node = nodes_[node_idx];

        if (node.max_idx < start_idx || node.min_idx >= best_valid_idx) {
            return;
        }

        const size_t* min_b = &all_min_bounds[node_idx * dim_];
        const size_t* max_b = &all_max_bounds[node_idx * dim_];

        // Rule 1: Is g + min_b dominated by the target frontier KDT?
        std::vector<size_t> query_min(dim_ + 1, 0);
        for(size_t d=0; d<dim_; ++d) query_min[d+1] = g[d+1] + min_b[d];
        if (target_frontier_kdt.check_dominated(query_min, *cmp_counter)) {
            return; // Pruned
        }

        // Rule 2: Is g + max_b guaranteed undominated by the target frontier KDT?
        std::vector<size_t> query_max(dim_ + 1, 0);
        for(size_t d=0; d<dim_; ++d) query_max[d+1] = g[d+1] + max_b[d];
        if (!target_frontier_kdt.check_dominated(query_max, *cmp_counter)) {
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
                    std::vector<size_t> query_exact(dim_ + 1, 0);
                    for(size_t d=0; d<dim_; ++d) query_exact[d+1] = g[d+1] + item.first[d];
                    if (!target_frontier_kdt.check_dominated(query_exact, *cmp_counter)) {
                        best_valid_idx = item.second;
                    }
                }
            }
        } else {
            self(self, node.left_child);
            self(self, node.right_child);
        }
    };

    query_node(query_node, 0);

    if (best_valid_idx != std::numeric_limits<size_t>::max()) {
        return std::make_pair(raw_heuristics_[best_valid_idx], best_valid_idx);
    }
    return std::nullopt;
}

} // namespace fast_mvh
'''
        code += dual_impl
    with open("src/src/mvh_kdtree.cpp", "w") as f:
        f.write(code)

if __name__ == "__main__":
    patch_main()
    patch_header()
    patch_cpp()
    patch_kdtree_h()
    patch_kdtree_cpp()
    print("V5 Patched successfully.")
