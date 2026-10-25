#ifndef L_NAMOA_DR_MVH_ABLATION_H
#define L_NAMOA_DR_MVH_ABLATION_H

#include "data_structures/adjacency_matrix.h"
#include "definitions.h"
#include "solvers/abstract_solver.h"
#include "fast_mvh/heuristic_index.h"
#include "fast_mvh/kdtree/hybrid_frontier.h"

#include <iostream>
#include <limits>
#include <memory_resource>
#include <string>
#include <vector>

class L_NAMOA_DR_MVH_ABLATION : public AbstractSolver {
public:
    enum class FallbackTarget { RESIDUE, FULL_HISTORY };
    enum class FallbackMode { FOREST, LINEAR };

    fast_mvh::PromotionPolicy promotion{};
    size_t heuristic_tree_min = std::numeric_limits<size_t>::max();
    size_t heuristic_leaf_size = 8;
    size_t fallback_buffer = 64;

    FallbackTarget fallback_target = FallbackTarget::FULL_HISTORY;
    FallbackMode fallback_mode = FallbackMode::FOREST;
    bool pure_full_d = false;

    // Instrumentation counters & timers
    uint64_t cmpchk = 0;
    uint64_t cmpupd = 0;
    uint64_t cmp_chooseh = 0;
    uint64_t cmp_full = 0;
    size_t num_reinsertion = 0, num_full_dominance_check = 0, num_good_fallback = 0, num_bad_fallback = 0;
    size_t num_chooseh = 0, num_frontier_trees = 0, num_heuristic_trees = 0, num_redundant_heuristics = 0;
    uint64_t num_htree_nodes = 0;
    uint64_t num_fallback_indexed = 0;

    double t_fallback_query = 0.0;
    double t_index_build = 0.0;
    double t_tracking = 0.0;
    size_t total_indexed_vectors = 0;
    size_t peak_indexed_vectors = 0;

    L_NAMOA_DR_MVH_ABLATION(const AdjacencyMatrix& adj_matrix, const EPS& eps) : AbstractSolver(adj_matrix, eps) {}

    std::string get_solver_name() override { return "L_NAMOA_DR_MVH_ABLATION"; }

    void init_search() override {
        AbstractSolver::init_search();
        cmpchk = cmpupd = cmp_chooseh = cmp_full = 0;
        num_reinsertion = num_full_dominance_check = num_good_fallback = num_bad_fallback = 0;
        num_chooseh = num_frontier_trees = num_heuristic_trees = num_redundant_heuristics = 0;
        num_htree_nodes = 0;
        num_fallback_indexed = 0;
        t_fallback_query = t_index_build = t_tracking = 0.0;
        total_indexed_vectors = peak_indexed_vectors = 0;
    }

    void operator()(const size_t& source, const size_t& target, const MultiValuedHeuristic& heuristic,
                    SolutionSet& solutions, unsigned int time_limit = 0,
                    const std::string& solutions_file = "", const std::string& stats_file = "");

    void operator()(const size_t&, const size_t&, const Heuristic&, SolutionSet&, unsigned) override {
        std::cout << "Single-valued heuristic not supported in MVH solver." << std::endl;
    }

    std::pmr::unsynchronized_pool_resource node_pool;
    const std::vector<size_t> zero_cost_ = std::vector<size_t>(8, 0);

private:
    template <int D>
    void run(size_t source, size_t target, const MultiValuedHeuristic& heuristic, SolutionSet& solutions,
             unsigned int time_limit);
};

#endif  // L_NAMOA_DR_MVH_ABLATION_H
