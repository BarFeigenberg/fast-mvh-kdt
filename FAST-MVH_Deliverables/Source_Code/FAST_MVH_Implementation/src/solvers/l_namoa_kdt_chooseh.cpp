#include "fast_mvh/solvers/l_namoa_kdt_chooseh.h"

#include <cassert>
#include <chrono>
#include <fstream>
#include <optional>
#include <queue>
#include <algorithm>

L_NAMOA_KDT_CHOOSEH::L_NAMOA_KDT_CHOOSEH(
    const AdjacencyMatrix& adj_matrix, const EPS& eps)
    : AbstractSolver(adj_matrix, eps) {
    truncated_non_dominated_g_flat.resize(adj_matrix.size() + 1);
    pareto_list.resize(adj_matrix.size() + 1);
    truncated_non_dominated_kdt.reserve(adj_matrix.size() + 1);
    size_t num_obj = adj_matrix.num_of_objectives;
    for (size_t i = 0; i <= adj_matrix.size(); ++i) {
        truncated_non_dominated_kdt.emplace_back(num_obj > 1 ? num_obj - 1 : 1);
    }
}

bool L_NAMOA_KDT_CHOOSEH::better_local_dominance_check(
    const std::vector<size_t>& g, size_t id) {
    num_dr_dominance_check += 1;
    bool dominated = false;
    
    if (variant == Variant::V5) {
        dominated = truncated_non_dominated_kdt[id].check_dominated(g, cmp_chooseh);
        if (!dominated) return false;
        
        if (truncated_non_dominated_kdt[id].get_max_g0() <= g[0]) {
            return true;
        }
    } else {
        const auto& flat_list = truncated_non_dominated_g_flat[id];
        size_t num_obj = g.size();
        
        for (size_t i = 0; i < flat_list.size(); i += num_obj) {
            bool cur_dominated = true;
            for (size_t d = 1; d < num_obj; d++) {
                if (g[d] < flat_list[i + d]) {
                    cur_dominated = false;
                    break;
                }
            }
            if (cur_dominated) {
                dominated = true;
                break;
            }
        }

        if (!dominated) {
            return false;
        }

        if (flat_list[flat_list.size() - num_obj + 0] <= g[0]) {
            return true;
        }
    }

    num_full_dominance_check += 1;
    for (const auto& pareto_elem : pareto_list[id]) {
        bool dom = true;
        for (size_t i = 0; i < pareto_elem->g.size(); i++) {
            if (g[i] < pareto_elem->g[i]) {
                dom = false;
                break;
            }
        }
        if (dom) {
            num_bad_fallback++;
            return true;
        }
    }

    num_good_fallback++;
    return false;
}

bool L_NAMOA_KDT_CHOOSEH::global_dominance_check(const NodePtr& node_ptr, const size_t& target_id) {
    num_global_dominance_check += 1;
    
    if (variant == Variant::V5) {
        return truncated_non_dominated_kdt[target_id].check_dominated(node_ptr->f, cmp_chooseh);
    }
    
    const auto& flat_list = truncated_non_dominated_g_flat[target_id];
    size_t num_obj = node_ptr->f.size();
    
    for (size_t i = 0; i < flat_list.size(); i += num_obj) {
        bool dominated = true;
        for (size_t d = 1; d < num_obj; d++) {
            if (node_ptr->f[d] < flat_list[i + d]) {
                dominated = false;
                break;
            }
        }
        if (dominated) {
            return true;
        }
    }
    return false;
}

std::optional<std::pair<std::vector<size_t>, size_t>>
L_NAMOA_KDT_CHOOSEH::get_first_undominated_heuristic_value(
    size_t state_id,
    const std::vector<size_t>& g_value, const size_t& target,
    const std::vector<std::vector<size_t>>& node_mvh,
    size_t start_idx, bool force_linear) {

        if (use_kdt_chooseh && !force_linear) {
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
            
            if (variant == Variant::V5) {
                return heuristic_kdtrees[state_id].choose_h_dual(
                    g_value, truncated_non_dominated_kdt[target], start_idx, &cmp_chooseh);
            }
            return heuristic_kdtrees[state_id].choose_h(
                g_value, truncated_non_dominated_g_flat[target], start_idx, &cmp_chooseh);
        }
    }

    // Instrumented linear scan fallback (Maya's exact baseline logic)
    const auto& flat_list = truncated_non_dominated_g_flat[target];
    size_t num_obj = g_value.size();
    
    for (size_t idx = start_idx; idx < node_mvh.size(); ++idx) {
        const auto& heuristic_value = node_mvh[idx];
        bool dominated = false;
        
        for (size_t i = 0; i < flat_list.size(); i += num_obj) {
            cmp_chooseh++;
            bool is_dominated = true;
            for (size_t d = 1; d < num_obj; d++) {
                if (heuristic_value[d] + g_value[d] < flat_list[i + d]) {
                    is_dominated = false;
                    break;
                }
            }
            if (is_dominated) {
                dominated = true;
                break;
            }
        }
        
        if (!dominated) {
            return std::make_pair(heuristic_value, idx);
        }
    }
    return std::nullopt;
}

void L_NAMOA_KDT_CHOOSEH::operator()(
    const size_t& source, const size_t& target,
    const MultiValuedHeuristic& heuristic, SolutionSet& solutions,
    unsigned int time_limit, const std::string& solutions_file, const std::string& stats_file) {
    init_search();
    start_time = std::clock();

    // Pre-build static K-d trees for each state if enabled
    if (use_kdt_chooseh && heuristic_kdtrees.empty()) {
        heuristic_kdtrees.reserve(heuristic.size());
        for (size_t s = 0; s < heuristic.size(); ++s) {
            heuristic_kdtrees.emplace_back(heuristic[s]);
        }
    }

    std::priority_queue<NodePtr, std::vector<NodePtr>, CompareNodeByFValue> open;

    const size_t num_obj = adj_matrix.num_of_objectives;
    std::vector<size_t> zero_cost(num_obj, 0);
    std::vector<size_t> source_heuristic_value(num_obj, 0);
    NodePtr source_node = std::make_shared<Node>(
        source, zero_cost,
        source_heuristic_value, nullptr, zero_cost);

    open.push(source_node);

    while (!open.empty()) {
        if (time_limit > 0 && (std::clock() - start_time) / CLOCKS_PER_SEC >= time_limit) {
            time_limit_reached = true;
            break;
        }
        auto node = open.top();
        open.pop();
        num_generation += 1;

        if (global_dominance_check(node, target)) {
            auto result = get_first_undominated_heuristic_value(
                node->id, node->g, target, heuristic[node->id], node->h_idx + 1);
            if (!result) {
                continue;
            }
            auto& [h_val, h_idx] = result.value();
            NodePtr new_node = std::make_shared<Node>(
                node->id, node->g, h_val, node->parent, node->c, h_idx);
            open.push(new_node);
            ++num_reinsertion;
            continue;
        }

        if (better_local_dominance_check(node->g, node->id)) {
            continue;
        }

        // Update frontier
        if (variant == Variant::V5) {
            truncated_non_dominated_kdt[node->id].update(node->g, cmp_chooseh);
        } else {
            auto& flat_list = truncated_non_dominated_g_flat[node->id];
            size_t write_idx = 0;
            for (size_t read_idx = 0; read_idx < flat_list.size(); read_idx += num_obj) {
                bool is_dominated_by_g = true;
                for (size_t i = 1; i < num_obj; i++) {
                    if (flat_list[read_idx + i] < node->g[i]) {
                        is_dominated_by_g = false;
                        break;
                    }
                }
                if (!is_dominated_by_g) {
                    if (write_idx != read_idx) {
                        for(size_t i = 0; i < num_obj; i++) {
                            flat_list[write_idx + i] = flat_list[read_idx + i];
                        }
                    }
                    write_idx += num_obj;
                }
            }
            flat_list.resize(write_idx);
            
            size_t low = 0;
            size_t high = flat_list.size() / num_obj;
            while (low < high) {
                size_t mid = low + (high - low) / 2;
                bool is_less = false;
                for (size_t d = 0; d < num_obj; ++d) {
                    if (node->g[d] < flat_list[mid * num_obj + d]) {
                        is_less = true;
                        break;
                    } else if (node->g[d] > flat_list[mid * num_obj + d]) {
                        break;
                    }
                }
                if (is_less) {
                    high = mid;
                } else {
                    low = mid + 1;
                }
            }
            size_t insert_pos = low * num_obj;
            flat_list.insert(flat_list.begin() + insert_pos, node->g.begin(), node->g.end());
        }

        num_expansion += 1;
        pareto_list[node->id].push_back(node);

        if (node->id == target) {
            solutions.push_back(node);
            continue;
        }

        const std::vector<Edge>& outgoing_edges = adj_matrix[node->id];

        for (const auto& outgoing_edge : outgoing_edges) {
            std::vector new_g(node->g);
            for (size_t i = 0; i < new_g.size(); i++) {
                new_g[i] += outgoing_edge.cost[i];
            }

            auto result = get_first_undominated_heuristic_value(
                outgoing_edge.target, new_g, target, heuristic[outgoing_edge.target], 0, (variant == Variant::V3));
            if (!result) {
                continue;
            }

            if (better_local_dominance_check(new_g, outgoing_edge.target)) {
                continue;
            }

            auto& [h_val, h_idx] = result.value();
            NodePtr successor_node = std::make_shared<Node>(
                outgoing_edge.target, new_g, h_val, node, outgoing_edge.cost, h_idx);
            open.push(successor_node);
        }
    }

    runtime = static_cast<float>(std::clock() - start_time);

    // Compute average sizes over active states
    size_t total_truncated = 0;
    size_t total_pareto = 0;
    num_active_states = 0;
    size_t num_nodes = (variant == Variant::V5) ? truncated_non_dominated_kdt.size() : truncated_non_dominated_g_flat.size();
    for (size_t i = 0; i < num_nodes; i++) {
        size_t pts_count = (variant == Variant::V5) ? truncated_non_dominated_kdt[i].size() : (truncated_non_dominated_g_flat[i].size() / num_obj);
        if (pareto_list[i].size() > 0 || pts_count > 0) {
            num_active_states++;
            total_truncated += pts_count;
            total_pareto += pareto_list[i].size();
        }
    }
    avg_truncated_size = num_active_states > 0 ? static_cast<float>(total_truncated) / static_cast<float>(num_active_states) : 0.0f;
    avg_pareto_size = num_active_states > 0 ? static_cast<float>(total_pareto) / static_cast<float>(num_active_states) : 0.0f;

    if (!solutions_file.empty()) {
        std::ofstream sol_out(solutions_file);
        for (const auto& sol : solutions) {
            for (size_t i = 0; i < sol->g.size(); i++) {
                if (i > 0) sol_out << ",";
                sol_out << sol->g[i];
            }
            sol_out << "\n";
        }
    }

    if (!stats_file.empty()) {
        std::ofstream stats_out(stats_file);
        stats_out << runtime / CLOCKS_PER_SEC << "\t"
                  << num_expansion << "\t" << num_generation << "\n";
    }
}
