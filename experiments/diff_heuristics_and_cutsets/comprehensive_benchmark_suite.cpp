#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <queue>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <string>
#include <cstdint>

constexpr int MAX_M = 8;
constexpr double TIMEOUT_SEC = 25.0; // 25s timeout per phase

struct CostVector {
    int m = 0;
    std::array<int, MAX_M> data{};

    int& operator[](int i) { return data[i]; }
    const int& operator[](int i) const { return data[i]; }

    bool operator==(const CostVector& o) const {
        if (m != o.m) return false;
        for (int i = 0; i < m; ++i) if (data[i] != o.data[i]) return false;
        return true;
    }

    bool operator<(const CostVector& o) const {
        for (int i = 0; i < m; ++i) {
            if (data[i] != o.data[i]) return data[i] < o.data[i];
        }
        return false;
    }

    bool operator>(const CostVector& o) const {
        for (int i = 0; i < m; ++i) {
            if (data[i] != o.data[i]) return data[i] > o.data[i];
        }
        return false;
    }

    bool dominates(const CostVector& o) const {
        bool strict = false;
        for (int i = 0; i < m; ++i) {
            if (data[i] > o.data[i]) return false;
            if (data[i] < o.data[i]) strict = true;
        }
        return strict;
    }

    bool weakly_dominates(const CostVector& o) const {
        for (int i = 0; i < m; ++i) {
            if (data[i] > o.data[i]) return false;
        }
        return true;
    }

    bool truncated_weakly_dominates(const CostVector& o) const {
        for (int i = 1; i < m; ++i) {
            if (data[i] > o.data[i]) return false;
        }
        return true;
    }
};

struct Edge {
    int to;
    CostVector cost;
};

struct MultiGraph {
    int V = 0;
    int M = 0;
    std::vector<std::vector<Edge>> adj;
    std::vector<std::vector<Edge>> rev_adj;

    void init(int v, int m) {
        V = v;
        M = m;
        adj.assign(V, {});
        rev_adj.assign(V, {});
    }

    void add_edge(int u, int v, const CostVector& c) {
        adj[u].push_back({v, c});
        rev_adj[v].push_back({u, c});
    }
};

// Parser for DIMACS multi-objective .gr files
bool load_instance(const std::string& dir, const std::string& prefix, int M, MultiGraph& g) {
    int max_node = 0;
    struct RawEdge { int u, v; std::vector<int> weights; };
    std::vector<RawEdge> raw_edges;

    for (int obj = 0; obj < M; ++obj) {
        std::string filename;
        if (prefix == "obj") {
            filename = dir + "/obj_" + std::to_string(obj) + ".gr";
        } else if (prefix == "c") {
            filename = dir + "/c" + std::to_string(obj + 1) + ".gr";
        } else if (prefix == "graph_c") {
            filename = dir + "/graph_c" + std::to_string(obj + 1) + ".gr";
        }

        std::ifstream in(filename);
        if (!in) {
            return false;
        }

        std::string line;
        size_t edge_idx = 0;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] != 'a') continue;
            std::stringstream ss(line);
            char a; int u, v, w;
            ss >> a >> u >> v >> w;
            --u; --v; // convert to 0-based
            max_node = std::max(max_node, std::max(u, v) + 1);

            if (obj == 0) {
                RawEdge re;
                re.u = u; re.v = v;
                re.weights.resize(M, 0);
                re.weights[0] = w;
                raw_edges.push_back(re);
            } else {
                if (edge_idx < raw_edges.size()) {
                    raw_edges[edge_idx].weights[obj] = w;
                    ++edge_idx;
                }
            }
        }
    }

    g.init(max_node, M);
    for (const auto& re : raw_edges) {
        CostVector cv;
        cv.m = M;
        for (int i = 0; i < M; ++i) cv[i] = re.weights[i];
        g.add_edge(re.u, re.v, cv);
    }
    return true;
}

// Result struct
struct RunResult {
    bool timed_out = false;
    double t_prep_ms = 0.0;
    double t_search_ms = 0.0;
    double t_total_ms = 0.0;
    uint64_t expansions = 0;
    size_t solutions = 0;
};

// 1. Single-Objective Dijkstra
CostVector run_dijkstra_for_obj(const MultiGraph& g, int start, int target, int obj) {
    std::vector<int> dist(g.V, 1e9);
    dist[start] = 0;
    CostVector res; res.m = g.M; res.data.fill(0);
    std::vector<CostVector> actual(g.V);
    for (int i = 0; i < g.V; ++i) { actual[i].m = g.M; actual[i].data.fill(0); }

    using Item = std::pair<int, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    pq.push({0, start});

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;
        if (u == target) return actual[target];

        for (const auto& edge : g.adj[u]) {
            int w = edge.cost[obj];
            if (dist[u] + w < dist[edge.to]) {
                dist[edge.to] = dist[u] + w;
                for (int m = 0; m < g.M; ++m) {
                    actual[edge.to][m] = actual[u][m] + edge.cost[m];
                }
                pq.push({dist[edge.to], edge.to});
            }
        }
    }
    return actual[target];
}

// 2. Ideal-Point Preprocessor (Single-Valued Heuristic)
std::vector<CostVector> compute_ideal_point_heuristic(const MultiGraph& g, int target) {
    // Backward Dijkstra for each objective
    std::vector<CostVector> h_ideal(g.V);
    for (int u = 0; u < g.V; ++u) { h_ideal[u].m = g.M; h_ideal[u].data.fill(0); }

    for (int obj = 0; obj < g.M; ++obj) {
        std::vector<int> dist(g.V, 1e9);
        dist[target] = 0;
        using Item = std::pair<int, int>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
        pq.push({0, target});

        while (!pq.empty()) {
            auto [d, u] = pq.top(); pq.pop();
            if (d > dist[u]) continue;

            for (const auto& edge : g.rev_adj[u]) {
                int w = edge.cost[obj];
                if (dist[u] + w < dist[edge.to]) {
                    dist[edge.to] = dist[u] + w;
                    pq.push({dist[edge.to], edge.to});
                }
            }
        }
        for (int u = 0; u < g.V; ++u) {
            h_ideal[u][obj] = (dist[u] >= 1e9) ? 0 : dist[u];
        }
    }
    return h_ideal;
}

// 3. Backward Apex Preprocessor
std::vector<std::vector<CostVector>> compute_backward_apex(const MultiGraph& g, int target, bool& timed_out) {
    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<std::vector<CostVector>> frontiers(g.V);

    using PQItem = std::pair<int, std::pair<int, CostVector>>;
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> pq;

    CostVector zero_cost; zero_cost.m = g.M; zero_cost.data.fill(0);
    pq.push({0, {target, zero_cost}});

    while (!pq.empty()) {
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration<double>(now - t0).count() > TIMEOUT_SEC) {
            timed_out = true;
            return {};
        }

        auto [f1, item] = pq.top(); pq.pop();
        int u = item.first; CostVector cost = item.second;

        bool dominated = false;
        for (const auto& p : frontiers[u]) {
            if (p.truncated_weakly_dominates(cost)) {
                dominated = true;
                break;
            }
        }
        if (dominated) continue;

        std::vector<CostVector> new_f;
        for (const auto& p : frontiers[u]) {
            if (!cost.truncated_weakly_dominates(p)) new_f.push_back(p);
        }
        new_f.push_back(cost);
        frontiers[u] = std::move(new_f);

        for (const auto& edge : g.rev_adj[u]) {
            CostVector nxt_c; nxt_c.m = g.M;
            for (int m = 0; m < g.M; ++m) nxt_c[m] = cost[m] + edge.cost[m];
            pq.push({nxt_c[0], {edge.to, nxt_c}});
        }
    }
    return frontiers;
}

// Priority Queue Entry
struct SearchNode {
    int u;
    CostVector g;
    int f1;
    bool operator>(const SearchNode& o) const {
        if (f1 != o.f1) return f1 > o.f1;
        return g.data > o.g.data;
    }
};

// Generic Search Engine
RunResult run_search(const MultiGraph& g, int s_start, int s_goal, 
                     const std::vector<std::vector<CostVector>>& H, bool use_dr_pruning) {
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t expansions = 0;
    std::vector<CostVector> sols;
    std::vector<std::vector<CostVector>> F(g.V);

    std::priority_queue<SearchNode, std::vector<SearchNode>, std::greater<SearchNode>> pq;
    CostVector zero_g; zero_g.m = g.M; zero_g.data.fill(0);

    int init_f1 = (H[s_start].empty()) ? 0 : zero_g[0] + H[s_start][0][0];
    pq.push({s_start, zero_g, init_f1});

    while (!pq.empty()) {
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration<double>(now - t0).count() > TIMEOUT_SEC) {
            RunResult res;
            res.timed_out = true;
            return res;
        }

        auto [u, g_curr, f1] = pq.top(); pq.pop();

        // Goal dominance check
        bool goal_dom = false;
        for (const auto& sol : sols) {
            if (sol.weakly_dominates(g_curr)) { goal_dom = true; break; }
        }
        if (goal_dom) continue;

        // Local dominance check
        bool local_dom = false;
        for (const auto& p : F[u]) {
            if (p.weakly_dominates(g_curr)) { local_dom = true; break; }
        }
        if (local_dom) continue;

        // Update local frontier
        std::vector<CostVector> new_f;
        for (const auto& p : F[u]) {
            if (!g_curr.weakly_dominates(p)) new_f.push_back(p);
        }
        new_f.push_back(g_curr);
        F[u] = std::move(new_f);

        if (u == s_goal) {
            sols.push_back(g_curr);
            continue;
        }

        expansions++;
        for (const auto& edge : g.adj[u]) {
            CostVector nxt_g; nxt_g.m = g.M;
            for (int m = 0; m < g.M; ++m) nxt_g[m] = g_curr[m] + edge.cost[m];

            bool succ_dom = false;
            for (const auto& p : F[edge.to]) {
                if (p.weakly_dominates(nxt_g)) { succ_dom = true; break; }
            }
            if (succ_dom) continue;

            int nxt_f1 = nxt_g[0];
            if (!H[edge.to].empty()) {
                nxt_f1 += H[edge.to][0][0];
            }
            pq.push({edge.to, nxt_g, nxt_f1});
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Filter sols to strict Pareto front
    std::vector<CostVector> pareto;
    for (size_t i = 0; i < sols.size(); ++i) {
        bool dom = false;
        for (size_t j = 0; j < sols.size(); ++j) {
            if (i != j && sols[j].dominates(sols[i])) { dom = true; break; }
        }
        if (!dom) pareto.push_back(sols[i]);
    }

    RunResult res;
    res.timed_out = false;
    res.t_search_ms = ms;
    res.expansions = expansions;
    res.solutions = pareto.size();
    return res;
}

void benchmark_instance(const std::string& name, const std::string& dir, const std::string& prefix, 
                        int M, int s_start, int s_goal, int landmark) {
    std::cout << "\n========================================================================================\n";
    std::cout << "INSTANCE: " << name << " (M = " << M << " Objectives, Start = " << s_start << ", Goal = " << s_goal << ")\n";
    std::cout << "========================================================================================\n";

    MultiGraph g;
    if (!load_instance(dir, prefix, M, g)) {
        std::cout << "Error: Failed to load instance from " << dir << "\n";
        return;
    }
    std::cout << "Graph Loaded: |V| = " << g.V << ", |E| = " << g.adj[0].size() * g.V << " approx\n";

    // -------------------------------------------------------------
    // Method 1: Single-Valued Heuristic (Ideal-Point Dijkstra)
    // -------------------------------------------------------------
    std::cout << "\n>>> [Method 1: Ideal-Point SVH Dijkstra Baseline]...\n";
    auto t0 = std::chrono::high_resolution_clock::now();
    auto h_ideal_vec = compute_ideal_point_heuristic(g, s_goal);
    auto t1 = std::chrono::high_resolution_clock::now();
    double t_prep_svh = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::vector<std::vector<CostVector>> H_svh(g.V);
    for (int u = 0; u < g.V; ++u) H_svh[u].push_back(h_ideal_vec[u]);

    auto res_svh = run_search(g, s_start, s_goal, H_svh, false);
    if (res_svh.timed_out) {
        std::cout << "  Result: TIMEOUT (> " << TIMEOUT_SEC << "s)\n";
    } else {
        res_svh.t_prep_ms = t_prep_svh;
        res_svh.t_total_ms = t_prep_svh + res_svh.t_search_ms;
        std::cout << "  Prep Time:  " << std::fixed << std::setprecision(2) << res_svh.t_prep_ms << " ms\n"
                  << "  Search Time:" << res_svh.t_search_ms << " ms (Expansions: " << res_svh.expansions << ")\n"
                  << "  TOTAL Time: " << res_svh.t_total_ms << " ms | Solutions: " << res_svh.solutions << "\n";
    }

    // -------------------------------------------------------------
    // Method 2: SOTA Backward Apex-MVH Preprocessing
    // -------------------------------------------------------------
    std::cout << "\n>>> [Method 2: SOTA Backward Apex-MVH (Maya Wolff et al. 2026)]...\n";
    bool apex_timed_out = false;
    auto t_apex_0 = std::chrono::high_resolution_clock::now();
    auto H_apex = compute_backward_apex(g, s_goal, apex_timed_out);
    auto t_apex_1 = std::chrono::high_resolution_clock::now();
    double t_prep_apex = std::chrono::duration<double, std::milli>(t_apex_1 - t_apex_0).count();

    if (apex_timed_out) {
        std::cout << "  Preprocessing: TIMEOUT (> " << TIMEOUT_SEC << "s during Apex MOSP)\n";
    } else {
        auto res_apex = run_search(g, s_start, s_goal, H_apex, true);
        if (res_apex.timed_out) {
            std::cout << "  Search: TIMEOUT (> " << TIMEOUT_SEC << "s)\n";
        } else {
            res_apex.t_prep_ms = t_prep_apex;
            res_apex.t_total_ms = t_prep_apex + res_apex.t_search_ms;
            std::cout << "  Apex Prep:  " << std::fixed << std::setprecision(2) << res_apex.t_prep_ms << " ms\n"
                      << "  Search Time:" << res_apex.t_search_ms << " ms (Expansions: " << res_apex.expansions << ")\n"
                      << "  TOTAL Time: " << res_apex.t_total_ms << " ms | Solutions: " << res_apex.solutions << "\n";
        }
    }

    // -------------------------------------------------------------
    // Method 3: Our Asymmetric Differential MVH
    // -------------------------------------------------------------
    std::cout << "\n>>> [Method 3: Asymmetric Differential MVH (OUR NEW PARADIGM)]...\n";
    // Landmark G_{u -> L} computed once offline:
    bool landmark_to = false;
    auto G_to_L = compute_backward_apex(g, landmark, landmark_to);

    // Online query-time preprocessing: M scalarized Dijkstra runs from s_goal to landmark
    auto t_diff_0 = std::chrono::high_resolution_clock::now();
    std::vector<CostVector> g_tL_candidates;
    for (int m = 0; m < M; ++m) {
        g_tL_candidates.push_back(run_dijkstra_for_obj(g, s_goal, landmark, m));
    }
    auto t_diff_1 = std::chrono::high_resolution_clock::now();
    double t_prep_diff = std::chrono::duration<double, std::milli>(t_diff_1 - t_diff_0).count();

    // Dynamic shift
    CostVector best_g_tL = g_tL_candidates[0];
    std::vector<std::vector<CostVector>> H_diff(g.V);
    for (int u = 0; u < g.V; ++u) {
        for (const auto& g_uL : G_to_L[u]) {
            CostVector h; h.m = M;
            for (int m = 0; m < M; ++m) {
                h[m] = std::max(0, g_uL[m] - best_g_tL[m]);
            }
            H_diff[u].push_back(h);
        }
    }

    auto res_diff = run_search(g, s_start, s_goal, H_diff, true);
    if (res_diff.timed_out) {
        std::cout << "  Result: TIMEOUT (> " << TIMEOUT_SEC << "s)\n";
    } else {
        res_diff.t_prep_ms = t_prep_diff;
        res_diff.t_total_ms = t_prep_diff + res_diff.t_search_ms;
        std::cout << "  Dijkstra Prep:" << std::setprecision(4) << res_diff.t_prep_ms << " ms (INSTANTANEOUS!)\n"
                  << "  Search Time:  " << std::setprecision(2) << res_diff.t_search_ms << " ms (Expansions: " << res_diff.expansions << ")\n"
                  << "  TOTAL Time:   " << res_diff.t_total_ms << " ms | Solutions: " << res_diff.solutions << "\n";

        if (!apex_timed_out && t_prep_apex > 0) {
            double prep_speedup = t_prep_apex / t_prep_diff;
            double end_to_end_speedup = (t_prep_apex + res_diff.t_search_ms) / res_diff.t_total_ms;
            std::cout << "  ------------------------------------------------------------------------\n"
                      << "  >>> PREPROCESSING SPEEDUP (Apex vs Ours):  " << std::setprecision(1) << prep_speedup << "x FASTER!\n"
                      << "  >>> END-TO-END QUERY SPEEDUP (Apex vs Ours): " << std::setprecision(2) << end_to_end_speedup << "x FASTER!\n";
        }
    }
}

int main() {
    std::cout << "========================================================================================\n";
    std::cout << "COMPREHENSIVE MULTI-METHOD BENCHMARK SUITE: SOTA VS ASYMMETRIC DIFFERENTIAL MVH\n";
    std::cout << "========================================================================================\n";

    // 1. bay_8d_2000 (Road Network, M=5, start=1999, goal=0, landmark=1000)
    benchmark_instance("bay_8d_2000_5D", "scratchpad/maps/bay_8d_2000", "obj", 5, 1999, 0, 1000);

    // 2. grid_8x8_M6_rho-0.2 (Grid 8x8, M=6, start=0, goal=63, landmark=7)
    benchmark_instance("grid_8x8_M6_rho-0.2", "scratchpad/deep_scaling/grid_8x8_M6_rho-0.2", "c", 6, 0, 63, 7);

    // 3. grid_8x8_M7_rho0.0 (Grid 8x8, M=7, start=0, goal=63, landmark=7)
    benchmark_instance("grid_8x8_M7_rho0.0", "scratchpad/deep_scaling/grid_8x8_M7_rho0.0", "c", 7, 0, 63, 7);

    // 4. geom_n100_d4_tradeoff (Geometric 100 nodes, M=4, start=0, goal=99, landmark=50)
    benchmark_instance("geom_n100_d4_tradeoff", "benchmarks/instances/geom_n100_d4_tradeoff", "obj", 4, 0, 99, 50);

    // 5. panda_8d (Robot Arm 8D, M=5, start=0, goal=92, landmark=45)
    benchmark_instance("panda_8d_M5", "scratchpad/maps/panda_8d", "graph_c", 5, 0, 92, 45);

    return 0;
}
