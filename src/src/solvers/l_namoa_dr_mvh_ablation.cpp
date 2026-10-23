#include "fast_mvh/solvers/l_namoa_dr_mvh_ablation.h"

#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <queue>
#include <stdexcept>

#include "fast_mvh/kdtree/ablation_fallback_index.h"

template <int D>
void L_NAMOA_DR_MVH_ABLATION::run(size_t source, size_t target, const MultiValuedHeuristic& heuristic,
                                  SolutionSet& solutions, unsigned int time_limit) {
    using Frontier = fast_mvh::HybridFrontier<D>;
    const size_t n_states = adj_matrix.size() + 1;
    std::vector<Frontier> front(n_states, Frontier(promotion));
    std::vector<fast_mvh::AblationFallbackIndex<D>> fb(n_states);
    std::vector<fast_mvh::HeuristicIndex<D>> hidx(heuristic.size());

    Frontier& T = front[target];
    auto domT = [&](const size_t* q) { return T.dominated_tr(q, cmp_chooseh); };

    auto chooseh = [&](size_t s, const size_t* g, size_t start, bool prev_refuted) -> size_t {
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

    auto local_check = [&](const size_t* g, size_t id) -> bool {
        const auto w = front[id].witness(g, cmpchk);
        if (w == Frontier::NONE) return false;
        if (w == Frontier::FULL) return true;
        ++num_full_dominance_check;

        const auto t0 = std::chrono::high_resolution_clock::now();
        bool dom = false;
        if (fallback_mode == FallbackMode::LINEAR) {
            dom = fb[id].contains_dominator_linear(g, cmp_full);
        } else {
            dom = fb[id].contains_dominator_forest(g, fallback_buffer, cmp_full);
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        t_fallback_query += std::chrono::duration<double>(t1 - t0).count();

        if (dom) {
            ++num_bad_fallback;
            return true;
        }
        ++num_good_fallback;
        return false;
    };

    struct Rec { size_t g[D]; uint32_t id, parent, h_idx; };
    struct Entry {
        size_t f[D];
        uint32_t rec;
        uint32_t h_idx;
    };
    struct Cmp {
        bool operator()(const Entry& a, const Entry& b) const {
            for (int k = 0; k < D; ++k) if (a.f[k] != b.f[k]) return a.f[k] > b.f[k];
            return false;
        }
    };
    constexpr uint32_t NO_PARENT = UINT32_MAX;
    std::vector<Rec> recs;
    recs.reserve(1 << 16);
    std::vector<Entry> heap;
    heap.reserve(1 << 16);
    std::priority_queue<Entry, std::vector<Entry>, Cmp> open(Cmp{}, std::move(heap));
    std::vector<uint32_t> sol_recs;

    auto push = [&](uint32_t rec, size_t r) {
        Entry e;
        const auto& h = heuristic[recs[rec].id][r];
        for (int k = 0; k < D; ++k) e.f[k] = recs[rec].g[k] + h[k];
        e.rec = rec;
        e.h_idx = static_cast<uint32_t>(r);
        open.push(e);
    };
    {
        Rec r0{};
        r0.id = static_cast<uint32_t>(source); r0.parent = NO_PARENT; r0.h_idx = 0;
        recs.push_back(r0);
        Entry e{};
        e.rec = 0; e.h_idx = 0;
        open.push(e);
    }

    while (!open.empty()) {
        if (time_limit > 0 && (std::clock() - start_time) / CLOCKS_PER_SEC >= (clock_t)time_limit) {
            time_limit_reached = true;
            break;
        }
        const Entry top = open.top();
        open.pop();
        ++num_generation;
        const uint32_t ri = top.rec;
        const size_t id = recs[ri].id;

        if (T.dominated_tr(top.f, cmpchk)) {
            const size_t r = chooseh(id, recs[ri].g, top.h_idx + 1, true);
            if (r >= heuristic[id].size()) continue;
            push(ri, r);
            ++num_reinsertion;
            continue;
        }
        if (local_check(recs[ri].g, id)) continue;

        if (fallback_target == FallbackTarget::RESIDUE) {
            const size_t g0 = recs[ri].g[0];
            auto& R = fb[id];
            front[id].update(recs[ri].g, cmpupd, [&](const size_t* p) {
                const auto t0 = std::chrono::high_resolution_clock::now();
                const bool retain = (g0 > p[0]);
                const auto t1 = std::chrono::high_resolution_clock::now();
                t_tracking += std::chrono::duration<double>(t1 - t0).count();
                if (retain) { R.insert(p); ++num_fallback_indexed; }
            });
        } else {
            const auto t0 = std::chrono::high_resolution_clock::now();
            fb[id].insert(recs[ri].g);
            ++num_fallback_indexed;
            const auto t1 = std::chrono::high_resolution_clock::now();
            t_tracking += std::chrono::duration<double>(t1 - t0).count();
            front[id].update(recs[ri].g, cmpupd);
        }
        ++num_expansion;

        if (id == target) {
            sol_recs.push_back(ri);
            continue;
        }
        for (const auto& e : adj_matrix[id]) {
            size_t ng[D];
            for (int i = 0; i < D; ++i) ng[i] = recs[ri].g[i] + e.cost[i];
            if (local_check(ng, e.target)) continue;
            const size_t r = chooseh(e.target, ng, 0, false);
            if (r >= heuristic[e.target].size()) continue;
            Rec nr;
            std::memcpy(nr.g, ng, sizeof(ng));
            nr.id = static_cast<uint32_t>(e.target); nr.parent = ri; nr.h_idx = static_cast<uint32_t>(r);
            recs.push_back(nr);
            push(static_cast<uint32_t>(recs.size() - 1), r);
        }
    }

    std::pmr::polymorphic_allocator<Node> alloc{&node_pool};
    std::vector<NodePtr> made(recs.size());
    const std::vector<size_t> zero(D, 0);
    auto make = [&](uint32_t i) -> NodePtr {
        std::vector<uint32_t> chain;
        for (uint32_t j = i; j != NO_PARENT && !made[j]; j = recs[j].parent) chain.push_back(j);
        for (size_t k = chain.size(); k-- > 0;) {
            const Rec& r = recs[chain[k]];
            made[chain[k]] = std::allocate_shared<Node>(alloc, r.id, std::vector<size_t>(r.g, r.g + D), zero,
                                                        r.parent == NO_PARENT ? nullptr : made[r.parent], zero_cost_, r.h_idx);
        }
        return made[i];
    };
    for (uint32_t ri : sol_recs) solutions.push_back(make(ri));

    for (const auto& f : front) num_frontier_trees += f.is_tree();
    for (const auto& fbi : fb) {
        t_index_build += fbi.build_time();
        total_indexed_vectors += fbi.size();
        if (fbi.size() > peak_indexed_vectors) {
            peak_indexed_vectors = fbi.size();
        }
    }
}

void L_NAMOA_DR_MVH_ABLATION::operator()(const size_t& source, const size_t& target,
                                         const MultiValuedHeuristic& heuristic, SolutionSet& solutions,
                                         unsigned int time_limit, const std::string& solutions_file,
                                         const std::string& stats_file) {
    (void)stats_file;
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
        default: throw std::invalid_argument("L_NAMOA_DR_MVH_ABLATION supports 2..8 objectives");
    }
    runtime = static_cast<float>(std::clock() - start_time);

    if (!solutions_file.empty()) {
        std::ofstream out(solutions_file);
        for (const auto& sol : solutions) {
            for (size_t i = 0; i < sol->g.size(); ++i) { if (i) out << ","; out << sol->g[i]; }
            out << "\n";
        }
    }
}
