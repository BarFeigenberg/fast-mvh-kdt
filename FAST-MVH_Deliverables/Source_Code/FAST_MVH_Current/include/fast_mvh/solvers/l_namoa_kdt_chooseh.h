#ifndef L_NAMOA_KDT_CHOOSEH_H
#define L_NAMOA_KDT_CHOOSEH_H

#include "data_structures/adjacency_matrix.h"
#include "data_structures/apex_path_pair.h"
#include "definitions.h"
#include "solvers/abstract_solver.h"
#include "fast_mvh/mvh_kdtree.h"
#include "fast_mvh/kdtree/dynamic_frontier_kdtree.h"

#include <chrono>
#include <iostream>
#include <memory_resource>
#include <optional>
#include <unordered_map>
#include <vector>

/**
 * Isolated Ablation Solver:
 * Uses Maya's exact closed-set representations and dominance rules,
 * but accelerates CHOOSEH using Roi's Static (d-1)-d K-d tree on Tr(H(s))
 * with Aggregated Subtree Pruning and exact lexicographical tie-breaking.
 */
class L_NAMOA_KDT_CHOOSEH : public AbstractSolver {
public:
    std::pmr::unsynchronized_pool_resource node_pool;

    // Maya's exact closed-set representations, flattened for Cache Locality (DoD)
    std::vector<std::vector<size_t>> truncated_non_dominated_g_flat;
    std::vector<fast_mvh::DynamicFrontierKDTree<>> truncated_non_dominated_kdt;
    std::vector<std::vector<NodePtr>> pareto_list;

    // Static K-d trees indexing Tr(H(s)) for each state
    std::vector<fast_mvh::StaticHeuristicKDTree> heuristic_kdtrees;

    // Switch between KDT CHOOSEH (true) and Linear Scan CHOOSEH (false)
    bool use_kdt_chooseh = true;
    enum class Variant {
        ORIGINAL,
        V1,
        V2,
        V3,
        V5
    };
    Variant variant = Variant::ORIGINAL;


    // Search and dominance counters
    size_t num_full_dominance_check   = 0;
    size_t num_dr_dominance_check     = 0;
    size_t num_global_dominance_check = 0;
    size_t num_good_fallback          = 0;
    size_t num_bad_fallback           = 0;
    size_t num_g_dominates_existing   = 0;
    size_t num_reinsertion            = 0;
    float avg_truncated_size          = 0.0f;
    float avg_pareto_size             = 0.0f;
    size_t num_active_states          = 0;

    // Detailed instrumentation for CHOOSEH comparisons
    uint64_t cmp_chooseh              = 0;

    void init_search() override {
        AbstractSolver::init_search();
        num_full_dominance_check   = 0;
        num_dr_dominance_check     = 0;
        num_global_dominance_check = 0;
        num_good_fallback          = 0;
        num_bad_fallback           = 0;
        num_g_dominates_existing   = 0;
        num_reinsertion            = 0;
        avg_truncated_size         = 0.0f;
        avg_pareto_size            = 0.0f;
        num_active_states          = 0;
        cmp_chooseh                = 0;

        for (auto& list : truncated_non_dominated_g_flat) {
            list.clear();
        }
        for (auto& list : pareto_list) {
            list.clear();
        }
        for (auto& kdt : truncated_non_dominated_kdt) {
            kdt.clear();
        }
    }

        std::string get_solver_name() override {
        if (!use_kdt_chooseh) return "L_NAMOA_DR_MVH_INSTRUMENTED";
        if (variant == Variant::V1) return "L_NAMOA_KDT_V1";
        if (variant == Variant::V2) return "L_NAMOA_KDT_V2";
        if (variant == Variant::V3) return "L_NAMOA_KDT_V3";
        if (variant == Variant::V5) return "L_NAMOA_KDT_V5";
        return "L_NAMOA_KDT_CHOOSEH";
    }

    void operator()(const size_t& source, const size_t& target,
                    const MultiValuedHeuristic& heuristic,
                    SolutionSet& solutions,
                    unsigned int time_limit = 0,
                    const std::string& solutions_file = "",
                    const std::string& stats_file = "");

    void operator()(const size_t& source, const size_t& target,
                    const Heuristic& heuristic, SolutionSet& solutions,
                    unsigned time_limit) override {
        std::cout << "Single-valued heuristic not supported in MVH solver." << std::endl;
    }

    L_NAMOA_KDT_CHOOSEH(const AdjacencyMatrix& adj_matrix, const EPS& eps);

    [[nodiscard]] bool better_local_dominance_check(const std::vector<size_t>& g, size_t id);

    bool global_dominance_check(const NodePtr& node_ptr, const size_t& target_id);

    std::optional<std::pair<std::vector<size_t>, size_t>> get_first_undominated_heuristic_value(
        size_t state_id,
        const std::vector<size_t>& g_value, const size_t& target,
        const std::vector<std::vector<size_t>>& node_mvh,
        size_t start_idx = 0, bool force_linear = false);
};

#endif // L_NAMOA_KDT_CHOOSEH_H
