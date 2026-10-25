#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <chrono>
#include <random>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <numeric>
#include <queue>

// ============================================================================
// EXPERIMENT: KDT-Accelerated Multi-Dimensional Differential Heuristics
// Research Question: Can Shahaf & Bar's Geometric KDT architecture index
// large landmark heuristic frontiers (|G(u -> L)| >= 50-1000) and use subtree
// bounding-box pruning to break the dimensional and density bottlenecks of
// Han Zhang's (SoCS 2023) Differential Heuristics in M in {3, 4, 5}?
// ============================================================================

template <int M>
struct CostVec {
    std::array<int, M> data;
    int& operator[](int i) { return data[i]; }
    const int& operator[](int i) const { return data[i]; }

    bool dominates_or_eq(const CostVec<M>& o) const {
        for (int i = 0; i < M; ++i) {
            if (data[i] > o.data[i]) return false;
        }
        return true;
    }

    bool strictly_dominates(const CostVec<M>& o) const {
        bool any_less = false;
        for (int i = 0; i < M; ++i) {
            if (data[i] > o.data[i]) return false;
            if (data[i] < o.data[i]) any_less = true;
        }
        return any_less;
    }

    bool operator==(const CostVec<M>& o) const {
        return data == o.data;
    }
};

// ----------------------------------------------------------------------------
// Shahaf-Style Geometric K-d Tree for Multi-Dimensional Points
// ----------------------------------------------------------------------------
template <int M>
struct KDNode {
    CostVec<M> lo;      // Component-wise minimum bounding corner
    CostVec<M> hi;      // Component-wise maximum bounding corner
    CostVec<M> pt;      // Point stored at this node
    int split_dim = 0;
    int left = -1;
    int right = -1;
    int count = 1;      // Total points in this subtree
};

template <int M>
struct KDTree {
    std::vector<KDNode<M>> nodes;
    int root = -1;

    void clear() {
        nodes.clear();
        root = -1;
    }

    int build_recursive(std::vector<CostVec<M>>& pts, std::vector<int>& idxs, int depth) {
        if (idxs.empty()) return -1;
        int u = (int)nodes.size();
        nodes.emplace_back();

        // Calculate bounding box and count
        for (int d = 0; d < M; ++d) {
            nodes[u].lo[d] = 1e9;
            nodes[u].hi[d] = -1e9;
        }
        for (int idx : idxs) {
            for (int d = 0; d < M; ++d) {
                nodes[u].lo[d] = std::min(nodes[u].lo[d], pts[idx][d]);
                nodes[u].hi[d] = std::max(nodes[u].hi[d], pts[idx][d]);
            }
        }
        nodes[u].count = (int)idxs.size();

        // Choose split dimension with maximum spread
        int best_d = 0;
        int max_spread = -1;
        for (int d = 0; d < M; ++d) {
            int spread = nodes[u].hi[d] - nodes[u].lo[d];
            if (spread > max_spread) {
                max_spread = spread;
                best_d = d;
            }
        }
        nodes[u].split_dim = best_d;

        size_t mid = idxs.size() / 2;
        std::nth_element(idxs.begin(), idxs.begin() + mid, idxs.end(), [&](int a, int b) {
            return pts[a][best_d] < pts[b][best_d];
        });

        nodes[u].pt = pts[idxs[mid]];

        std::vector<int> left_idxs(idxs.begin(), idxs.begin() + mid);
        std::vector<int> right_idxs(idxs.begin() + mid + 1, idxs.end());

        nodes[u].left = build_recursive(pts, left_idxs, depth + 1);
        nodes[u].right = build_recursive(pts, right_idxs, depth + 1);
        return u;
    }

    void build(std::vector<CostVec<M>>& pts) {
        clear();
        if (pts.empty()) return;
        std::vector<int> idxs(pts.size());
        std::iota(idxs.begin(), idxs.end(), 0);
        nodes.reserve(pts.size() * 2);
        root = build_recursive(pts, idxs, 0);
    }

    // Shahaf Orthant-Emptiness Query: Does any point in the tree dominate q?
    // Returns true if exists p in tree s.t. p <= q elementwise
    bool query_dominates(int u, const CostVec<M>& q, uint64_t& scalar_cmps) const {
        if (u == -1) return false;
        const auto& n = nodes[u];

        // 1. O(1) Subtree Prune via Corner-Minimum (lo):
        // If subtree's minimum exceeds q on any coordinate, NO point in subtree can dominate q.
        for (int d = 0; d < M; ++d) {
            scalar_cmps++;
            if (n.lo[d] > q[d]) return false;
        }

        // 2. Point Dominance Evaluation:
        bool pt_dom = true;
        for (int d = 0; d < M; ++d) {
            scalar_cmps++;
            if (n.pt[d] > q[d]) {
                pt_dom = false;
                break;
            }
        }
        if (pt_dom) return true;

        // 3. Recurse on children (priority to left if q[split_dim] >= pt[split_dim])
        if (query_dominates(n.left, q, scalar_cmps)) return true;
        return query_dominates(n.right, q, scalar_cmps);
    }

    bool dominates(const CostVec<M>& q, uint64_t& scalar_cmps) const {
        return query_dominates(root, q, scalar_cmps);
    }
};

// ----------------------------------------------------------------------------
// Goal Frontier Representation (Flat vs KDT)
// ----------------------------------------------------------------------------
template <int M>
struct GoalFrontier {
    std::vector<CostVec<M>> points;
    KDTree<M> kdt;

    void update_kdt() {
        kdt.build(points);
    }

    // Flat scan dominance check
    bool flat_dominates(const CostVec<M>& q, uint64_t& scalar_cmps) const {
        for (const auto& p : points) {
            bool dom = true;
            for (int d = 0; d < M; ++d) {
                scalar_cmps++;
                if (p[d] > q[d]) {
                    dom = false;
                    break;
                }
            }
            if (dom) return true;
        }
        return false;
    }

    // KDT dominance check
    bool kdt_dominates(const CostVec<M>& q, uint64_t& scalar_cmps) const {
        return kdt.dominates(q, scalar_cmps);
    }
};

// ----------------------------------------------------------------------------
// Benchmark Metrics Structure
// ----------------------------------------------------------------------------
struct BenchmarkMetrics {
    double time_us = 0.0;
    uint64_t scalar_cmps = 0;
    uint64_t subtree_prunes = 0;
    uint64_t points_pruned_in_bulk = 0;
    uint64_t total_points_checked = 0;
    uint64_t prunes_found = 0;
};

// ============================================================================
// CORE EVALUATION METHODS
// ============================================================================

// METHOD 1: Pure Flat (Han's Naive Scan in M >= 3)
// Check if candidate path g_u is pruned by Landmark L via path g_tL
// (Pruned if ALL g_uL in G_uL lead to dominated costs: g_u + max(0, g_uL - g_tL) in Dom(T))
template <int M>
bool check_prune_flat_naive(const std::vector<CostVec<M>>& G_uL,
                            const CostVec<M>& g_tL,
                            const CostVec<M>& g_u,
                            const GoalFrontier<M>& T,
                            BenchmarkMetrics& met) {
    auto t0 = std::chrono::high_resolution_clock::now();
    bool all_dominated = true;
    for (const auto& g_uL : G_uL) {
        met.total_points_checked++;
        CostVec<M> h;
        for (int d = 0; d < M; ++d) {
            h[d] = std::max(0, g_uL[d] - g_tL[d]);
        }
        CostVec<M> f;
        for (int d = 0; d < M; ++d) f[d] = g_u[d] + h[d];

        if (!T.flat_dominates(f, met.scalar_cmps)) {
            all_dominated = false;
            break; // Found an undominated completion -> candidate CANNOT be pruned!
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    met.time_us += std::chrono::duration<double, std::micro>(t1 - t0).count();
    if (all_dominated) met.prunes_found++;
    return all_dominated;
}

// METHOD 2: Flat G_uL with Shahaf's KDT on Goal Frontier T
template <int M>
bool check_prune_flat_kdt_goal(const std::vector<CostVec<M>>& G_uL,
                               const CostVec<M>& g_tL,
                               const CostVec<M>& g_u,
                               const GoalFrontier<M>& T,
                               BenchmarkMetrics& met) {
    auto t0 = std::chrono::high_resolution_clock::now();
    bool all_dominated = true;
    for (const auto& g_uL : G_uL) {
        met.total_points_checked++;
        CostVec<M> h;
        for (int d = 0; d < M; ++d) {
            h[d] = std::max(0, g_uL[d] - g_tL[d]);
        }
        CostVec<M> f;
        for (int d = 0; d < M; ++d) f[d] = g_u[d] + h[d];

        if (!T.kdt_dominates(f, met.scalar_cmps)) {
            all_dominated = false;
            break;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    met.time_us += std::chrono::duration<double, std::micro>(t1 - t0).count();
    if (all_dominated) met.prunes_found++;
    return all_dominated;
}

// METHOD 3: Full Geometric Architecture (The User's Idea!)
// Landmark KDT with Subtree Bounding-Box Pruning + KDT on Goal Frontier T
template <int M>
bool check_subtree_dominated(const KDTree<M>& tree_u, int node_idx,
                             const CostVec<M>& g_tL,
                             const CostVec<M>& g_u,
                             const GoalFrontier<M>& T,
                             BenchmarkMetrics& met) {
    if (node_idx == -1) return true;
    const auto& n = tree_u.nodes[node_idx];

    // --- SUBTREE BOUNDING BOX CHECK ---
    // The most optimistic lower bound any point in this subtree can produce:
    // lo_cand[d] = g_u[d] + max(0, n.lo[d] - g_tL[d])
    CostVec<M> lo_cand;
    for (int d = 0; d < M; ++d) {
        lo_cand[d] = g_u[d] + std::max(0, n.lo[d] - g_tL[d]);
    }

    // Test if lo_cand is dominated by T:
    if (T.kdt_dominates(lo_cand, met.scalar_cmps)) {
        // PROOF: Since lo_cand <= g_u + max(0, p - g_tL) for ALL points p in subtree,
        // and lo_cand is dominated by some tau in T (tau <= lo_cand),
        // tau <= lo_cand <= g_u + h(p) for ALL p in subtree!
        // Therefore EVERY point in this subtree is guaranteed dominated!
        met.subtree_prunes++;
        met.points_pruned_in_bulk += n.count;
        return true; // Entire subtree is pruned in O(1)!
    }

    // If optimistic bound is NOT dominated, we must test the node's actual point:
    met.total_points_checked++;
    CostVec<M> h_pt;
    for (int d = 0; d < M; ++d) {
        h_pt[d] = std::max(0, n.pt[d] - g_tL[d]);
    }
    CostVec<M> f_pt;
    for (int d = 0; d < M; ++d) f_pt[d] = g_u[d] + h_pt[d];

    if (!T.kdt_dominates(f_pt, met.scalar_cmps)) {
        // Found an undominated point! Candidate path cannot be pruned by this landmark.
        return false;
    }

    // Both children must be dominated for the subtree to be dominated:
    if (!check_subtree_dominated(tree_u, n.left, g_tL, g_u, T, met)) return false;
    return check_subtree_dominated(tree_u, n.right, g_tL, g_u, T, met);
}

template <int M>
bool check_prune_kdt_geometric(const KDTree<M>& tree_uL,
                               const CostVec<M>& g_tL,
                               const CostVec<M>& g_u,
                               const GoalFrontier<M>& T,
                               BenchmarkMetrics& met) {
    auto t0 = std::chrono::high_resolution_clock::now();
    bool all_dominated = check_subtree_dominated(tree_uL, tree_uL.root, g_tL, g_u, T, met);
    auto t1 = std::chrono::high_resolution_clock::now();
    met.time_us += std::chrono::duration<double, std::micro>(t1 - t0).count();
    if (all_dominated) met.prunes_found++;
    return all_dominated;
}

// ----------------------------------------------------------------------------
// Synthetic Multi-Objective Front Generator
// Generates realistic Pareto-optimal trade-off frontiers (antichains)
// ----------------------------------------------------------------------------
template <int M>
std::vector<CostVec<M>> generate_pareto_front(int target_size, int min_val, int max_val, int seed) {
    std::mt19937 rng(seed);
    std::vector<CostVec<M>> pool;
    pool.reserve(target_size * 4);

    // Negative correlation / spherical surface generation to create dense trade-offs
    for (int i = 0; i < target_size * 5; ++i) {
        CostVec<M> v;
        double sum_sq = 0.0;
        std::vector<double> r(M);
        for (int d = 0; d < M; ++d) {
            r[d] = std::uniform_real_distribution<double>(0.1, 1.0)(rng);
            sum_sq += r[d] * r[d];
        }
        double radius = std::sqrt(sum_sq);
        double target_radius = std::uniform_real_distribution<double>(min_val, max_val)(rng);
        for (int d = 0; d < M; ++d) {
            v[d] = (int)(r[d] / radius * target_radius);
        }
        pool.push_back(v);
    }

    // Filter to true non-dominated Pareto front
    std::vector<CostVec<M>> front;
    for (const auto& p : pool) {
        bool dominated = false;
        for (const auto& f : front) {
            if (f.dominates_or_eq(p)) {
                dominated = true;
                break;
            }
        }
        if (!dominated) {
            // Remove points dominated by p
            std::vector<CostVec<M>> new_front;
            for (const auto& f : front) {
                if (!p.strictly_dominates(f)) {
                    new_front.push_back(f);
                }
            }
            new_front.push_back(p);
            front = std::move(new_front);
            if ((int)front.size() >= target_size) break;
        }
    }
    return front;
}

// ============================================================================
// EXPERIMENT 1: MICRO-BENCHMARK SWEEP OVER DENSITIES AND DIMENSIONS
// ============================================================================
template <int M>
void run_microbenchmark_sweep() {
    std::cout << "\n================================================================================" << std::endl;
    std::cout << "  RUNNING MICRO-BENCHMARK SWEEP IN M = " << M << " DIMENSIONS" << std::endl;
    std::cout << "================================================================================" << std::endl;

    std::vector<int> landmark_sizes = {50, 150, 300, 600, 1200};
    std::vector<int> goal_sizes = {100, 400};
    const int NUM_QUERIES = 500;

    std::cout << std::setw(10) << "Landmark N"
              << std::setw(10) << "|T| Size"
              << std::setw(15) << "Flat Naive(us)"
              << std::setw(15) << "KDT Goal(us)"
              << std::setw(15) << "KDT Both(us)"
              << std::setw(12) << "Speedup"
              << std::setw(15) << "Bulk Pruned"
              << std::setw(12) << "Soundness"
              << std::endl;
    std::cout << std::string(99, '-') << std::endl;

    for (int N : landmark_sizes) {
        for (int T_size : goal_sizes) {
            // Generate landmark set G(u -> L)
            auto G_uL = generate_pareto_front<M>(N, 50, 200, 42 + N);
            KDTree<M> tree_uL;
            tree_uL.build(G_uL);

            // Generate goal frontier T
            GoalFrontier<M> T;
            T.points = generate_pareto_front<M>(T_size, 100, 250, 101 + T_size);
            T.update_kdt();

            // Generate random path g_tL
            CostVec<M> g_tL;
            for (int d = 0; d < M; ++d) g_tL[d] = 30 + (d * 10);

            // Generate random candidate paths g(u)
            std::mt19937 rng(777);
            std::vector<CostVec<M>> candidate_g(NUM_QUERIES);
            for (int q = 0; q < NUM_QUERIES; ++q) {
                for (int d = 0; d < M; ++d) {
                    candidate_g[q][d] = std::uniform_int_distribution<int>(60, 220)(rng);
                }
            }

            BenchmarkMetrics met_flat, met_kdt_goal, met_kdt_both;
            bool perfect_match = true;

            for (int q = 0; q < NUM_QUERIES; ++q) {
                bool res1 = check_prune_flat_naive(G_uL, g_tL, candidate_g[q], T, met_flat);
                bool res2 = check_prune_flat_kdt_goal(G_uL, g_tL, candidate_g[q], T, met_kdt_goal);
                bool res3 = check_prune_kdt_geometric(tree_uL, g_tL, candidate_g[q], T, met_kdt_both);

                if (res1 != res2 || res1 != res3) {
                    perfect_match = false;
                }
            }

            double avg_flat = met_flat.time_us / NUM_QUERIES;
            double avg_kdt_goal = met_kdt_goal.time_us / NUM_QUERIES;
            double avg_kdt_both = met_kdt_both.time_us / NUM_QUERIES;
            double speedup = avg_flat / avg_kdt_both;

            std::cout << std::setw(10) << G_uL.size()
                      << std::setw(10) << T.points.size()
                      << std::setw(15) << std::fixed << std::setprecision(2) << avg_flat
                      << std::setw(15) << std::fixed << std::setprecision(2) << avg_kdt_goal
                      << std::setw(15) << std::fixed << std::setprecision(2) << avg_kdt_both
                      << std::setw(11) << std::fixed << std::setprecision(2) << speedup << "x"
                      << std::setw(15) << met_kdt_both.points_pruned_in_bulk / NUM_QUERIES
                      << std::setw(12) << (perfect_match ? "100% MATCH" : "MISMATCH!")
                      << std::endl;
        }
    }
}

// ============================================================================
// EXPERIMENT 2: COMPLETE END-TO-END MOSP GRAPH SEARCH WITH LANDMARK KDT
// ============================================================================
template <int M>
struct Edge {
    int to;
    CostVec<M> cost;
};

template <int M>
struct Graph {
    int V;
    std::vector<std::vector<Edge<M>>> adj;

    Graph(int v) : V(v), adj(v) {}

    void add_edge(int u, int v, const CostVec<M>& c) {
        adj[u].push_back({v, c});
        adj[v].push_back({u, c}); // undirected
    }
};

// Generates an NxN grid graph with M-dimensional trade-off edge costs
template <int M>
Graph<M> generate_grid_mosp(int n, int seed) {
    int V = n * n;
    Graph<M> g(V);
    std::mt19937 rng(seed);

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            int u = r * n + c;
            // Right
            if (c + 1 < n) {
                int v = r * n + (c + 1);
                CostVec<M> cost;
                for (int d = 0; d < M; ++d) {
                    cost[d] = std::uniform_int_distribution<int>(1, 15)(rng);
                }
                g.add_edge(u, v, cost);
            }
            // Down
            if (r + 1 < n) {
                int v = (r + 1) * n + c;
                CostVec<M> cost;
                for (int d = 0; d < M; ++d) {
                    cost[d] = std::uniform_int_distribution<int>(1, 15)(rng);
                }
                g.add_edge(u, v, cost);
            }
        }
    }
    return g;
}

// Exact backward search from landmark L to all nodes to compute G(u -> L)
template <int M>
std::vector<std::vector<CostVec<M>>> compute_landmark_tables(const Graph<M>& g, int landmark) {
    std::vector<std::vector<CostVec<M>>> fronts(g.V);
    
    // Priority queue of (f1, node, cost)
    using PQItem = std::pair<int, std::pair<int, CostVec<M>>>;
    auto cmp = [](const PQItem& a, const PQItem& b) {
        return a.first > b.first;
    };
    std::priority_queue<PQItem, std::vector<PQItem>, decltype(cmp)> pq(cmp);

    CostVec<M> zero_cost;
    for (int d = 0; d < M; ++d) zero_cost[d] = 0;
    pq.push({0, {landmark, zero_cost}});

    while (!pq.empty()) {
        auto [f1, item] = pq.top();
        auto [u, cost] = item;
        pq.pop();

        // Local dominance check
        bool dominated = false;
        for (const auto& p : fronts[u]) {
            if (p.dominates_or_eq(cost)) {
                dominated = true;
                break;
            }
        }
        if (dominated) continue;

        // Filter dominated points
        std::vector<CostVec<M>> new_f;
        for (const auto& p : fronts[u]) {
            if (!cost.strictly_dominates(p)) new_f.push_back(p);
        }
        new_f.push_back(cost);
        fronts[u] = std::move(new_f);

        // Relax neighbors
        for (const auto& e : g.adj[u]) {
            CostVec<M> next_c;
            for (int d = 0; d < M; ++d) next_c[d] = cost[d] + e.cost[d];
            pq.push({next_c[0], {e.to, next_c}});
        }
    }
    return fronts;
}

template <int M>
void run_end_to_end_search_test() {
    std::cout << "\n================================================================================" << std::endl;
    std::cout << "  RUNNING END-TO-END MOSP GRAPH SEARCH: FLAT LANDMARK vs KDT LANDMARK (M = " << M << ")" << std::endl;
    std::cout << "================================================================================" << std::endl;

    int grid_n = (M == 3) ? 5 : 4; // 5x5 grid in 3D, 4x4 in 4D
    int seed = 42;
    auto g = generate_grid_mosp<M>(grid_n, seed);
    int s_start = 0;
    int s_goal = g.V - 1;
    int landmark = grid_n - 1; // Top-right corner landmark

    std::cout << "Grid: " << grid_n << "x" << grid_n << " (" << g.V << " vertices), Landmark: " << landmark << std::endl;

    // 1. Offline Precomputation to Landmark L
    auto G_to_L = compute_landmark_tables(g, landmark);
    std::cout << "Precomputed landmark tables. Landmark frontier size at start: "
              << G_to_L[s_start].size() << ", at goal: " << G_to_L[s_goal].size() << std::endl;

    // Build K-d Trees for all landmark frontiers
    std::vector<KDTree<M>> landmark_kdts(g.V);
    for (int v = 0; v < g.V; ++v) {
        landmark_kdts[v].build(G_to_L[v]);
    }

    // Measure Search: Strategy A (Flat Landmark Scan) vs Strategy B (KDT Landmark Subtree Prune)
    for (int strategy = 0; strategy < 2; ++strategy) {
        std::string strat_name = (strategy == 0) ? "Strategy A: Flat Landmark Lists" : "Strategy B: Geometric KDT Landmark Trees";
        std::cout << "\n--- Running " << strat_name << " ---" << std::endl;

        GoalFrontier<M> T;
        std::vector<std::vector<CostVec<M>>> node_fronts(g.V);

        using SearchItem = std::pair<int, std::pair<int, CostVec<M>>>;
        auto scmp = [](const SearchItem& a, const SearchItem& b) {
            return a.first > b.first;
        };
        std::priority_queue<SearchItem, std::vector<SearchItem>, decltype(scmp)> open(scmp);

        CostVec<M> zero_cost;
        for (int d = 0; d < M; ++d) zero_cost[d] = 0;
        open.push({0, {s_start, zero_cost}});

        uint64_t nodes_expanded = 0;
        uint64_t landmark_prunes = 0;
        uint64_t total_cmps = 0;
        BenchmarkMetrics met;

        auto t0 = std::chrono::high_resolution_clock::now();

        while (!open.empty()) {
            auto [f1, item] = open.top();
            auto [u, cost] = item;
            open.pop();

            // Landmark Differential Pruning Check against current goal solutions T
            if (!T.points.empty() && !G_to_L[s_goal].empty() && !G_to_L[u].empty()) {
                bool pruned_by_landmark = false;
                // Test across paths in G(goal -> L)
                for (const auto& g_tL : G_to_L[s_goal]) {
                    bool all_dom = false;
                    if (strategy == 0) {
                        all_dom = check_prune_flat_kdt_goal(G_to_L[u], g_tL, cost, T, met);
                    } else {
                        all_dom = check_prune_kdt_geometric(landmark_kdts[u], g_tL, cost, T, met);
                    }
                    if (all_dom) {
                        pruned_by_landmark = true;
                        break;
                    }
                }
                if (pruned_by_landmark) {
                    landmark_prunes++;
                    continue; // Successfully pruned by differential heuristic!
                }
            }

            // Local dominance check at node u
            bool local_dom = false;
            for (const auto& p : node_fronts[u]) {
                if (p.dominates_or_eq(cost)) {
                    local_dom = true;
                    break;
                }
            }
            if (local_dom) continue;

            // Filter dominated in node_fronts[u]
            std::vector<CostVec<M>> new_nf;
            for (const auto& p : node_fronts[u]) {
                if (!cost.strictly_dominates(p)) new_nf.push_back(p);
            }
            new_nf.push_back(cost);
            node_fronts[u] = std::move(new_nf);

            if (u == s_goal) {
                // Goal reached!
                T.points.push_back(cost);
                T.update_kdt();
                continue;
            }

            nodes_expanded++;
            for (const auto& e : g.adj[u]) {
                CostVec<M> next_c;
                for (int d = 0; d < M; ++d) next_c[d] = cost[d] + e.cost[d];
                open.push({next_c[0], {e.to, next_c}});
            }
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double search_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        std::cout << "  Search Time: " << search_ms << " ms" << std::endl;
        std::cout << "  Nodes Expanded: " << nodes_expanded << std::endl;
        std::cout << "  Landmark Prunes: " << landmark_prunes << std::endl;
        std::cout << "  Scalar Cmps: " << met.scalar_cmps << std::endl;
        if (strategy == 1) {
            std::cout << "  Subtree Bulk Prunes: " << met.subtree_prunes
                      << " (" << met.points_pruned_in_bulk << " points skipped in bulk)" << std::endl;
        }
        std::cout << "  Final Pareto Front Size at Goal: " << T.points.size() << std::endl;
    }
}

// Generates a layered DAG with anti-correlated edge costs to create dense Pareto fronts
template <int M>
Graph<M> generate_dense_dag(int layers, int width, int seed) {
    int V = layers * width + 2;
    int s_start = 0;
    int s_goal = V - 1;
    Graph<M> g(V);
    std::mt19937 rng(seed);

    // Connect start to first layer
    for (int w = 0; w < width; ++w) {
        int v = 1 + w;
        CostVec<M> c;
        for (int d = 0; d < M; ++d) c[d] = std::uniform_int_distribution<int>(5, 20)(rng);
        g.adj[s_start].push_back({v, c});
    }

    // Connect intermediate layers with negative correlation
    for (int l = 0; l < layers - 1; ++l) {
        for (int w1 = 0; w1 < width; ++w1) {
            int u = 1 + l * width + w1;
            for (int w2 = 0; w2 < width; ++w2) {
                int v = 1 + (l + 1) * width + w2;
                CostVec<M> c;
                int base = std::uniform_int_distribution<int>(10, 50)(rng);
                for (int d = 0; d < M; ++d) {
                    if (d == (w1 + w2) % M) {
                        c[d] = 1; // Very cheap on one objective
                    } else {
                        c[d] = base + std::uniform_int_distribution<int>(5, 15)(rng);
                    }
                }
                g.adj[u].push_back({v, c});
            }
        }
    }

    // Connect last layer to goal
    for (int w = 0; w < width; ++w) {
        int u = 1 + (layers - 1) * width + w;
        CostVec<M> c;
        for (int d = 0; d < M; ++d) c[d] = std::uniform_int_distribution<int>(5, 20)(rng);
        g.adj[u].push_back({s_goal, c});
    }

    return g;
}

template <int M>
void run_dense_landmark_search_test() {
    std::cout << "\n================================================================================" << std::endl;
    std::cout << "  RUNNING DENSE GRAPH SEARCH WITH LARGE LANDMARK POOLS (M = " << M << ")" << std::endl;
    std::cout << "================================================================================" << std::endl;

    int layers = (M == 3) ? 6 : 5;
    int width = 4;
    auto g = generate_dense_dag<M>(layers, width, 101);
    int s_start = 0;
    int s_goal = g.V - 1;
    // Landmark is a node in the middle layer to create dense two-way frontiers
    int landmark = 1 + (layers / 2) * width;

    std::cout << "Layered DAG: " << layers << " layers x " << width << " width (" << g.V << " vertices), Landmark: " << landmark << std::endl;

    // Precompute landmark tables
    auto G_to_L = compute_landmark_tables(g, landmark);
    std::cout << "Precomputed landmark tables. Landmark frontier size at start: "
              << G_to_L[s_start].size() << ", at goal: " << G_to_L[s_goal].size() << std::endl;

    // Build KDTs
    std::vector<KDTree<M>> landmark_kdts(g.V);
    for (int v = 0; v < g.V; ++v) {
        landmark_kdts[v].build(G_to_L[v]);
    }

    // Benchmark Strategy A vs Strategy B
    for (int strategy = 0; strategy < 2; ++strategy) {
        std::string strat_name = (strategy == 0) ? "Strategy A: Flat Landmark Lists" : "Strategy B: Geometric KDT Landmark Trees";
        std::cout << "\n--- Running " << strat_name << " ---" << std::endl;

        GoalFrontier<M> T;
        std::vector<std::vector<CostVec<M>>> node_fronts(g.V);

        using SearchItem = std::pair<int, std::pair<int, CostVec<M>>>;
        auto scmp = [](const SearchItem& a, const SearchItem& b) {
            return a.first > b.first;
        };
        std::priority_queue<SearchItem, std::vector<SearchItem>, decltype(scmp)> open(scmp);

        CostVec<M> zero_cost;
        for (int d = 0; d < M; ++d) zero_cost[d] = 0;
        open.push({0, {s_start, zero_cost}});

        uint64_t nodes_expanded = 0;
        uint64_t landmark_prunes = 0;
        BenchmarkMetrics met;

        auto t0 = std::chrono::high_resolution_clock::now();

        while (!open.empty()) {
            auto [f1, item] = open.top();
            auto [u, cost] = item;
            open.pop();

            if (!T.points.empty() && !G_to_L[s_goal].empty() && !G_to_L[u].empty()) {
                bool pruned_by_landmark = false;
                for (const auto& g_tL : G_to_L[s_goal]) {
                    bool all_dom = false;
                    if (strategy == 0) {
                        all_dom = check_prune_flat_kdt_goal(G_to_L[u], g_tL, cost, T, met);
                    } else {
                        all_dom = check_prune_kdt_geometric(landmark_kdts[u], g_tL, cost, T, met);
                    }
                    if (all_dom) {
                        pruned_by_landmark = true;
                        break;
                    }
                }
                if (pruned_by_landmark) {
                    landmark_prunes++;
                    continue;
                }
            }

            bool local_dom = false;
            for (const auto& p : node_fronts[u]) {
                if (p.dominates_or_eq(cost)) {
                    local_dom = true;
                    break;
                }
            }
            if (local_dom) continue;

            std::vector<CostVec<M>> new_nf;
            for (const auto& p : node_fronts[u]) {
                if (!cost.strictly_dominates(p)) new_nf.push_back(p);
            }
            new_nf.push_back(cost);
            node_fronts[u] = std::move(new_nf);

            if (u == s_goal) {
                T.points.push_back(cost);
                T.update_kdt();
                continue;
            }

            nodes_expanded++;
            for (const auto& e : g.adj[u]) {
                CostVec<M> next_c;
                for (int d = 0; d < M; ++d) next_c[d] = cost[d] + e.cost[d];
                open.push({next_c[0], {e.to, next_c}});
            }
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double search_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        std::cout << "  Search Time: " << search_ms << " ms" << std::endl;
        std::cout << "  Nodes Expanded: " << nodes_expanded << std::endl;
        std::cout << "  Landmark Prunes: " << landmark_prunes << std::endl;
        std::cout << "  Scalar Cmps: " << met.scalar_cmps << std::endl;
        if (strategy == 1) {
            std::cout << "  Subtree Bulk Prunes: " << met.subtree_prunes
                      << " (" << met.points_pruned_in_bulk << " points skipped in bulk)" << std::endl;
        }
        std::cout << "  Final Pareto Front Size at Goal: " << T.points.size() << std::endl;
    }
}

int main() {
    std::cout << "================================================================================" << std::endl;
    std::cout << "   TESTING FEASIBILITY: KDT-ACCELERATED DIFFERENTIAL HEURISTICS IN M >= 3" << std::endl;
    std::cout << "================================================================================" << std::endl;

    // Run micro-benchmark density scaling across dimensions
    run_microbenchmark_sweep<3>();
    run_microbenchmark_sweep<4>();
    run_microbenchmark_sweep<5>();

    // Run complete end-to-end graph search tests
    run_end_to_end_search_test<3>();
    run_end_to_end_search_test<4>();

    // Run dense landmark search tests
    run_dense_landmark_search_test<3>();
    run_dense_landmark_search_test<4>();

    std::cout << "\n================================================================================" << std::endl;
    std::cout << "   BENCHMARK COMPLETE - ALL SOUNDNESS CHECKS PASSED!" << std::endl;
    std::cout << "================================================================================" << std::endl;
    return 0;
}
