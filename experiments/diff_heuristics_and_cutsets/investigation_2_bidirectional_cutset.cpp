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

    bool truncated_weakly_dominates(const CostVec<M>& o) const {
        for (int i = 1; i < M; ++i) {
            if (data[i] > o.data[i]) return false;
        }
        return true;
    }
};

template <int M>
struct LayeredGraph {
    int V;
    int num_stages;
    int stage_width;
    std::vector<std::vector<int>> stage_nodes;
    struct Edge {
        int to;
        CostVec<M> cost;
    };
    std::vector<std::vector<Edge>> adj;
    std::vector<std::vector<Edge>> rev_adj;
    int s_start;
    int s_goal;
    std::vector<int> cut_nodes; // Middle stage nodes

    LayeredGraph(int stages, int width, int seed = 42) 
        : num_stages(stages), stage_width(width) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist_base(2, 25);
        std::uniform_int_distribution<int> dist_anti(1, 30);

        int curr_id = 0;
        stage_nodes.resize(stages);

        // Stage 0: Start node
        s_start = curr_id++;
        stage_nodes[0] = {s_start};

        // Middle stages
        int mid_stage = stages / 2;
        for (int s = 1; s < stages - 1; ++s) {
            int w = (s == mid_stage) ? std::min(3, width) : width; // Bottleneck at middle stage
            stage_nodes[s].resize(w);
            for (int i = 0; i < w; ++i) {
                stage_nodes[s][i] = curr_id++;
            }
        }

        // Last stage: Goal node
        s_goal = curr_id++;
        stage_nodes[stages - 1] = {s_goal};

        V = curr_id;
        adj.resize(V);
        rev_adj.resize(V);
        cut_nodes = stage_nodes[mid_stage];

        // Connect adjacent stages with anti-correlated cost vectors
        for (int s = 0; s < stages - 1; ++s) {
            for (int u : stage_nodes[s]) {
                for (int v : stage_nodes[s + 1]) {
                    CostVec<M> c;
                    c[0] = dist_anti(rng);
                    c[1] = std::max(1, 32 - c[0] + (int)(rng() % 5) - 2); // Strong trade-off
                    for (int m = 2; m < M; ++m) {
                        c[m] = dist_base(rng);
                    }
                    adj[u].push_back({v, c});
                    rev_adj[v].push_back({u, c});
                }
            }
        }
    }
};

// Exact Pareto filtering helper
template <int M>
std::vector<CostVec<M>> pareto_filter(std::vector<CostVec<M>> pts) {
    if (pts.empty()) return {};
    std::sort(pts.begin(), pts.end());
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());

    std::vector<CostVec<M>> res;
    for (size_t i = 0; i < pts.size(); ++i) {
        bool dom = false;
        for (size_t j = 0; j < pts.size(); ++j) {
            if (i != j && pts[j].dominates(pts[i])) {
                dom = true;
                break;
            }
        }
        if (!dom) res.push_back(pts[i]);
    }
    return res;
}

// -------------------------------------------------------------
// Unidirectional Search (Monolithic FAST-MVH Oracle)
// -------------------------------------------------------------
template <int M>
struct UniSearchResult {
    double runtime_ms;
    uint64_t expansions;
    uint64_t dominance_checks;
    std::vector<CostVec<M>> solutions;
};

template <int M>
UniSearchResult<M> run_unidirectional_search(const LayeredGraph<M>& g) {
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t expansions = 0;
    uint64_t dom_checks = 0;

    std::vector<CostVec<M>> sols;
    std::vector<std::vector<CostVec<M>>> F(g.V);

    struct Node {
        int u;
        CostVec<M> g;
        bool operator>(const Node& o) const {
            if (g[0] != o.g[0]) return g[0] > o.g[0];
            return g.data > o.g.data;
        }
    };

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    CostVec<M> zero_g;
    zero_g.data.fill(0);
    pq.push({g.s_start, zero_g});

    while (!pq.empty()) {
        auto [u, g_curr] = pq.top();
        pq.pop();

        // Goal dominance check
        bool goal_dom = false;
        for (const auto& sol : sols) {
            dom_checks++;
            if (sol.weakly_dominates(g_curr)) {
                goal_dom = true;
                break;
            }
        }
        if (goal_dom) continue;

        // Local dominance check
        bool local_dom = false;
        for (const auto& p : F[u]) {
            dom_checks++;
            if (p.weakly_dominates(g_curr)) {
                local_dom = true;
                break;
            }
        }
        if (local_dom) continue;

        // Update local frontier
        std::vector<CostVec<M>> new_f;
        for (const auto& p : F[u]) {
            dom_checks++;
            if (!g_curr.weakly_dominates(p)) new_f.push_back(p);
        }
        new_f.push_back(g_curr);
        F[u] = std::move(new_f);

        if (u == g.s_goal) {
            sols.push_back(g_curr);
            continue;
        }

        expansions++;
        for (const auto& edge : g.adj[u]) {
            CostVec<M> nxt_g;
            for (int m = 0; m < M; ++m) nxt_g[m] = g_curr[m] + edge.cost[m];

            bool succ_dom = false;
            for (const auto& p : F[edge.to]) {
                dom_checks++;
                if (p.weakly_dominates(nxt_g)) {
                    succ_dom = true;
                    break;
                }
            }
            if (!succ_dom) {
                pq.push({edge.to, nxt_g});
            }
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return {ms, expansions, dom_checks, pareto_filter<M>(sols)};
}

// -------------------------------------------------------------
// Bidirectional / Cut-Set Search with Architecture 2
// (Eager Stitching into Witness-Checked T + Residue R(goal))
// -------------------------------------------------------------
template <int M>
struct BidiSearchResult {
    double backward_ms;
    double forward_ms;
    double total_ms;
    uint64_t bwd_expansions;
    uint64_t fwd_expansions;
    uint64_t total_expansions;
    uint64_t dom_checks;
    uint64_t eager_prunes;
    std::vector<CostVec<M>> solutions;
};

template <int M>
BidiSearchResult<M> run_bidirectional_cutset_search(const LayeredGraph<M>& g) {
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t bwd_expansions = 0;
    uint64_t fwd_expansions = 0;
    uint64_t dom_checks = 0;
    uint64_t eager_prunes = 0;

    // STEP 1: Backward Search from Goal to Cut-Set
    // Computes exact backward Pareto frontiers F_bwd(L_i -> Goal)
    std::vector<std::vector<CostVec<M>>> F_bwd(g.V);
    {
        struct BwdNode {
            int u;
            CostVec<M> cost;
            bool operator>(const BwdNode& o) const {
                if (cost[0] != o.cost[0]) return cost[0] > o.cost[0];
                return cost.data > o.cost.data;
            }
        };
        std::priority_queue<BwdNode, std::vector<BwdNode>, std::greater<BwdNode>> bpq;
        CostVec<M> zero_c; zero_c.data.fill(0);
        bpq.push({g.s_goal, zero_c});

        while (!bpq.empty()) {
            auto [u, cost] = bpq.top();
            bpq.pop();

            bool dom = false;
            for (const auto& p : F_bwd[u]) {
                dom_checks++;
                if (p.weakly_dominates(cost)) { dom = true; break; }
            }
            if (dom) continue;

            std::vector<CostVec<M>> new_f;
            for (const auto& p : F_bwd[u]) {
                dom_checks++;
                if (!cost.weakly_dominates(p)) new_f.push_back(p);
            }
            new_f.push_back(cost);
            F_bwd[u] = std::move(new_f);

            // Do not expand beyond cut nodes in backward stage
            bool is_cut = false;
            for (int cut_node : g.cut_nodes) {
                if (u == cut_node) { is_cut = true; break; }
            }
            if (is_cut) continue;

            bwd_expansions++;
            for (const auto& edge : g.rev_adj[u]) {
                CostVec<M> nxt_c;
                for (int m = 0; m < M; ++m) nxt_c[m] = cost[m] + edge.cost[m];
                bpq.push({edge.to, nxt_c});
            }
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double bwd_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // STEP 2: Forward Search from Start to Cut-Set
    // With Architecture 2: Eager Stitching + Full Witness Check + Residue Forest R(goal)
    auto t2 = std::chrono::high_resolution_clock::now();
    std::vector<CostVec<M>> T_front;    // F(goal) truncated frontier
    std::vector<CostVec<M>> R_goal;     // Residue set (full M coordinates)
    std::vector<CostVec<M>> final_sols; // Realizable goal solutions
    std::vector<std::vector<CostVec<M>>> F_fwd(g.V);

    // Witness-Checked GoalDom predicate
    auto goal_dom_with_residue = [&](const CostVec<M>& q) -> bool {
        bool has_tr_dominator = false;
        for (const auto& p : T_front) {
            dom_checks++;
            if (p.truncated_weakly_dominates(q)) {
                if (p[0] <= q[0]) {
                    return true; // FULL WITNESS!
                }
                has_tr_dominator = true;
            }
        }
        if (!has_tr_dominator) return false;

        // Fallback to exact residue forest
        for (const auto& r : R_goal) {
            dom_checks++;
            if (r.weakly_dominates(q)) return true;
        }
        return false;
    };

    // Update goal frontier with eager upper bounds (Algorithm 1 Update)
    auto update_goal_frontier = [&](const CostVec<M>& stitched) {
        std::vector<CostVec<M>> next_T;
        for (const auto& p : T_front) {
            dom_checks++;
            if (stitched.truncated_weakly_dominates(p)) {
                // p is evicted
                if (p[0] < stitched[0]) {
                    R_goal.push_back(p); // Retain in residue
                }
            } else {
                next_T.push_back(p);
            }
        }
        next_T.push_back(stitched);
        T_front = std::move(next_T);
    };

    struct FwdNode {
        int u;
        CostVec<M> g;
        bool operator>(const FwdNode& o) const {
            if (g[0] != o.g[0]) return g[0] > o.g[0];
            return g.data > o.g.data;
        }
    };
    std::priority_queue<FwdNode, std::vector<FwdNode>, std::greater<FwdNode>> fpq;
    CostVec<M> zero_g; zero_g.data.fill(0);
    fpq.push({g.s_start, zero_g});

    while (!fpq.empty()) {
        auto [u, g_curr] = fpq.top();
        fpq.pop();

        // 1. GoalDom check with Witness-Checked T + Residue
        if (goal_dom_with_residue(g_curr)) {
            eager_prunes++;
            continue;
        }

        // 2. Local dominance check
        bool local_dom = false;
        for (const auto& p : F_fwd[u]) {
            dom_checks++;
            if (p.weakly_dominates(g_curr)) { local_dom = true; break; }
        }
        if (local_dom) continue;

        std::vector<CostVec<M>> new_f;
        for (const auto& p : F_fwd[u]) {
            dom_checks++;
            if (!g_curr.weakly_dominates(p)) new_f.push_back(p);
        }
        new_f.push_back(g_curr);
        F_fwd[u] = std::move(new_f);

        // Check if u is a cut node
        bool is_cut = false;
        for (int cut_node : g.cut_nodes) {
            if (u == cut_node) { is_cut = true; break; }
        }

        if (is_cut) {
            // EAGER STITCHING: Immediately combine g_curr with F_bwd[u]!
            for (const auto& h_bwd : F_bwd[u]) {
                CostVec<M> full_sol;
                for (int m = 0; m < M; ++m) full_sol[m] = g_curr[m] + h_bwd[m];
                final_sols.push_back(full_sol);
                update_goal_frontier(full_sol);
            }
            continue; // Cut nodes terminate forward search
        }

        fwd_expansions++;
        for (const auto& edge : g.adj[u]) {
            CostVec<M> nxt_g;
            for (int m = 0; m < M; ++m) nxt_g[m] = g_curr[m] + edge.cost[m];

            // Local dominance check before insertion
            bool succ_dom = false;
            for (const auto& p : F_fwd[edge.to]) {
                dom_checks++;
                if (p.weakly_dominates(nxt_g)) { succ_dom = true; break; }
            }
            if (!succ_dom) {
                fpq.push({edge.to, nxt_g});
            }
        }
    }

    auto t3 = std::chrono::high_resolution_clock::now();
    double fwd_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
    double total_ms = bwd_ms + fwd_ms;

    return {bwd_ms, fwd_ms, total_ms, bwd_expansions, fwd_expansions, 
            bwd_expansions + fwd_expansions, dom_checks, eager_prunes, pareto_filter<M>(final_sols)};
}

template <int M>
void execute_investigation_2(int stages, int width, int seed) {
    std::cout << "\n========================================================================\n";
    std::cout << "INVESTIGATION 2: DEPTH EXPLOSION & BIDIRECTIONAL CUT-SET (M = " << M 
              << ", Stages = " << stages << ", Width = " << width << ")\n";
    std::cout << "========================================================================\n";

    LayeredGraph<M> g(stages, width, seed);
    std::cout << "Graph Topology: Total Vertices = " << g.V 
              << " | Middle Cut-Set Size = " << g.cut_nodes.size() 
              << " | Path Depth = " << stages - 1 << " stages\n";

    // 1. Unidirectional Monolithic Search
    std::cout << "\n>>> Running Unidirectional FAST-MVH Search...\n";
    auto uni = run_unidirectional_search<M>(g);
    std::cout << "  [Unidirectional Search]: Runtime = " << std::fixed << std::setprecision(2) << uni.runtime_ms << " ms"
              << " | Expansions = " << uni.expansions 
              << " | Dom Checks = " << uni.dominance_checks 
              << " | Pareto Solutions = " << uni.solutions.size() << "\n";

    // 2. Bidirectional Cut-Set Search with Architecture 2
    std::cout << "\n>>> Running Bidirectional Cut-Set Search with Architecture 2...\n";
    auto bidi = run_bidirectional_cutset_search<M>(g);
    std::cout << "  [Bidirectional Search]:\n"
              << "      Backward Stage (Goal->Cut): " << std::setprecision(2) << bidi.backward_ms << " ms (Exp: " << bidi.bwd_expansions << ")\n"
              << "      Forward Stage (Start->Cut):  " << bidi.forward_ms << " ms (Exp: " << bidi.fwd_expansions << " | Eager Prunes: " << bidi.eager_prunes << ")\n"
              << "      TOTAL Runtime:               " << bidi.total_ms << " ms (Total Exp: " << bidi.total_expansions << ")\n"
              << "      Pareto Solutions:            " << bidi.solutions.size() << "\n";

    // Soundness check: exactly identical solutions
    bool match = (uni.solutions.size() == bidi.solutions.size());
    if (match) {
        for (size_t i = 0; i < uni.solutions.size(); ++i) {
            if (!(uni.solutions[i] == bidi.solutions[i])) { match = false; break; }
        }
    }
    std::cout << "\n  [Soundness Verification]: Bit-Identical Pareto Front = " 
              << (match ? "TRUE (100% SOUND)" : "FALSE (CORRUPTED)") << "\n";

    double speedup = uni.runtime_ms / bidi.total_ms;
    double exp_reduction = (double)uni.expansions / (double)bidi.total_expansions;

    std::cout << "  ************************************************************************\n";
    std::cout << "  >>> EXPANSION REDUCTION: " << std::fixed << std::setprecision(2) << exp_reduction << "x FEWER EXPANSIONS!\n";
    std::cout << "  >>> WALL-CLOCK SPEEDUP:  " << std::fixed << std::setprecision(2) << speedup << "x SPEEDUP OVER UNIDIRECTIONAL SEARCH!\n";
    std::cout << "  ************************************************************************\n";
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << "CRITICAL C++ BENCHMARK: BREAKING SEARCH DEPTH EXPLOSION VIA CUT-SETS\n";
    std::cout << "========================================================================\n";

    // Test on increasingly deep networks
    execute_investigation_2<4>(6, 6, 42);   // Depth 5, M=4
    execute_investigation_2<4>(8, 6, 101);  // Depth 7, M=4
    execute_investigation_2<4>(10, 6, 777); // Depth 9, M=4 (Deep network)
    execute_investigation_2<5>(6, 6, 77);   // Depth 5, M=5
    execute_investigation_2<5>(8, 6, 999);  // Depth 7, M=5

    return 0;
}
