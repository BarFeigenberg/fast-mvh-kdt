#include <iostream>
#include <vector>
#include <array>
#include <queue>
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <cassert>

template <int M>
struct CostVec {
    std::array<int, M> data;
    int& operator[](int i) { return data[i]; }
    const int& operator[](int i) const { return data[i]; }

    bool operator==(const CostVec<M>& o) const {
        return data == o.data;
    }

    bool operator<(const CostVec<M>& o) const {
        return data < o.data;
    }

    bool operator>(const CostVec<M>& o) const {
        return data > o.data;
    }

    bool dominates(const CostVec<M>& o) const {
        bool strict = false;
        for (int i = 0; i < M; ++i) {
            if (data[i] > o.data[i]) return false;
            if (data[i] < o.data[i]) strict = true;
        }
        return strict;
    }

    bool weakly_dominates(const CostVec<M>& o) const {
        for (int i = 0; i < M; ++i) {
            if (data[i] > o.data[i]) return false;
        }
        return true;
    }

    // Projection Tr_1: drops coordinate 0 (coordinates 1 .. M-1)
    bool tr1_weakly_dominates(const CostVec<M>& o) const {
        for (int i = 1; i < M; ++i) {
            if (data[i] > o.data[i]) return false;
        }
        return true;
    }

    // Projection Tr_M: drops coordinate M-1 (coordinates 0 .. M-2)
    bool trM_weakly_dominates(const CostVec<M>& o) const {
        for (int i = 0; i < M - 1; ++i) {
            if (data[i] > o.data[i]) return false;
        }
        return true;
    }
};

template <int M>
struct Graph {
    int V;
    struct Edge {
        int to;
        CostVec<M> cost;
    };
    std::vector<std::vector<Edge>> adj;

    Graph(int v) : V(v), adj(v) {}

    void add_directed_edge(int u, int v, const CostVec<M>& c) {
        adj[u].push_back({v, c});
    }
};

// Generates an anti-correlated DAG where many paths arrive at each state
template <int M>
Graph<M> generate_dense_dag(int stages, int width, int seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist_base(2, 25);
    std::uniform_int_distribution<int> dist_anti(1, 30);

    int curr_id = 0;
    std::vector<std::vector<int>> stage_nodes(stages);
    for (int s = 0; s < stages; ++s) {
        int w = (s == 0 || s == stages - 1) ? 1 : width;
        stage_nodes[s].resize(w);
        for (int i = 0; i < w; ++i) stage_nodes[s][i] = curr_id++;
    }

    Graph<M> g(curr_id);
    for (int s = 0; s < stages - 1; ++s) {
        for (int u : stage_nodes[s]) {
            for (int v : stage_nodes[s + 1]) {
                CostVec<M> c;
                c[0] = dist_anti(rng);
                c[1] = std::max(1, 35 - c[0] + (int)(rng() % 5) - 2);
                for (int m = 2; m < M; ++m) {
                    c[m] = dist_base(rng);
                }
                g.add_directed_edge(u, v, c);
            }
        }
    }
    return g;
}

// -------------------------------------------------------------
// Strategy 1: Standard Single-Axis DR (Tr_1 + Full M-D Fallback)
// -------------------------------------------------------------
template <int M>
struct SingleAxisStats {
    double runtime_ms;
    uint64_t expansions;
    uint64_t tr1_checks;
    uint64_t full_fallbacks;
    uint64_t scalar_cmps;
    std::vector<CostVec<M>> solutions;
};

template <int M>
SingleAxisStats<M> run_single_axis_search(const Graph<M>& g, int s_start, int s_goal) {
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t expansions = 0;
    uint64_t tr1_checks = 0;
    uint64_t full_fallbacks = 0;
    uint64_t scalar_cmps = 0;

    std::vector<CostVec<M>> sols;
    std::vector<std::vector<CostVec<M>>> F(g.V);     // Tr1 frontier
    std::vector<std::vector<CostVec<M>>> G_cl(g.V);  // Full closed set for fallback

    struct Node {
        int u;
        CostVec<M> g;
        bool operator>(const Node& o) const {
            if (g[0] != o.g[0]) return g[0] > o.g[0];
            return g.data > o.g.data;
        }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    CostVec<M> zero_g; zero_g.data.fill(0);
    pq.push({s_start, zero_g});

    while (!pq.empty()) {
        auto [u, g_curr] = pq.top();
        pq.pop();

        // Local dominance check via Single-Axis (Tr_1)
        bool dominated = false;
        bool has_tr_dominator = false;

        for (const auto& p : F[u]) {
            tr1_checks++;
            scalar_cmps += (M - 1);
            if (p.tr1_weakly_dominates(g_curr)) {
                scalar_cmps += 1;
                if (p[0] <= g_curr[0]) {
                    dominated = true; // Full witness
                    break;
                }
                has_tr_dominator = true;
            }
        }

        if (!dominated && has_tr_dominator) {
            // Trigger Full M-Dimensional Fallback
            full_fallbacks++;
            for (const auto& p : G_cl[u]) {
                scalar_cmps += M;
                if (p.weakly_dominates(g_curr)) {
                    dominated = true;
                    break;
                }
            }
        }

        if (dominated) continue;

        // Update F[u] and G_cl[u]
        G_cl[u].push_back(g_curr);
        std::vector<CostVec<M>> new_f;
        for (const auto& p : F[u]) {
            scalar_cmps += (M - 1);
            if (!g_curr.tr1_weakly_dominates(p)) {
                new_f.push_back(p);
            }
        }
        new_f.push_back(g_curr);
        F[u] = std::move(new_f);

        if (u == s_goal) {
            sols.push_back(g_curr);
            continue;
        }

        expansions++;
        for (const auto& edge : g.adj[u]) {
            CostVec<M> nxt_g;
            for (int m = 0; m < M; ++m) nxt_g[m] = g_curr[m] + edge.cost[m];
            pq.push({edge.to, nxt_g});
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Pareto filter sols
    std::vector<CostVec<M>> pareto;
    for (size_t i = 0; i < sols.size(); ++i) {
        bool dom = false;
        for (size_t j = 0; j < sols.size(); ++j) {
            if (i != j && sols[j].dominates(sols[i])) { dom = true; break; }
        }
        if (!dom) pareto.push_back(sols[i]);
    }

    return {ms, expansions, tr1_checks, full_fallbacks, scalar_cmps, pareto};
}

// -------------------------------------------------------------
// Strategy 2: Multi-Axis Dual Projection (Tr_1 + Tr_M Fallback Filter)
// -------------------------------------------------------------
template <int M>
struct MultiAxisStats {
    double runtime_ms;
    uint64_t expansions;
    uint64_t tr1_checks;
    uint64_t trM_checks;
    uint64_t fallbacks_avoided;
    uint64_t remaining_fallbacks;
    uint64_t scalar_cmps;
    std::vector<CostVec<M>> solutions;
};

template <int M>
MultiAxisStats<M> run_multi_axis_search(const Graph<M>& g, int s_start, int s_goal) {
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t expansions = 0;
    uint64_t tr1_checks = 0;
    uint64_t trM_checks = 0;
    uint64_t fallbacks_avoided = 0;
    uint64_t remaining_fallbacks = 0;
    uint64_t scalar_cmps = 0;

    std::vector<CostVec<M>> sols;
    std::vector<std::vector<CostVec<M>>> F1(g.V);     // Primary Tr1 frontier (coords 1..M-1)
    std::vector<std::vector<CostVec<M>>> FM(g.V);     // Complementary TrM frontier (coords 0..M-2)
    std::vector<std::vector<CostVec<M>>> G_cl(g.V);   // Full closed set

    struct Node {
        int u;
        CostVec<M> g;
        bool operator>(const Node& o) const {
            if (g[0] != o.g[0]) return g[0] > o.g[0];
            return g.data > o.g.data;
        }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    CostVec<M> zero_g; zero_g.data.fill(0);
    pq.push({s_start, zero_g});

    while (!pq.empty()) {
        auto [u, g_curr] = pq.top();
        pq.pop();

        // 1. Primary Projection Test (Tr_1)
        bool dominated = false;
        bool has_tr1_dominator = false;

        for (const auto& p : F1[u]) {
            tr1_checks++;
            scalar_cmps += (M - 1);
            if (p.tr1_weakly_dominates(g_curr)) {
                scalar_cmps += 1;
                if (p[0] <= g_curr[0]) {
                    dominated = true; // Full witness in Tr_1
                    break;
                }
                has_tr1_dominator = true;
            }
        }

        // 2. If Tr1 was inconclusive (false positive order violation), query Complementary Projection (Tr_M)!
        if (!dominated && has_tr1_dominator) {
            bool has_trM_dominator = false;
            for (const auto& p : FM[u]) {
                trM_checks++;
                scalar_cmps += (M - 1);
                if (p.trM_weakly_dominates(g_curr)) {
                    scalar_cmps += 1;
                    if (p[M - 1] <= g_curr[M - 1]) {
                        dominated = true; // Full witness in Tr_M!
                        break;
                    }
                    has_trM_dominator = true;
                }
            }

            if (!dominated) {
                if (!has_trM_dominator) {
                    // Tr_M PROVED that no vector dominates g_curr in coordinates 0..M-2!
                    // Therefore, g_curr is GUARANTEED to be non-dominated!
                    // Full M-D fallback completely avoided!
                    fallbacks_avoided++;
                } else {
                    // Both projections had false positives: must do remaining fallback
                    remaining_fallbacks++;
                    for (const auto& p : G_cl[u]) {
                        scalar_cmps += M;
                        if (p.weakly_dominates(g_curr)) {
                            dominated = true;
                            break;
                        }
                    }
                }
            }
        }

        if (dominated) continue;

        // Update frontiers
        G_cl[u].push_back(g_curr);

        // Update F1
        std::vector<CostVec<M>> new_f1;
        for (const auto& p : F1[u]) {
            scalar_cmps += (M - 1);
            if (!g_curr.tr1_weakly_dominates(p)) new_f1.push_back(p);
        }
        new_f1.push_back(g_curr);
        F1[u] = std::move(new_f1);

        // Update FM
        std::vector<CostVec<M>> new_fM;
        for (const auto& p : FM[u]) {
            scalar_cmps += (M - 1);
            if (!g_curr.trM_weakly_dominates(p)) new_fM.push_back(p);
        }
        new_fM.push_back(g_curr);
        FM[u] = std::move(new_fM);

        if (u == s_goal) {
            sols.push_back(g_curr);
            continue;
        }

        expansions++;
        for (const auto& edge : g.adj[u]) {
            CostVec<M> nxt_g;
            for (int m = 0; m < M; ++m) nxt_g[m] = g_curr[m] + edge.cost[m];
            pq.push({edge.to, nxt_g});
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Pareto filter sols
    std::vector<CostVec<M>> pareto;
    for (size_t i = 0; i < sols.size(); ++i) {
        bool dom = false;
        for (size_t j = 0; j < sols.size(); ++j) {
            if (i != j && sols[j].dominates(sols[i])) { dom = true; break; }
        }
        if (!dom) pareto.push_back(sols[i]);
    }

    return {ms, expansions, tr1_checks, trM_checks, fallbacks_avoided, remaining_fallbacks, scalar_cmps, pareto};
}

template <int M>
void execute_investigation_3(int stages, int width, int seed) {
    std::cout << "\n========================================================================\n";
    std::cout << "INVESTIGATION 3: MULTI-AXIS PROJECTIONS (M = " << M 
              << ", Stages = " << stages << ", Width = " << width << ")\n";
    std::cout << "========================================================================\n";

    Graph<M> g = generate_dense_dag<M>(stages, width, seed);
    int s_start = 0;
    int s_goal = g.V - 1;

    // 1. Single-Axis Search (Tr_1 Baseline)
    auto single = run_single_axis_search<M>(g, s_start, s_goal);
    std::cout << "  [Strategy 1: Single-Axis Tr_1 Baseline]:\n"
              << "      Runtime:               " << std::fixed << std::setprecision(2) << single.runtime_ms << " ms\n"
              << "      Expansions:            " << single.expansions << "\n"
              << "      Tr_1 Dominance Checks: " << single.tr1_checks << "\n"
              << "      Full M-D Fallbacks:    " << single.full_fallbacks << "\n"
              << "      Scalar Comparisons:    " << single.scalar_cmps << "\n"
              << "      Pareto Solutions:      " << single.solutions.size() << "\n";

    // 2. Multi-Axis Dual Projection Search (Tr_1 + Tr_M)
    auto multi = run_multi_axis_search<M>(g, s_start, s_goal);
    std::cout << "\n  [Strategy 2: Multi-Axis Dual Projections Tr_1 + Tr_M]:\n"
              << "      Runtime:               " << std::setprecision(2) << multi.runtime_ms << " ms\n"
              << "      Expansions:            " << multi.expansions << "\n"
              << "      Tr_1 Checks:           " << multi.tr1_checks << "\n"
              << "      Tr_M Checks:           " << multi.trM_checks << "\n"
              << "      FALLBACKS AVOIDED:     " << multi.fallbacks_avoided << " (Resolved without full scan!)\n"
              << "      Remaining Fallbacks:   " << multi.remaining_fallbacks << "\n"
              << "      Scalar Comparisons:    " << multi.scalar_cmps << "\n"
              << "      Pareto Solutions:      " << multi.solutions.size() << "\n";

    // Soundness check: exactly identical solutions
    bool match = (single.solutions.size() == multi.solutions.size());
    if (match) {
        for (size_t i = 0; i < single.solutions.size(); ++i) {
            if (!(single.solutions[i] == multi.solutions[i])) { match = false; break; }
        }
    }
    std::cout << "\n  [Soundness Verification]: Bit-Identical Pareto Front = " 
              << (match ? "TRUE (100% SOUND)" : "FALSE") << "\n";

    if (single.full_fallbacks > 0) {
        double avoid_rate = ((double)multi.fallbacks_avoided / (double)single.full_fallbacks) * 100.0;
        std::cout << "  >>> Fallback Avoidance Rate: " << std::setprecision(1) << avoid_rate << "%\n";
    }

    double speedup = single.runtime_ms / multi.runtime_ms;
    std::cout << "  >>> Wall-Clock Speedup: " << std::setprecision(2) << speedup << "x " 
              << (speedup >= 1.0 ? "FASTER" : "SLOWER (Dual tree update overhead exceeds fallback savings)") << "\n";
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << "CRITICAL C++ BENCHMARK: MULTI-AXIS K-D TREE / COORDINATE PROJECTIONS\n";
    std::cout << "========================================================================\n";

    execute_investigation_3<4>(6, 8, 42);   // M=4, dense DAG
    execute_investigation_3<4>(8, 7, 101);  // M=4, deeper DAG
    execute_investigation_3<5>(6, 7, 77);   // M=5, dense DAG
    execute_investigation_3<5>(7, 7, 999);  // M=5, deeper DAG
    execute_investigation_3<6>(6, 6, 123);  // M=6, dense DAG

    return 0;
}
