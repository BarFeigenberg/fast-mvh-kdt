#include "fast_mvh/solvers/L_NAMOA_DR_MVH_FAST3.h"

#include <ctime>
#include <fstream>
#include <queue>
#include <stdexcept>

#include "fast_mvh/kdtree/arena_gcl.h"

template <int D>
inline bool dominates_fast(const size_t* __restrict a, const size_t* __restrict g) {
    if constexpr (D == 2) {
        return (a[0] <= g[0]) & (a[1] <= g[1]);
    } else if constexpr (D == 3) {
        return (a[0] <= g[0]) & (a[1] <= g[1]) & (a[2] <= g[2]);
    } else if constexpr (D == 4) {
        return (a[0] <= g[0]) & (a[1] <= g[1]) & (a[2] <= g[2]) & (a[3] <= g[3]);
    } else if constexpr (D == 5) {
        return (a[0] <= g[0]) & (a[1] <= g[1]) & (a[2] <= g[2]) & (a[3] <= g[3]) & (a[4] <= g[4]);
    } else if constexpr (D == 6) {
        return (a[0] <= g[0]) & (a[1] <= g[1]) & (a[2] <= g[2]) & (a[3] <= g[3]) & (a[4] <= g[4]) & (a[5] <= g[5]);
    } else if constexpr (D == 7) {
        return (a[0] <= g[0]) & (a[1] <= g[1]) & (a[2] <= g[2]) & (a[3] <= g[3]) & (a[4] <= g[4]) & (a[5] <= g[5]) & (a[6] <= g[6]);
    } else if constexpr (D == 8) {
        return (a[0] <= g[0]) & (a[1] <= g[1]) & (a[2] <= g[2]) & (a[3] <= g[3]) & (a[4] <= g[4]) & (a[5] <= g[5]) & (a[6] <= g[6]) & (a[7] <= g[7]);
    } else {
        for (int k = 0; k < D; ++k) if (g[k] < a[k]) return false;
        return true;
    }
}

template <int D>
void L_NAMOA_DR_MVH_FAST3::run(size_t source, size_t target, const MultiValuedHeuristic& heuristic,
                              SolutionSet& solutions, unsigned int time_limit) {
    using Frontier = fast_mvh::HybridFrontier<D>;
    const size_t n_states = adj_matrix.size() + 1;
    std::vector<Frontier> front(n_states, Frontier(promotion, 0));
    
    std::vector<std::vector<size_t>> gcl(n_states);
    typename fast_mvh::ArenaGcl<D>::Arena gcl_arena;
    std::vector<fast_mvh::ArenaGcl<D>> gcl_idx;
    gcl_idx.reserve(n_states);
    for (size_t i = 0; i < n_states; ++i) {
        gcl_idx.emplace_back(gcl_arena);
    }
    
    std::vector<fast_mvh::HeuristicIndex<D>> hidx(heuristic.size());

    Frontier& T = front[target];
    auto domT = [&](const size_t* q) { return T.dominated_tr(q, cmp_chooseh); };

    auto chooseh = [&](size_t s, const std::vector<size_t>& g, size_t start, bool prev_refuted) -> size_t {
        ++num_chooseh;
        const auto& Hs = heuristic[s];
        if (start >= Hs.size()) return Hs.size();
        if (T.size() == 0) return start;
        auto& ix = hidx[s];
        if (!ix.loaded()) {
            ix.build(Hs, heuristic_tree_min, heuristic_leaf_size);
            num_heuristic_trees += ix.has_tree();
            num_redundant_heuristics += ix.redundant();
        }
        return ix.choose(g, start, prev_refuted, domT, num_htree_nodes);
    };

    auto local_check = [&](const std::vector<size_t>& g, size_t id) -> bool {
        const auto w = front[id].witness(g.data(), cmpchk);
        if (w == Frontier::NONE) return false;
        if (w == Frontier::FULL) return true;
        if (front[id].max_g0() <= g[0]) return true;  // t-discarding

        ++num_full_dominance_check;
        if (gcl_idx[id].contains_dominator(g.data(), gcl[id], cmp_full)) {
            ++num_bad_fallback;
            return true;
        }
        ++num_good_fallback;
        return false;
    };

    std::pmr::polymorphic_allocator<Node> alloc{&node_pool};
    std::priority_queue<NodePtr, std::vector<NodePtr>, CompareNodeByFValue> open;
    const std::vector<size_t> zero(D, 0);
    open.push(std::allocate_shared<Node>(alloc, source, zero, zero, nullptr, zero));

    while (!open.empty()) {
        if (time_limit > 0 && (std::clock() - start_time) / CLOCKS_PER_SEC >= time_limit) {
            time_limit_reached = true;
            break;
        }
        auto node = open.top();
        open.pop();
        ++num_generation;

        if (T.dominated_tr(node->f.data(), cmpchk)) {  // global dominance check
            const size_t r = chooseh(node->id, node->g, node->h_idx + 1, true);
            if (r >= heuristic[node->id].size()) continue;
            open.push(std::allocate_shared<Node>(alloc, node->id, node->g, heuristic[node->id][r],
                                                 node->parent, node->c, r));
            ++num_reinsertion;
            continue;
        }
        if (local_check(node->g, node->id)) continue;

        front[node->id].update(node->g.data(), cmpupd);
        gcl[node->id].insert(gcl[node->id].end(), node->g.begin(), node->g.end());
        gcl_idx[node->id].update_tree(gcl[node->id]);
        ++num_expansion;

        if (node->id == target) {
            solutions.push_back(node);
            continue;
        }
        for (const auto& e : adj_matrix[node->id]) {
            std::vector<size_t> ng(node->g);
            for (int i = 0; i < D; ++i) ng[i] += e.cost[i];
            if (local_check(ng, e.target)) continue;
            const size_t r = chooseh(e.target, ng, 0, false);
            if (r >= heuristic[e.target].size()) continue;
            open.push(std::allocate_shared<Node>(alloc, e.target, ng, heuristic[e.target][r], node, e.cost, r));
        }
    }
    for (const auto& f : front) num_frontier_trees += f.is_tree();
}

void L_NAMOA_DR_MVH_FAST3::operator()(const size_t& source, const size_t& target,
                                     const MultiValuedHeuristic& heuristic, SolutionSet& solutions,
                                     unsigned int time_limit, const std::string& solutions_file,
                                     const std::string& stats_file) {
    init_search();
    start_time = std::clock();
    switch (adj_matrix.num_of_objectives) {
        case 2: run<2>(source, target, heuristic, solutions, time_limit); break;
        case 3: run<3>(source, target, heuristic, solutions, time_limit); break;
        case 4: run<4>(source, target, heuristic, solutions, time_limit); break;
        case 5: run<5>(source, target, heuristic, solutions, time_limit); break;
        case 6: run<6>(source, target, heuristic, solutions, time_limit); break;
        case 7: run<7>(source, target, heuristic, solutions, time_limit); break;
        case 8: run<8>(source, target, heuristic, solutions, time_limit); break;
        default: throw std::invalid_argument("L_NAMOA_DR_MVH_FAST3 supports 2..8 objectives");
    }
    runtime = static_cast<float>(std::clock() - start_time);

    if (!solutions_file.empty()) {
        std::ofstream out(solutions_file);
        for (const auto& sol : solutions) {
            for (size_t i = 0; i < sol->g.size(); ++i) { if (i) out << ","; out << sol->g[i]; }
            out << "\n";
        }
    }
    if (!stats_file.empty()) {
        std::ofstream out(stats_file);
        out << runtime / CLOCKS_PER_SEC << "\t" << num_expansion << "\t" << num_generation << "\t"
            << cmpchk << "\t" << cmpupd << "\n";
    }
}
