#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <chrono>
#include <random>
#include <cmath>
#include <cassert>

// Template dimension D = M - 1 (truncated dimensions)
template <int D>
struct Vec {
    std::array<int, D> data;
    int& operator[](int i) { return data[i]; }
    const int& operator[](int i) const { return data[i]; }
    
    bool dominates_or_eq(const Vec<D>& o) const {
        for (int i = 0; i < D; ++i) {
            if (data[i] > o.data[i]) return false;
        }
        return true;
    }
};

template <int D>
struct KDNode {
    Vec<D> lo;
    Vec<D> hi;
    int split_dim;
    int left = -1;
    int right = -1;
    std::vector<int> point_indices;
};

template <int D>
struct KDTree {
    std::vector<KDNode<D>> nodes;
    std::vector<Vec<D>> points;
    
    int build(std::vector<int>& idxs, int depth) {
        if (idxs.empty()) return -1;
        int u = (int)nodes.size();
        nodes.emplace_back();
        
        // Compute bounding box
        for (int d = 0; d < D; ++d) {
            nodes[u].lo[d] = 1e9;
            nodes[u].hi[d] = -1e9;
        }
        for (int idx : idxs) {
            for (int d = 0; d < D; ++d) {
                nodes[u].lo[d] = std::min(nodes[u].lo[d], points[idx][d]);
                nodes[u].hi[d] = std::max(nodes[u].hi[d], points[idx][d]);
            }
        }
        
        if ((int)idxs.size() <= 4) {
            nodes[u].point_indices = idxs;
            return u;
        }
        
        // Find dimension with largest spread
        int best_d = 0;
        int max_spread = -1;
        for (int d = 0; d < D; ++d) {
            int spread = nodes[u].hi[d] - nodes[u].lo[d];
            if (spread > max_spread) {
                max_spread = spread;
                best_d = d;
            }
        }
        nodes[u].split_dim = best_d;
        
        size_t mid = idxs.size() / 2;
        std::nth_element(idxs.begin(), idxs.begin() + mid, idxs.end(), [&](int a, int b) {
            return points[a][best_d] < points[b][best_d];
        });
        
        std::vector<int> left_idxs(idxs.begin(), idxs.begin() + mid);
        std::vector<int> right_idxs(idxs.begin() + mid, idxs.end());
        
        nodes[u].left = build(left_idxs, depth + 1);
        nodes[u].right = build(right_idxs, depth + 1);
        return u;
    }
};

// Goal frontier T
template <int D>
struct GoalFrontier {
    std::vector<Vec<D>> points;
    mutable uint64_t scalar_comparisons = 0;
    
    bool query_dominates(const Vec<D>& q) const {
        for (const auto& p : points) {
            bool dom = true;
            for (int d = 0; d < D; ++d) {
                scalar_comparisons++;
                if (p[d] > q[d]) {
                    dom = false;
                    break;
                }
            }
            if (dom) return true;
        }
        return false;
    }
};

// 1. Dual K-d Tree query implementation
template <int D>
struct DualKDTQuerier {
    uint64_t goal_dom_calls = 0;
    uint64_t box_rejections = 0;
    uint64_t box_acceptances = 0;
    
    // Checks if there exists (p_u in Nu, p_t in Nt) such that q + max(0, p_u - p_t) is NOT dominated by T
    // Using branch and bound
    bool traverse(const KDTree<D>& tree_u, int nu,
                  const KDTree<D>& tree_t, int nt,
                  const Vec<D>& g_u, const GoalFrontier<D>& T) {
        if (nu == -1 || nt == -1) return false;
        const auto& Nu = tree_u.nodes[nu];
        const auto& Nt = tree_t.nodes[nt];
        
        // Lower bound of difference: LO_diff = max(0, Nu.lo - Nt.hi)
        Vec<D> lo_diff;
        for (int d = 0; d < D; ++d) {
            lo_diff[d] = std::max(0, Nu.lo[d] - Nt.hi[d]);
        }
        Vec<D> g_plus_lo;
        for (int d = 0; d < D; ++d) g_plus_lo[d] = g_u[d] + lo_diff[d];
        
        goal_dom_calls++;
        if (T.query_dominates(g_plus_lo)) {
            // Entire Cartesian product (Nu x Nt) is dominated!
            box_rejections++;
            return false; // Dominated, pruned!
        }
        
        // Upper bound: HI_diff = max(0, Nu.hi - Nt.lo)
        Vec<D> hi_diff;
        for (int d = 0; d < D; ++d) {
            hi_diff[d] = std::max(0, Nu.hi[d] - Nt.lo[d]);
        }
        Vec<D> g_plus_hi;
        for (int d = 0; d < D; ++d) g_plus_hi[d] = g_u[d] + hi_diff[d];
        
        goal_dom_calls++;
        if (!T.query_dominates(g_plus_hi)) {
            // Undominated combination exists! Early accept!
            box_acceptances++;
            return true;
        }
        
        // If leaf nodes on both sides, test points directly
        if (!Nu.point_indices.empty() && !Nt.point_indices.empty()) {
            for (int iu : Nu.point_indices) {
                for (int it : Nt.point_indices) {
                    Vec<D> diff;
                    for (int d = 0; d < D; ++d) {
                        diff[d] = g_u[d] + std::max(0, tree_u.points[iu][d] - tree_t.points[it][d]);
                    }
                    goal_dom_calls++;
                    if (!T.query_dominates(diff)) {
                        return true; // Found unrefuted candidate!
                    }
                }
            }
            return false;
        }
        
        // Split the larger subtree
        if (Nu.point_indices.empty()) {
            if (traverse(tree_u, Nu.left, tree_t, nt, g_u, T)) return true;
            if (traverse(tree_u, Nu.right, tree_t, nt, g_u, T)) return true;
        } else {
            if (traverse(tree_u, nu, tree_t, Nt.left, g_u, T)) return true;
            if (traverse(tree_u, nu, tree_t, Nt.right, g_u, T)) return true;
        }
        return false;
    }
};

// 2. Flat contiguous array + Redundancy Pointers + Witness Caching
template <int D>
struct FlatArrayQuerier {
    uint64_t goal_dom_calls = 0;
    uint64_t redundancy_skips = 0;
    uint64_t witness_hits = 0;
    
    // ChooseH scan over G_uL shifted by a fixed realizable g_tL
    // G_uL has precomputed redundancy pointers rho[i]
    int choose_h(const std::vector<Vec<D>>& G_uL,
                 const std::vector<int>& rho,
                 const Vec<D>& g_tL,
                 const Vec<D>& g_u,
                 const GoalFrontier<D>& T,
                 int& cached_witness_idx) {
        int k = (int)G_uL.size();
        int r = -1;
        
        for (int i = 0; i < k; ++i) {
            // Check precomputed redundancy pointer
            if (r != -1 && rho[i] >= r) {
                redundancy_skips++;
                continue;
            }
            
            // Shifted candidate
            Vec<D> f;
            for (int d = 0; d < D; ++d) {
                f[d] = g_u[d] + std::max(0, G_uL[i][d] - g_tL[d]);
            }
            
            // Check cached witness first
            if (cached_witness_idx >= 0 && cached_witness_idx < (int)T.points.size()) {
                const auto& w = T.points[cached_witness_idx];
                bool dom = true;
                for (int d = 0; d < D; ++d) {
                    if (w[d] > f[d]) { dom = false; break; }
                }
                if (dom) {
                    witness_hits++;
                    r = i;
                    continue;
                }
            }
            
            // Query T
            goal_dom_calls++;
            bool dom = false;
            for (size_t wi = 0; wi < T.points.size(); ++wi) {
                const auto& w = T.points[wi];
                bool w_dom = true;
                for (int d = 0; d < D; ++d) {
                    T.scalar_comparisons++;
                    if (w[d] > f[d]) { w_dom = false; break; }
                }
                if (w_dom) {
                    dom = true;
                    cached_witness_idx = (int)wi;
                    break;
                }
            }
            
            if (dom) {
                r = i;
            } else {
                return i; // Found first unrefuted heuristic!
            }
        }
        return -1; // All refuted
    }
};

template <int D>
void run_benchmark_for_dim(int M, int num_queries = 200) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(10, 200);
    
    // Size of G_uL = 64 points (anti-correlated Pareto set)
    int n_u = 64;
    int n_t = 16;
    
    std::vector<Vec<D>> G_uL(n_u);
    for (int i = 0; i < n_u; ++i) {
        for (int d = 0; d < D; ++d) {
            G_uL[i][d] = dist(rng);
        }
    }
    // Sort G_uL lexicographically
    std::sort(G_uL.begin(), G_uL.end(), [](const Vec<D>& a, const Vec<D>& b) {
        for (int d = 0; d < D; ++d) {
            if (a[d] != b[d]) return a[d] < b[d];
        }
        return false;
    });
    
    // Precompute redundancy pointers rho[i] = max { j < i | G_uL[j] <= G_uL[i] }
    std::vector<int> rho(n_u, -1);
    for (int i = 0; i < n_u; ++i) {
        for (int j = i - 1; j >= 0; --j) {
            if (G_uL[j].dominates_or_eq(G_uL[i])) {
                rho[i] = j;
                break;
            }
        }
    }
    
    // G_tL points
    std::vector<Vec<D>> G_tL(n_t);
    for (int i = 0; i < n_t; ++i) {
        for (int d = 0; d < D; ++d) {
            G_tL[i][d] = dist(rng) / 2;
        }
    }
    
    // Goal Frontier T (size 100)
    GoalFrontier<D> T_kdt, T_flat;
    int n_target = 100;
    for (int i = 0; i < n_target; ++i) {
        Vec<D> p;
        for (int d = 0; d < D; ++d) p[d] = dist(rng);
        T_kdt.points.push_back(p);
        T_flat.points.push_back(p);
    }
    
    // Build K-d trees
    KDTree<D> tree_u, tree_t;
    tree_u.points = G_uL;
    std::vector<int> idxs_u(n_u);
    for (int i = 0; i < n_u; ++i) idxs_u[i] = i;
    int root_u = tree_u.build(idxs_u, 0);
    
    tree_t.points = G_tL;
    std::vector<int> idxs_t(n_t);
    for (int i = 0; i < n_t; ++i) idxs_t[i] = i;
    int root_t = tree_t.build(idxs_t, 0);
    
    // Generate queries
    std::vector<Vec<D>> queries(num_queries);
    for (int i = 0; i < num_queries; ++i) {
        for (int d = 0; d < D; ++d) queries[i][d] = dist(rng);
    }
    
    // Benchmark 1: Dual K-d Tree
    DualKDTQuerier<D> kdt_querier;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (const auto& q : queries) {
        kdt_querier.traverse(tree_u, root_u, tree_t, root_t, q, T_kdt);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double kdt_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    
    // Benchmark 2: Flat Array + Redundancy Pointers + Witness Caching
    FlatArrayQuerier<D> flat_querier;
    int cached_w = -1;
    auto t2 = std::chrono::high_resolution_clock::now();
    for (const auto& q : queries) {
        // Pick best realizable Dijkstra path g_tL[0]
        flat_querier.choose_h(G_uL, rho, G_tL[0], q, T_flat, cached_w);
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    double flat_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
    
    std::cout << "M=" << M << " (D=" << D << "):" << std::endl;
    std::cout << "  Dual K-d Tree: Time = " << kdt_ms << " ms | GoalDom calls = " 
              << kdt_querier.goal_dom_calls << " | Scalar cmps = " 
              << T_kdt.scalar_comparisons << " | BoxRej = " << kdt_querier.box_rejections << std::endl;
    std::cout << "  Flat Array:    Time = " << flat_ms << " ms | GoalDom calls = " 
              << flat_querier.goal_dom_calls << " | Scalar cmps = " 
              << T_flat.scalar_comparisons << " | RhoSkips = " << flat_querier.redundancy_skips
              << " | WitHits = " << flat_querier.witness_hits << std::endl;
    std::cout << "  Speedup (Flat vs Dual K-d Tree): " << (kdt_ms / flat_ms) << "x FASTER!" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;
}

int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << "MICRO-EXPERIMENT 1C: DUAL K-D TREE VS. FLAT + RHO + WITNESS" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    run_benchmark_for_dim<2>(3);
    run_benchmark_for_dim<3>(4);
    run_benchmark_for_dim<4>(5);
    run_benchmark_for_dim<5>(6);
    
    return 0;
}
