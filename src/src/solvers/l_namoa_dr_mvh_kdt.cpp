#include "fast_mvh/solvers/l_namoa_dr_mvh_kdt.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <optional>
#include <queue>

    L_NAMOA_DR_MVH_KDT::L_NAMOA_DR_MVH_KDT(
    const AdjacencyMatrix& adj_matrix, const EPS& eps)
    : AbstractSolver(adj_matrix, eps) {
    size_t num_obj = adj_matrix.num_of_objectives;
    
    truncated_non_dominated_kdt.reserve(adj_matrix.size() + 1);
    for (size_t i = 0; i <= adj_matrix.size(); ++i) {
        // We use (d-1) for KD-Tree projection dimension.
        truncated_non_dominated_kdt.emplace_back(num_obj - 1);
    }
    pareto_list.resize(adj_matrix.size() + 1);
}

bool L_NAMOA_DR_MVH_KDT::better_local_dominance_check(
    const std::vector<size_t>& g, size_t id) {
    num_dr_dominance_check += 1;
    
    // 1. O(1) T-discarding check: Is there a vector in KDT that dominates `g` in M-1 dimensions?
    bool dominated = truncated_non_dominated_kdt[id].check_dominated(g, cmpchk);

    if (!dominated) {
        return false; // Not dominated even in truncated space. Safe to add.
    }

    // 2. Monotonicity check
    // If the global max g1 of the set is <= g[0], we are sure it's fully dominated.
    if (truncated_non_dominated_kdt[id].get_max_g0() <= g[0]) {
        return true;
    }

    // 3. Fallback Check: full M-dimensional check on pareto_list
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

bool L_NAMOA_DR_MVH_KDT::global_dominance_check(const NodePtr& node_ptr, const size_t& target_id) {
    num_global_dominance_check += 1;
    // node_ptr->f is the lower bound. If target frontier has a vector that dominates f in M-1 dims, prune.
    return truncated_non_dominated_kdt[target_id].check_dominated(node_ptr->f, cmpchk);
}

std::optional<std::pair<std::vector<size_t>, size_t>>
L_NAMOA_DR_MVH_KDT::get_first_undominated_heuristic_value(
    const std::vector<size_t>& g_value, const size_t& target,
    const std::vector<std::vector<size_t>>& node_mvh,
    size_t start_idx) {
    for (size_t idx = start_idx; idx < node_mvh.size(); ++idx) {
        const auto& heuristic_value = node_mvh[idx];
        
        // Compute f_value for this specific heuristic
        std::vector<size_t> f_val(g_value.size());
        for (size_t i = 0; i < g_value.size(); ++i) {
            f_val[i] = g_value[i] + heuristic_value[i];
        }
        
        // If f_val is NOT dominated by the target frontier, we can use it.
        bool dominated = truncated_non_dominated_kdt[target].check_dominated(f_val, cmpchk);
        
        if (!dominated) {
            return std::make_pair(heuristic_value, idx);
        }
    }
    return std::nullopt;
}

void L_NAMOA_DR_MVH_KDT::operator()(
    const size_t& source, const size_t& target,
    const MultiValuedHeuristic& heuristic, SolutionSet& solutions,
    unsigned int time_limit, const std::string& solutions_file, const std::string& stats_file) {
    init_search();
    start_time = std::clock();
    std::pmr::polymorphic_allocator<Node> alloc{&node_pool};
    std::priority_queue<NodePtr, std::vector<NodePtr>, CompareNodeByFValue> open;

    const size_t num_obj = adj_matrix.num_of_objectives;
    std::vector<size_t> source_heuristic_value(num_obj, 0);
    NodePtr source_node = std::allocate_shared<Node>(alloc, source, std::vector<size_t>(num_obj, 0),
                                                     source_heuristic_value, nullptr, std::vector<size_t>(num_obj, 0));

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
            auto result = get_first_undominated_heuristic_value(node->g, target, heuristic[node->id], node->h_idx + 1);
            if (!result) {
                continue;
            }
            auto& [h_val, h_idx] = result.value();
            NodePtr new_node = std::allocate_shared<Node>(
                alloc, node->id, node->g, h_val, node->parent, node->c, h_idx);
            open.push(new_node);
            ++num_reinsertion;
            continue;
        }

        if (better_local_dominance_check(node->g, node->id)) {
            continue;
        }

        // Insert into KD-Tree (automatically tombstones dominated vectors)
        truncated_non_dominated_kdt[node->id].update(node->g, cmpupd);

        num_expansion += 1;

        pareto_list[node->id].push_back(node);

        if (node->id == target) {
            solutions.push_back(node);
            continue;
        }

        const std::vector<Edge>& outgoing_edges = adj_matrix[node->id];

        for (const auto& outgoing_edge : outgoing_edges) {
            std::vector<size_t> new_g(node->g);
            for (size_t i = 0; i < new_g.size(); i++) {
                new_g[i] += outgoing_edge.cost[i];
            }

            auto result = get_first_undominated_heuristic_value(new_g, target, heuristic[outgoing_edge.target]);
            if (!result) {
                continue;
            }

            if (better_local_dominance_check(new_g, outgoing_edge.target)) {
                continue;
            }

            auto& [h_val, h_idx] = result.value();
            NodePtr successor_node = std::allocate_shared<Node>(
                alloc, outgoing_edge.target, new_g, h_val, node, outgoing_edge.cost, h_idx);
            open.push(successor_node);
        }
    }

    runtime = static_cast<float>(std::clock() - start_time);

    size_t total_truncated = 0;
    size_t total_pareto = 0;
    num_active_states = 0;
    size_t num_nodes = truncated_non_dominated_kdt.size();
    for (size_t i = 0; i < num_nodes; i++) {
        if (pareto_list[i].size() > 0 || truncated_non_dominated_kdt[i].size() > 0) {
            num_active_states++;
            total_truncated += truncated_non_dominated_kdt[i].size();
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
        // Include cmpchk and cmpupd for metric instrumentation
        stats_out << runtime / CLOCKS_PER_SEC << "\t"
            << num_expansion << "\t" << num_generation << "\t"
            << cmpchk << "\t" << cmpupd << "\n";
    }
}
