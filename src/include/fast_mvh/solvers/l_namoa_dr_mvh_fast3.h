#ifndef L_NAMOA_DR_MVH_FAST3_H
#define L_NAMOA_DR_MVH_FAST3_H

// Hybrid integration layer (Maya + Shahaf + Roi) for L-NAMOA*dr-mvh.
// Same OPEN (NodePtr + CompareNodeByFValue), same push/pop sequence and same predicates as
// Maya's L_NAMOA_DR_MVH, hence identical expansions and bit-identical Pareto fronts.
//
// Integration decisions (all measured, see benchmarks/runs/<ts>/REPORT.md):
//  * G_cl^Tr(s) and T = G_cl^Tr(goal): fast_mvh::HybridFrontier (flat -> K-d tree on measured scan cost).
//  * G_cl(s): contiguous D-strided array (fallback scans without NodePtr indirection).
//  * LOCALDOMCHECK: witness query -> a full-D witness settles "dominated" without the fallback scan.
//  * Generation: LOCALDOMCHECK before CHOOSEH (both are side-effect-free predicates, so the
//    conjunction and therefore the pushed node are unchanged; CHOOSEH is skipped for locally
//    dominated successors).
//  * CHOOSEH: fast_mvh::HeuristicIndex (contiguous Tr(H(s)), redundancy pointers, Roi tree only
//    for |H(s)| >= heuristic_tree_min).

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

class L_NAMOA_DR_MVH_FAST3 : public AbstractSolver {
public:
    // Tunables (defaults from the empirical study).
    fast_mvh::PromotionPolicy promotion{};   // min_size = 8, mean_scan = 64
    size_t heuristic_tree_min = std::numeric_limits<size_t>::max();  // Roi tree off by default
    size_t heuristic_leaf_size = 8;

    size_t k_star = 0;
    
    // Instrumentation.
    uint64_t cmpchk = 0;       // point comparisons in local/global dominance queries
    uint64_t cmpupd = 0;       // point comparisons in frontier updates
    uint64_t cmp_chooseh = 0;  // point comparisons in CHOOSEH target queries
    uint64_t cmp_full = 0;     // point comparisons in full-D fallback scans
    size_t num_reinsertion = 0, num_full_dominance_check = 0, num_good_fallback = 0, num_bad_fallback = 0;
    size_t num_chooseh = 0, num_frontier_trees = 0, num_heuristic_trees = 0, num_redundant_heuristics = 0;
    uint64_t num_htree_nodes = 0;

    L_NAMOA_DR_MVH_FAST3(const AdjacencyMatrix& adj_matrix, const EPS& eps) : AbstractSolver(adj_matrix, eps) {}

    std::string get_solver_name() override { return "L_NAMOA_DR_MVH_FAST3"; }

    void init_search() override {
        AbstractSolver::init_search();
        cmpchk = cmpupd = cmp_chooseh = cmp_full = 0;
        num_reinsertion = num_full_dominance_check = num_good_fallback = num_bad_fallback = 0;
        num_chooseh = num_frontier_trees = num_heuristic_trees = num_redundant_heuristics = 0;
        num_htree_nodes = 0;
    }

    void operator()(const size_t& source, const size_t& target, const MultiValuedHeuristic& heuristic,
                    SolutionSet& solutions, unsigned int time_limit = 0,
                    const std::string& solutions_file = "", const std::string& stats_file = "");

    void operator()(const size_t&, const size_t&, const Heuristic&, SolutionSet&, unsigned) override {
        std::cout << "Single-valued heuristic not supported in MVH solver." << std::endl;
    }

    // Node pool must outlive every NodePtr handed out through `solutions`.
    std::pmr::unsynchronized_pool_resource node_pool;

private:
    template <int D>
    void run(size_t source, size_t target, const MultiValuedHeuristic& heuristic, SolutionSet& solutions,
             unsigned int time_limit);
};

#endif  // L_NAMOA_DR_MVH_FAST3_H
