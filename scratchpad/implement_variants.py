import os
import re

def patch_main():
    main_path = "src/src/main.cpp"
    with open(main_path, "r") as f:
        content = f.read()

    # Find the parser check
    search = 'if (algorithm == "L_NAMOA_KDT_CHOOSEH" || algorithm == "L_NAMOA_DR_MVH_INSTRUMENTED") {'
    replace = '''if (algorithm == "L_NAMOA_KDT_CHOOSEH" || algorithm == "L_NAMOA_DR_MVH_INSTRUMENTED" || algorithm == "L_NAMOA_KDT_V1" || algorithm == "L_NAMOA_KDT_V2" || algorithm == "L_NAMOA_KDT_V3") {
            L_NAMOA_KDT_CHOOSEH solver(adj_matrix, eps);
            solver.use_kdt_chooseh = (algorithm != "L_NAMOA_DR_MVH_INSTRUMENTED");
            if (algorithm == "L_NAMOA_KDT_V1") solver.variant = L_NAMOA_KDT_CHOOSEH::Variant::V1;
            else if (algorithm == "L_NAMOA_KDT_V2") solver.variant = L_NAMOA_KDT_CHOOSEH::Variant::V2;
            else if (algorithm == "L_NAMOA_KDT_V3") solver.variant = L_NAMOA_KDT_CHOOSEH::Variant::V3;
            else solver.variant = L_NAMOA_KDT_CHOOSEH::Variant::ORIGINAL;'''
            
    content = content.replace(search + '\n            L_NAMOA_KDT_CHOOSEH solver(adj_matrix, eps);\n            solver.use_kdt_chooseh = (algorithm == "L_NAMOA_KDT_CHOOSEH");', replace)
    
    with open(main_path, "w") as f:
        f.write(content)

def patch_header():
    header_path = "src/include/fast_mvh/solvers/l_namoa_kdt_chooseh.h"
    with open(header_path, "r") as f:
        content = f.read()

    enum_code = '''
    enum class Variant {
        ORIGINAL,
        V1,
        V2,
        V3
    };
    Variant variant = Variant::ORIGINAL;
'''
    content = content.replace('bool use_kdt_chooseh = true;', 'bool use_kdt_chooseh = true;' + enum_code)
    
    name_func = '''    std::string get_solver_name() override {
        if (!use_kdt_chooseh) return "L_NAMOA_DR_MVH_INSTRUMENTED";
        if (variant == Variant::V1) return "L_NAMOA_KDT_V1";
        if (variant == Variant::V2) return "L_NAMOA_KDT_V2";
        if (variant == Variant::V3) return "L_NAMOA_KDT_V3";
        return "L_NAMOA_KDT_CHOOSEH";
    }'''
    content = re.sub(r'std::string get_solver_name\(\) override \{.*?\}', name_func, content, flags=re.DOTALL)
    
    sig = 'size_t start_idx = 0);'
    new_sig = 'size_t start_idx = 0, bool force_linear = false);'
    content = content.replace(sig, new_sig)
    
    with open(header_path, "w") as f:
        f.write(content)

def patch_cpp():
    cpp_path = "src/src/solvers/l_namoa_kdt_chooseh.cpp"
    with open(cpp_path, "r") as f:
        content = f.read()
        
    # In operator() for expansion, variant 3 should use KDT. So no changes needed if default is KDT.
    # In better_local_dominance_check for insertion:
    loc = 'auto heuristic_value_opt = get_first_undominated_heuristic_value(\n        state_id, g_value, target, node_mvh, start_idx);'
    new_loc = 'auto heuristic_value_opt = get_first_undominated_heuristic_value(\n        state_id, g_value, target, node_mvh, start_idx, (variant == Variant::V3));'
    content = content.replace(loc, new_loc)
    
    # In get_first_undominated_heuristic_value
    sig = 'size_t start_idx) {'
    new_sig = 'size_t start_idx, bool force_linear) {'
    content = content.replace(sig, new_sig)
    
    # V1 implementation
    kdt_block = '''    if (use_kdt_chooseh && !force_linear) {
        if (state_id < heuristic_kdtrees.size() && !heuristic_kdtrees[state_id].empty()) {
            
            if (variant == Variant::V1) {
                const auto& flat_list = truncated_non_dominated_g_flat[target];
                size_t num_obj = g_value.size();
                const auto& h0 = node_mvh[start_idx];
                bool h0_dominated = false;
                for (size_t i = 0; i < flat_list.size(); i += num_obj) {
                    cmp_chooseh++;
                    bool is_dominated = true;
                    for (size_t d = 1; d < num_obj; d++) {
                        if (h0[d] + g_value[d] < flat_list[i + d]) {
                            is_dominated = false;
                            break;
                        }
                    }
                    if (is_dominated) {
                        h0_dominated = true;
                        break;
                    }
                }
                if (!h0_dominated) {
                    return std::make_pair(h0, start_idx);
                }
            } else if (variant == Variant::V2) {
                return heuristic_kdtrees[state_id].choose_h_bottom_up(
                    g_value, truncated_non_dominated_g_flat[target], start_idx, &cmp_chooseh);
            }
            
            return heuristic_kdtrees[state_id].choose_h(
                g_value, truncated_non_dominated_g_flat[target], start_idx, &cmp_chooseh);
        }
    }'''
    
    content = re.sub(r'if \(use_kdt_chooseh\).*?&cmp_chooseh\);\n        }\n    }', kdt_block, content, flags=re.DOTALL)
    
    with open(cpp_path, "w") as f:
        f.write(content)

def patch_kdtree_h():
    h_path = "src/include/fast_mvh/mvh_kdtree.h"
    with open(h_path, "r") as f:
        content = f.read()
        
    node = 'uint32_t count{0}; // >0 indicates leaf node'
    new_node = 'uint32_t count{0}; // >0 indicates leaf node\n    uint32_t parent_idx{UINT32_MAX};'
    content = content.replace(node, new_node)
    
    tree_mem = 'std::vector<StaticKDNode> nodes_;'
    new_tree_mem = 'std::vector<StaticKDNode> nodes_;\n    std::vector<uint32_t> leaf_of_idx;'
    content = content.replace(tree_mem, new_tree_mem)
    
    func = 'uint64_t* cmp_counter = nullptr) const;'
    new_func = 'uint64_t* cmp_counter = nullptr) const;\n\n    [[nodiscard]] std::optional<std::pair<std::vector<size_t>, size_t>>\n    choose_h_bottom_up(const std::vector<size_t>& g,\n             const std::vector<size_t>& flat_target_frontier,\n             size_t start_idx = 0,\n             uint64_t* cmp_counter = nullptr) const;'
    content = content.replace(func, new_func)
    
    with open(h_path, "w") as f:
        f.write(content)

def patch_kdtree_cpp():
    cpp_path = "src/src/mvh_kdtree.cpp"
    with open(cpp_path, "r") as f:
        content = f.read()
        
    # Resize leaf_of_idx
    init = 'all_max_bounds.resize(dim_);'
    new_init = 'all_max_bounds.resize(dim_);\n    leaf_of_idx.resize(raw_heuristics_.size(), UINT32_MAX);'
    content = content.replace(init, new_init)
    
    # parent assignments
    pa = 'nodes_[node_idx].right_child = right_idx;'
    new_pa = 'nodes_[node_idx].right_child = right_idx;\n    nodes_[left_idx].parent_idx = node_idx;\n    nodes_[right_idx].parent_idx = node_idx;'
    content = content.replace(pa, new_pa)
    
    # leaf assignments
    la = 'leaf_items_.push_back(std::move(item));\n        }'
    new_la = 'leaf_of_idx[item.second] = node_idx;\n            leaf_items_.push_back(std::move(item));\n        }'
    content = content.replace(la, new_la)
    
    # add choose_h_bottom_up
    bottom_up = '''
std::optional<std::pair<std::vector<size_t>, size_t>>
StaticHeuristicKDTree::choose_h_bottom_up(
    const std::vector<size_t>& g,
    const std::vector<size_t>& flat_target_frontier,
    size_t start_idx,
    uint64_t* cmp_counter) const {
    
    if (raw_heuristics_.empty() || start_idx >= raw_heuristics_.size()) return std::nullopt;
    if (flat_target_frontier.empty()) return std::make_pair(raw_heuristics_[start_idx], start_idx);
    
    size_t num_obj = dim_ + 1;
    uint32_t curr = leaf_of_idx[start_idx];
    
    while (curr != UINT32_MAX) {
        if (curr >= nodes_.size()) break;
        const auto& node = nodes_[curr];
        
        if (node.is_leaf()) {
            const auto& h0 = raw_heuristics_[start_idx];
            bool h0_dominated = false;
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
                    h0_dominated = true;
                    break;
                }
            }
            if (!h0_dominated) return std::make_pair(h0, start_idx);
        } else {
            const size_t* min_b = &all_min_bounds[curr * dim_];
            bool min_dominated = false;
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
                    min_dominated = true;
                    break;
                }
            }
            if (!min_dominated) {
                std::vector<size_t> synthetic_h(dim_ + 1, 0);
                for(size_t d=0; d<dim_; ++d) synthetic_h[d+1] = min_b[d];
                // For synthetic, we keep start_idx as the reference point to avoid regression
                return std::make_pair(synthetic_h, start_idx);
            }
        }
        curr = node.parent_idx;
    }
    
    return std::nullopt;
}
'''
    content += bottom_up
    
    with open(cpp_path, "w") as f:
        f.write(content)

if __name__ == "__main__":
    patch_main()
    patch_header()
    patch_cpp()
    patch_kdtree_h()
    patch_kdtree_cpp()
    print("Patched all files!")
