// Prototype L-NAMOA*dr-mvh integration layer (scratchpad only).
// Replicates Maya's search loop exactly (same Node type, same OPEN comparator, same push order)
// while making every data structure pluggable and instrumented.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <memory_resource>
#include <queue>
#include <vector>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "data_structures/adjacency_matrix.h"
#include "data_structures/node.h"
#include "definitions.h"
using SolutionSet = std::vector<NodePtr>;

namespace proto {

constexpr size_t INF = std::numeric_limits<size_t>::max();
inline uint64_t tick() { return __rdtsc(); }

struct Cfg {
    size_t localX = INF;       // build frontier KD tree at a (non-target) state once live size >= localX
    size_t targetX = INF;      // same, for the target state's frontier T
    int chooseh = 0;           // 0 linear, 1 idx-tree top-down, 2 idx-tree bottom-up, 3 Roi spatial top-down
    size_t hX = 0;             // build heuristic tree for s only if |H(s)| >= hX
    size_t leafB = 8;          // heuristic tree leaf size
    bool fastpath = false;     // test h[start] before touching the tree
    bool suffix_ideal = false; // test Tr(g)+min_{j>=start} Tr(h_j) first (O(1) "none left" proof)
    bool local_first = false;  // at generation, run LOCALDOMCHECK before CHOOSEH (both are pure predicates)
    bool filterT = false;      // dual-tree: pass filtered candidate subsets of T down the heuristic tree
    bool gcl_prune = false;    // keep only the full-D Pareto set of G_cl(s)
    bool exact_max = true;     // exact max g1 over live truncated set (vs stale-until-rebuild)
    bool branchless = false;   // branch-free dominance kernel for flat scans
    bool flatH = false;        // linear CHOOSEH over a contiguous copy of Tr(H(s)) instead of vector<vector>
    bool witness = false;      // LOCALDOMCHECK: accept a full-D witness found in F(s) before any fallback
    bool redund = false;       // skip h_j when an earlier, already-refuted h_i has Tr(h_i) <= Tr(h_j)
    bool lastwit = false;      // flat frontiers: re-test last dominator first
    size_t rb_div = 4, rb_slack = 64;  // KD rebuild when total > built + built/rb_div + rb_slack
    uint32_t bk_cap = 0;               // 0 = Shahaf point-per-node KD; >0 = bucket KD leaf capacity
    uint64_t promoteC = 0;             // >0: promote flat->tree when mean scan length >= promoteC (and size >= X)
};

struct Stats {
    uint64_t t_global = 0, t_ch_gen = 0, t_ch_re = 0, t_local = 0, t_full = 0, t_update = 0, t_total = 0, t_build = 0;
    uint64_t cmpchk = 0, cmpupd = 0, cmpfull = 0, cmp_ch = 0;
    uint64_t ch_calls_gen = 0, ch_calls_re = 0, ch_first = 0, ch_later = 0, ch_none = 0, ch_ideal_prune = 0;
    uint64_t ch_skip_sum = 0, ch_H_sum = 0, ch_T_sum = 0, ch_tree_calls = 0, ch_nodes = 0;
    uint64_t loc_calls = 0, loc_notr = 0, loc_tdisc = 0, loc_bad = 0, loc_good = 0;
    uint64_t loc_size_hist[24] = {0};  // log2 buckets of live frontier size at local query
    uint64_t T_size_hist[24] = {0};
    uint64_t loc_cyc[24] = {0}, loc_cnt[24] = {0}, T_cyc[24] = {0}, T_cnt[24] = {0}, upd_cyc[24] = {0}, upd_cnt[24] = {0};
    bool prof = false;
    uint64_t kd_builds = 0, kd_rebuilds = 0, htrees = 0, h_redundant = 0, h_total = 0, ch_rskip = 0, domT_calls = 0;
    size_t expansions = 0, generations = 0, reinsertions = 0;
};

inline int lg2b(size_t n) { int b = 0; while (n > 1 && b < 23) { n >>= 1; ++b; } return n == 0 ? 0 : b + (b < 23); }

// ---------------------------------------------------------------------------------------------
// Bucket K-d tree: internal nodes hold only (box, split); points live in contiguous fixed-capacity
// leaf buckets. Deletions compact the bucket in place (no tombstones).
// ---------------------------------------------------------------------------------------------
template <int D>
class BucketKD {
    static constexpr int P = D - 1;
public:
    struct BNode { size_t lo[P]; size_t hi[P]; size_t split; uint32_t l, r; uint32_t axis, off, cnt, pad; };
    std::vector<BNode> nodes;
    std::vector<size_t> pool;  // leaf slots: CAP*D each
    uint32_t CAP = 16;
    size_t live = 0, since_build = 0, built = 0;
    std::vector<uint32_t> stk;

    bool empty() const { return nodes.empty(); }
    static bool leaf(const BNode& n) { return n.l == UINT32_MAX; }

    uint32_t new_leaf() {
        BNode n; for (int k = 0; k < P; ++k) { n.lo[k] = INF; n.hi[k] = 0; }
        n.split = 0; n.l = n.r = UINT32_MAX; n.axis = 0; n.off = (uint32_t)(pool.size() / D); n.cnt = 0; n.pad = 0;
        pool.resize(pool.size() + (size_t)CAP * D);
        nodes.push_back(n); return (uint32_t)nodes.size() - 1;
    }
    void build(const std::vector<const size_t*>& pts) {
        nodes.clear(); pool.clear();
        std::vector<const size_t*> v(pts);
        if (v.empty()) { new_leaf(); live = 0; built = 0; since_build = 0; return; }
        build_rec(v, 0, v.size());
        live = built = v.size(); since_build = 0;
    }
    uint32_t build_rec(std::vector<const size_t*>& v, size_t lo, size_t hi) {
        if (hi - lo <= CAP / 2 || hi - lo <= 1) {
            uint32_t id = new_leaf();
            for (size_t i = lo; i < hi; ++i) put(id, v[i]);
            return id;
        }
        size_t bl[P], bh[P]; for (int k = 0; k < P; ++k) { bl[k] = INF; bh[k] = 0; }
        for (size_t i = lo; i < hi; ++i) for (int k = 0; k < P; ++k) { bl[k] = std::min(bl[k], v[i][k + 1]); bh[k] = std::max(bh[k], v[i][k + 1]); }
        int ax = 0; size_t best = 0; for (int k = 0; k < P; ++k) if (bh[k] - bl[k] > best) { best = bh[k] - bl[k]; ax = k; }
        if (best == 0) { std::fprintf(stderr, "BucketKD: duplicate Tr points\n"); std::abort(); }
        size_t sv; size_t m = median_partition(v, lo, hi, ax, sv);
        uint32_t id = (uint32_t)nodes.size(); nodes.emplace_back();
        uint32_t l = build_rec(v, lo, m), r = build_rec(v, m, hi);
        BNode& n = nodes[id];
        for (int k = 0; k < P; ++k) { n.lo[k] = bl[k]; n.hi[k] = bh[k]; }
        n.split = sv; n.axis = ax; n.l = l; n.r = r; n.off = 0; n.cnt = 0;
        return id;
    }
    void put(uint32_t id, const size_t* p) {
        BNode& n = nodes[id];
        std::memcpy(&pool[((size_t)n.off + n.cnt) * D], p, sizeof(size_t) * D); ++n.cnt;
        for (int k = 0; k < P; ++k) { n.lo[k] = std::min(n.lo[k], p[k + 1]); n.hi[k] = std::max(n.hi[k], p[k + 1]); }
    }
    void split_leaf(uint32_t id) {
        std::vector<size_t> tmp(pool.begin() + (size_t)nodes[id].off * D, pool.begin() + ((size_t)nodes[id].off + nodes[id].cnt) * D);
        std::vector<const size_t*> v; for (size_t i = 0; i < tmp.size(); i += D) v.push_back(&tmp[i]);
        size_t bl[P], bh[P]; for (int k = 0; k < P; ++k) { bl[k] = INF; bh[k] = 0; }
        for (auto a : v) for (int k = 0; k < P; ++k) { bl[k] = std::min(bl[k], a[k + 1]); bh[k] = std::max(bh[k], a[k + 1]); }
        int ax = 0; size_t best = 0;
        for (int k = 0; k < P; ++k) if (bh[k] - bl[k] > best) { best = bh[k] - bl[k]; ax = k; }
        size_t sv; size_t m = median_partition(v, 0, v.size(), ax, sv);
        uint32_t off = nodes[id].off;
        uint32_t b = new_leaf();
        uint32_t a = (uint32_t)nodes.size(); nodes.push_back(nodes[b]);
        nodes[a].off = off; nodes[a].cnt = 0;
        for (int k = 0; k < P; ++k) { nodes[a].lo[k] = INF; nodes[a].hi[k] = 0; }
        for (size_t i = 0; i < m; ++i) put(a, v[i]);
        for (size_t i = m; i < v.size(); ++i) put(b, v[i]);
        BNode& p = nodes[id]; p.l = a; p.r = b; p.axis = ax; p.split = sv; p.cnt = 0;
    }
    // partition v[lo,hi) on axis so that left = {a : a[ax+1] < sv}; both sides non-empty when spread > 0
    size_t median_partition(std::vector<const size_t*>& v, size_t lo, size_t hi, int ax, size_t& sv) {
        size_t mid = lo + (hi - lo) / 2;
        std::nth_element(v.begin() + lo, v.begin() + mid, v.begin() + hi, [ax](const size_t* a, const size_t* b) { return a[ax + 1] < b[ax + 1]; });
        sv = v[mid][ax + 1];
        size_t m = std::partition(v.begin() + lo, v.begin() + hi, [ax, sv](const size_t* a) { return a[ax + 1] < sv; }) - v.begin();
        if (m == lo) {
            ++sv;
            m = std::partition(v.begin() + lo, v.begin() + hi, [ax, sv](const size_t* a) { return a[ax + 1] < sv; }) - v.begin();
        }
        return m;
    }
    void insert(const size_t* p) {
        ++live; ++since_build;
        uint32_t cur = 0;
        while (true) {
            BNode& n = nodes[cur];
            for (int k = 0; k < P; ++k) { if (p[k + 1] < n.lo[k]) n.lo[k] = p[k + 1]; if (p[k + 1] > n.hi[k]) n.hi[k] = p[k + 1]; }
            if (leaf(n)) break;
            cur = (p[n.axis + 1] < n.split) ? n.l : n.r;
        }
        if (nodes[cur].cnt == CAP) { split_leaf(cur); BNode& n = nodes[cur]; cur = (p[n.axis + 1] < n.split) ? n.l : n.r; }
        put(cur, p);
    }
    // weakly-Tr-dominated-by-g points are removed; returns #removed with p0 == m0
    size_t remove_dominated(const size_t* g, uint64_t& cmp, size_t m0) {
        size_t killed_max = 0;
        stk.clear(); stk.push_back(0);
        while (!stk.empty()) {
            uint32_t id = stk.back(); stk.pop_back();
            BNode& n = nodes[id];
            bool prune = false;
            for (int k = 0; k < P; ++k) if (n.hi[k] < g[k + 1]) { prune = true; break; }
            if (prune) continue;
            if (!leaf(n)) { stk.push_back(n.r); stk.push_back(n.l); continue; }
            size_t* base = &pool[(size_t)n.off * D];
            uint32_t w = 0;
            for (uint32_t i = 0; i < n.cnt; ++i) {
                size_t* a = base + (size_t)i * D; ++cmp;
                int k = 1; for (; k < D; ++k) if (g[k] > a[k]) break;
                if (k == D) { --live; if (a[0] == m0) ++killed_max; continue; }
                if (w != i) std::memcpy(base + (size_t)w * D, a, sizeof(size_t) * D);
                ++w;
            }
            n.cnt = w;
        }
        return killed_max;
    }
    template <int MODE>  // 0: exists Tr-dominator; 1: witness (2 full, 1 tr-only, 0 none)
    int query(const size_t* q, uint64_t& cmp) const {
        auto& s = const_cast<std::vector<uint32_t>&>(stk);
        s.clear(); s.push_back(0);
        int res = 0;
        while (!s.empty()) {
            uint32_t id = s.back(); s.pop_back();
            const BNode& n = nodes[id];
            bool prune = false;
            for (int k = 0; k < P; ++k) if (n.lo[k] > q[k + 1]) { prune = true; break; }
            if (prune) continue;
            if (!leaf(n)) { s.push_back(n.r); s.push_back(n.l); continue; }
            const size_t* a = &pool[(size_t)n.off * D];
            for (uint32_t i = 0; i < n.cnt; ++i, a += D) {
                ++cmp; int k = 1; for (; k < D; ++k) if (a[k] > q[k]) break;
                if (k == D) { if (MODE == 0) return 1; if (a[0] <= q[0]) return 2; res = 1; }
            }
        }
        return res;
    }
    void collect_le(const size_t* ub, std::vector<const size_t*>& out, uint64_t& cmp) const {
        auto& s = const_cast<std::vector<uint32_t>&>(stk);
        s.clear(); s.push_back(0);
        while (!s.empty()) {
            uint32_t id = s.back(); s.pop_back();
            const BNode& n = nodes[id];
            bool prune = false;
            for (int k = 0; k < P; ++k) if (n.lo[k] > ub[k + 1]) { prune = true; break; }
            if (prune) continue;
            if (!leaf(n)) { s.push_back(n.r); s.push_back(n.l); continue; }
            const size_t* a = &pool[(size_t)n.off * D];
            for (uint32_t i = 0; i < n.cnt; ++i, a += D) {
                ++cmp; int k = 1; for (; k < D; ++k) if (a[k] > ub[k]) break;
                if (k == D) out.push_back(a);
            }
        }
    }
    void max0(size_t& m, size_t& c) const {
        m = 0; c = 0;
        for (const auto& n : nodes) if (leaf(n)) for (uint32_t i = 0; i < n.cnt; ++i) {
            size_t v = pool[((size_t)n.off + i) * D];
            if (v > m) { m = v; c = 1; } else if (v == m) ++c;
        }
    }
    void rebuild() {
        std::vector<size_t> buf; buf.reserve(live * D);
        for (const auto& n : nodes) if (leaf(n)) buf.insert(buf.end(), pool.begin() + (size_t)n.off * D, pool.begin() + ((size_t)n.off + n.cnt) * D);
        std::vector<const size_t*> v; for (size_t i = 0; i < buf.size(); i += D) v.push_back(&buf[i]);
        build(v);
    }
};

// ---------------------------------------------------------------------------------------------
// Dynamic truncated frontier F(s): flat array until promote threshold, then dynamic K-d tree.
// Stores full D-dim points (coordinate 0 needed for the DR monotonicity test).
// ---------------------------------------------------------------------------------------------
template <int D>
class Front {
    static constexpr int P = D - 1;
public:
    struct KNode {
#ifdef PAD_NODES
        size_t pt[10];
        size_t lo[10];
        size_t hi[10];
#else
        size_t pt[D];
        size_t lo[P];
        size_t hi[P];
#endif
        uint32_t l, r;
        uint32_t axis;
        uint32_t dead;
    };
    BucketKD<D> bk;
    bool is_bk = false;
    uint32_t bk_cap = 0;  // 0 = point-per-node KD (Shahaf), else bucket KD with this leaf capacity

    // flat mode
    std::vector<size_t> flat;  // n*D
    // kd mode
    std::vector<KNode> nodes;
    uint32_t root = UINT32_MAX;
    size_t total = 0, built = 0;
    bool is_tree = false;

    size_t live = 0;
    size_t maxg0 = 0;
    size_t maxg0_cnt = 0;  // number of live points with g0 == maxg0 (exact-max bookkeeping)

    size_t size() const { return live; }

    // ---- dominance query in truncated space: exists live p with p[k] <= q[k], k=1..D-1
    template <bool BRANCHLESS>
    bool dom_flat(const size_t* q, uint64_t& cmp) const {
        const size_t* a = flat.data();
        const size_t n = flat.size() / D;
        if (use_lastw && lastw < n) {
            const size_t* b = a + lastw * D; ++cmp;
            int k = 1; for (; k < D; ++k) if (b[k] > q[k]) break;
            if (k == D) return true;
        }
        for (size_t i = 0; i < n; ++i, a += D) {
            ++cmp;
            if constexpr (BRANCHLESS) {
                bool ok = true;
                for (int k = 1; k < D; ++k) ok &= (a[k] <= q[k]);
                if (ok) { lastw = i; return true; }
            } else {
                int k = 1;
                for (; k < D; ++k) if (a[k] > q[k]) break;
                if (k == D) { lastw = i; return true; }
            }
        }
        return false;
    }
    mutable size_t lastw = 0;
    bool use_lastw = false;

    bool dom_tree(const size_t* q, uint64_t& cmp) const {
        if (root == UINT32_MAX) return false;
        static thread_local std::vector<uint32_t> stack; stack.resize(std::max<size_t>(stack.size(), nodes.size() + 2)); int sp = 0; stack[sp++] = root;
        while (sp) {
            const KNode& n = nodes[stack[--sp]];
            bool prune = false;
            for (int k = 0; k < P; ++k) if (n.lo[k] > q[k + 1]) { prune = true; break; }
            if (prune) continue;
            if (!n.dead) {
                ++cmp;
                int k = 1;
                for (; k < D; ++k) if (n.pt[k] > q[k]) break;
                if (k == D) return true;
            }
            if (n.r != UINT32_MAX) stack[sp++] = n.r;
            if (n.l != UINT32_MAX) stack[sp++] = n.l;
        }
        return false;
    }

    mutable uint64_t q_n = 0, q_cmp = 0;  // flat-mode scan statistics for adaptive promotion
    bool dom(const size_t* q, uint64_t& cmp, bool branchless) const {
        if (is_bk) return bk.template query<0>(q, cmp) != 0;
        if (is_tree) return dom_tree(q, cmp);
        uint64_t c0 = cmp;
        bool r = branchless ? dom_flat<true>(q, cmp) : dom_flat<false>(q, cmp);
        ++q_n; q_cmp += cmp - c0;
        return r;
    }

    // Witness query: returns 2 if exists live p <= q in ALL D coords (full dominance witness),
    // 1 if only Tr-dominators with p0 > q0 exist, 0 if not Tr-dominated.
    int dom_witness(const size_t* q, uint64_t& cmp) const {
        if (is_bk) return bk.template query<1>(q, cmp);
        int res = 0;
        if (!is_tree) {
            const size_t* a = flat.data(); const size_t n = flat.size() / D;
            ++q_n;
            for (size_t i = 0; i < n; ++i, a += D) {
                ++cmp; ++q_cmp; int k = 1; for (; k < D; ++k) if (a[k] > q[k]) break;
                if (k == D) { if (a[0] <= q[0]) return 2; res = 1; }
            }
            return res;
        }
        if (root == UINT32_MAX) return 0;
        static thread_local std::vector<uint32_t> stack; stack.resize(std::max<size_t>(stack.size(), nodes.size() + 2)); int sp = 0; stack[sp++] = root;
        while (sp) {
            const KNode& n = nodes[stack[--sp]];
            bool prune = false;
            for (int k = 0; k < P; ++k) if (n.lo[k] > q[k + 1]) { prune = true; break; }
            if (prune) continue;
            if (!n.dead) {
                ++cmp; int k = 1; for (; k < D; ++k) if (n.pt[k] > q[k]) break;
                if (k == D) { if (n.pt[0] <= q[0]) return 2; res = 1; }
            }
            if (n.r != UINT32_MAX) stack[sp++] = n.r;
            if (n.l != UINT32_MAX) stack[sp++] = n.l;
        }
        return res;
    }

    // ---- collect live points with p <= ub (truncated coords) into out (as pointers to D-vectors)
    void collect_le(const size_t* ub, std::vector<const size_t*>& out, uint64_t& cmp) const {
        if (is_bk) { bk.collect_le(ub, out, cmp); return; }
        if (!is_tree) {
            const size_t* a = flat.data(); const size_t n = flat.size() / D;
            for (size_t i = 0; i < n; ++i, a += D) {
                ++cmp; int k = 1; for (; k < D; ++k) if (a[k] > ub[k]) break;
                if (k == D) out.push_back(a);
            }
            return;
        }
        if (root == UINT32_MAX) return;
        static thread_local std::vector<uint32_t> stack; stack.resize(std::max<size_t>(stack.size(), nodes.size() + 2)); int sp = 0; stack[sp++] = root;
        while (sp) {
            const KNode& n = nodes[stack[--sp]];
            bool prune = false;
            for (int k = 0; k < P; ++k) if (n.lo[k] > ub[k + 1]) { prune = true; break; }
            if (prune) continue;
            if (!n.dead) {
                ++cmp; int k = 1; for (; k < D; ++k) if (n.pt[k] > ub[k]) break;
                if (k == D) out.push_back(n.pt);
            }
            if (n.r != UINT32_MAX) stack[sp++] = n.r;
            if (n.l != UINT32_MAX) stack[sp++] = n.l;
        }
    }

    // ---- update: remove live p with g[k] <= p[k] (k>=1), insert g.
    size_t rb_div = 4, rb_slack = 64;
    uint64_t promoteC = 0;
    void insert(const size_t* g, uint64_t& cmp, size_t X, bool exact_max, Stats& st) {
        if (!is_tree) {
            size_t w = 0; const size_t n = flat.size() / D;
            size_t mx = g[0];
            for (size_t i = 0; i < n; ++i) {
                const size_t* a = &flat[i * D];
                ++cmp;
                int k = 1; for (; k < D; ++k) if (a[k] < g[k]) break;
                if (k == D) continue;  // weakly dominated by g in Tr space -> drop
                if (a[0] > mx) mx = a[0];
                if (w != i) std::memmove(&flat[w * D], a, sizeof(size_t) * D);
                ++w;
            }
            flat.resize(w * D);
            flat.insert(flat.end(), g, g + D);
            live = w + 1;
            maxg0 = mx;
            if (promoteC == 0) { if (live >= X) promote(st); }
            else if (live >= X && q_n >= 16) {
                // cost model: promote once the recent mean flat scan length exceeds promoteC comparisons
                if (q_cmp >= promoteC * q_n) promote(st);
                else if (q_n > 4096) { q_n >>= 1; q_cmp >>= 1; }
            }
            return;
        }
        if (is_bk) {
            size_t killed = bk.remove_dominated(g, cmp, maxg0);
            bk.insert(g); live = bk.live;
            if (g[0] > maxg0) { maxg0 = g[0]; maxg0_cnt = 1; }
            else {
                if (g[0] == maxg0) ++maxg0_cnt;
                maxg0_cnt -= std::min(maxg0_cnt, killed);
                if (maxg0_cnt == 0) bk.max0(maxg0, maxg0_cnt);
            }
            if (bk.since_build > bk.built / rb_div + rb_slack) { bk.rebuild(); ++st.kd_rebuilds; }
            return;
        }
        // tree mode
        size_t killed_at_max = 0;
        if (root != UINT32_MAX) mark(g, cmp, killed_at_max);
        insert_tree(g);
        if (exact_max) {
            if (g[0] > maxg0) { maxg0 = g[0]; maxg0_cnt = 1; }
            else {
                if (g[0] == maxg0) ++maxg0_cnt;
                maxg0_cnt -= std::min(maxg0_cnt, killed_at_max);
                if (maxg0_cnt == 0) recompute_max();
            }
        } else if (g[0] > maxg0) maxg0 = g[0];
        if (total > built + built / rb_div + rb_slack) { rebuild(); ++st.kd_rebuilds; }
    }

    void promote(Stats& st) {
        std::vector<const size_t*> pts; pts.reserve(live);
        for (size_t i = 0; i < live; ++i) pts.push_back(&flat[i * D]);
        if (bk_cap) {
            bk.CAP = bk_cap; bk.build(pts); is_bk = true; is_tree = true;
            bk.max0(maxg0, maxg0_cnt);
            std::vector<size_t>().swap(flat); ++st.kd_builds; return;
        }
        nodes.clear(); nodes.reserve(live * 2);
        root = build(pts, 0, pts.size(), 0);
        is_tree = true; total = built = live;
        recompute_max();
        std::vector<size_t>().swap(flat);
        ++st.kd_builds;
    }

private:
    void recompute_max() {
        maxg0 = 0; maxg0_cnt = 0;
        for (const auto& n : nodes) if (!n.dead) {
            if (n.pt[0] > maxg0) { maxg0 = n.pt[0]; maxg0_cnt = 1; }
            else if (n.pt[0] == maxg0) ++maxg0_cnt;
        }
    }

    void mark(const size_t* g, uint64_t& cmp, size_t& killed_at_max) {
        static thread_local std::vector<uint32_t> stack; stack.resize(std::max<size_t>(stack.size(), nodes.size() + 2)); int sp = 0; stack[sp++] = root;
        while (sp) {
            KNode& n = nodes[stack[--sp]];
            bool prune = false;
            for (int k = 0; k < P; ++k) if (n.hi[k] < g[k + 1]) { prune = true; break; }
            if (prune) continue;
            if (!n.dead) {
                ++cmp;
                int k = 1; for (; k < D; ++k) if (g[k] > n.pt[k]) break;
                if (k == D) { n.dead = 1; --live; if (n.pt[0] == maxg0) ++killed_at_max; }
            }
            if (n.r != UINT32_MAX) stack[sp++] = n.r;
            if (n.l != UINT32_MAX) stack[sp++] = n.l;
        }
    }

    void init_node(KNode& n, const size_t* p, uint32_t axis) {
        std::memcpy(n.pt, p, sizeof(size_t) * D);
        for (int k = 0; k < P; ++k) n.lo[k] = n.hi[k] = p[k + 1];
        n.l = n.r = UINT32_MAX; n.axis = axis; n.dead = 0;
    }

    void insert_tree(const size_t* p) {
        ++total; ++live;
        if (root == UINT32_MAX) { nodes.emplace_back(); init_node(nodes.back(), p, 0); root = 0; return; }
        uint32_t cur = root;
        while (true) {
            KNode& n = nodes[cur];
            for (int k = 0; k < P; ++k) { if (p[k + 1] < n.lo[k]) n.lo[k] = p[k + 1]; if (p[k + 1] > n.hi[k]) n.hi[k] = p[k + 1]; }
            uint32_t a = n.axis;
            bool left = p[a + 1] < n.pt[a + 1];
            uint32_t c = left ? n.l : n.r;
            if (c == UINT32_MAX) {
                uint32_t id = (uint32_t)nodes.size();
                uint32_t na = (a + 1) % P;
                nodes.emplace_back();
                KNode& par = nodes[cur];
                if (left) par.l = id; else par.r = id;
                init_node(nodes[id], p, na);
                return;
            }
            cur = c;
        }
    }

    uint32_t build(std::vector<const size_t*>& pts, size_t lo, size_t hi, int depth) {
        if (lo >= hi) return UINT32_MAX;
        uint32_t axis = depth % P;
        size_t mid = lo + (hi - lo) / 2;
        std::nth_element(pts.begin() + lo, pts.begin() + mid, pts.begin() + hi,
                         [axis](const size_t* a, const size_t* b) { return a[axis + 1] < b[axis + 1]; });
        uint32_t id = (uint32_t)nodes.size();
        nodes.emplace_back();
        init_node(nodes[id], pts[mid], axis);
        uint32_t l = build(pts, lo, mid, depth + 1);
        uint32_t r = build(pts, mid + 1, hi, depth + 1);
        KNode& n = nodes[id];
        n.l = l; n.r = r;
        for (uint32_t c : {l, r}) if (c != UINT32_MAX)
            for (int k = 0; k < P; ++k) { n.lo[k] = std::min(n.lo[k], nodes[c].lo[k]); n.hi[k] = std::max(n.hi[k], nodes[c].hi[k]); }
        return id;
    }

    void rebuild() {
        std::vector<size_t> buf; buf.reserve(live * D);
        for (const auto& n : nodes) if (!n.dead) buf.insert(buf.end(), n.pt, n.pt + D);
        std::vector<const size_t*> pts; pts.reserve(live);
        for (size_t i = 0; i < buf.size(); i += D) pts.push_back(&buf[i]);
        nodes.clear();
        root = build(pts, 0, pts.size(), 0);
        total = built = live = pts.size();
        recompute_max();
    }
};

// ---------------------------------------------------------------------------------------------
// Heuristic index over H(s) (static). Two layouts:
//  * index-ordered implicit segment tree ("lex tree"): leaves are blocks of consecutive indices
//  * Roi's spatial K-d tree (max-spread median split) with sorted per-node index lists
// ---------------------------------------------------------------------------------------------
template <int D>
struct HTree {
    static constexpr int P = D - 1;
    size_t K = 0, B = 8, L = 0, Lp = 1;
    std::vector<size_t> htr;     // K*P truncated heuristics in index order
    std::vector<size_t> bmin, bmax;  // (2*Lp)*P node boxes (1-based heap layout)
    std::vector<size_t> sufmin;  // K*P suffix minima
    // Roi spatial tree
    struct RNode { uint32_t l, r, first, count, min_idx, max_idx, ioff, icnt; };
    std::vector<RNode> rn; std::vector<size_t> rmin, rmax; std::vector<uint32_t> ridx; std::vector<uint32_t> rleaf; // rleaf: item idx order
    bool built_lex = false, built_roi = false, built_suf = false;
    std::vector<uint32_t> rptr;  // rptr[j] = 1 + max{i<j : Tr(h_i) <= Tr(h_j)}, 0 if none
    size_t n_redundant = 0;
    void build_rptr() {
        rptr.assign(K, 0); n_redundant = 0;
        const size_t lim = std::min<size_t>(K, 48);  // cheap antichain probe on a prefix
        for (size_t j = 1; j < K; ++j) {
            if (j == lim && n_redundant == 0) return;  // prefix is an antichain: assume no redundancy (still sound)
            const size_t* b = &htr[j * P];
            for (size_t i = j; i-- > 0;) {
                const size_t* a = &htr[i * P]; int k = 0; for (; k < P; ++k) if (a[k] > b[k]) break;
                if (k == P) { rptr[j] = (uint32_t)(i + 1); ++n_redundant; break; }
            }
        }
    }

    void load(const std::vector<std::vector<size_t>>& H) {
        K = H.size(); htr.resize(K * P);
        for (size_t i = 0; i < K; ++i) for (int k = 0; k < P; ++k) htr[i * P + k] = H[i][k + 1];
    }
    void build_suffix() {
        sufmin.resize(K * P);
        for (size_t i = K; i-- > 0;) for (int k = 0; k < P; ++k)
            sufmin[i * P + k] = (i + 1 < K) ? std::min(htr[i * P + k], sufmin[(i + 1) * P + k]) : htr[i * P + k];
        built_suf = true;
    }
    void build_lex(size_t leafB) {
        B = leafB; L = (K + B - 1) / B; Lp = 1; while (Lp < L) Lp <<= 1;
        bmin.assign(2 * Lp * P, INF); bmax.assign(2 * Lp * P, 0);
        for (size_t lf = 0; lf < L; ++lf) {
            size_t node = Lp + lf;
            for (size_t i = lf * B; i < std::min(K, (lf + 1) * B); ++i)
                for (int k = 0; k < P; ++k) { bmin[node * P + k] = std::min(bmin[node * P + k], htr[i * P + k]); bmax[node * P + k] = std::max(bmax[node * P + k], htr[i * P + k]); }
        }
        for (size_t n = Lp - 1; n >= 1; --n)
            for (int k = 0; k < P; ++k) {
                bmin[n * P + k] = std::min(bmin[2 * n * P + k], bmin[(2 * n + 1) * P + k]);
                bmax[n * P + k] = std::max(bmax[2 * n * P + k], bmax[(2 * n + 1) * P + k]);
            }
        built_lex = true;
    }
    // node n (heap index) covers leaves [first_leaf, last_leaf]
    size_t first_idx(size_t n) const { while (n < Lp) n <<= 1; return (n - Lp) * B; }

    void build_roi(size_t leafB) {
        B = leafB;
        std::vector<uint32_t> items(K); for (size_t i = 0; i < K; ++i) items[i] = (uint32_t)i;
        rn.clear(); rmin.clear(); rmax.clear(); ridx.clear(); rleaf.clear();
        roi_rec(items, 0, K);
        built_roi = true;
    }
    uint32_t roi_rec(std::vector<uint32_t>& it, size_t lo, size_t hi) {
        uint32_t id = (uint32_t)rn.size(); rn.push_back({}); rmin.resize(rn.size() * P, INF); rmax.resize(rn.size() * P, 0);
        RNode nd{UINT32_MAX, UINT32_MAX, 0, 0, UINT32_MAX, 0, 0, 0};
        for (size_t j = lo; j < hi; ++j) {
            uint32_t i = it[j]; nd.min_idx = std::min(nd.min_idx, i); nd.max_idx = std::max(nd.max_idx, i);
            for (int k = 0; k < P; ++k) { rmin[id * P + k] = std::min(rmin[id * P + k], htr[i * P + k]); rmax[id * P + k] = std::max(rmax[id * P + k], htr[i * P + k]); }
        }
        std::vector<uint32_t> s(it.begin() + lo, it.begin() + hi); std::sort(s.begin(), s.end());
        nd.ioff = (uint32_t)ridx.size(); nd.icnt = (uint32_t)s.size(); ridx.insert(ridx.end(), s.begin(), s.end());
        if (hi - lo <= B) {
            nd.first = (uint32_t)rleaf.size(); nd.count = (uint32_t)(hi - lo);
            rleaf.insert(rleaf.end(), s.begin(), s.end());
            rn[id] = nd; return id;
        }
        int sd = 0; size_t best = 0;
        for (int k = 0; k < P; ++k) { size_t sp = rmax[id * P + k] - rmin[id * P + k]; if (sp > best) { best = sp; sd = k; } }
        size_t mid = lo + (hi - lo) / 2;
        std::nth_element(it.begin() + lo, it.begin() + mid, it.begin() + hi, [&](uint32_t a, uint32_t b) { return htr[a * P + sd] < htr[b * P + sd]; });
        rn[id] = nd;
        uint32_t l = roi_rec(it, lo, mid); uint32_t r = roi_rec(it, mid, hi);
        rn[id].l = l; rn[id].r = r;
        return id;
    }
};

// ---------------------------------------------------------------------------------------------
// Solver
// ---------------------------------------------------------------------------------------------
template <int D>
class Solver {
    static constexpr int P = D - 1;
public:
    const AdjacencyMatrix& adj;
    Cfg cfg;
    Stats st;
    std::pmr::unsynchronized_pool_resource node_pool;
    std::vector<Front<D>> F;
    std::vector<std::vector<size_t>> Gcl;  // flat full-D closed sets
    std::vector<HTree<D>> HT;
    std::vector<uint8_t> ht_state;         // 0 unknown, 1 tree built, 2 below threshold
    const MultiValuedHeuristic* H = nullptr;
    size_t target = 0;
    std::vector<const size_t*> cand;       // scratch for filtered-T traversal
    size_t cur_lo1 = 0; bool use_r = false;

    Solver(const AdjacencyMatrix& a, Cfg c) : adj(a), cfg(c) {
        F.resize(a.size() + 1); Gcl.resize(a.size() + 1);
        for (auto& f : F) { f.use_lastw = c.lastwit; f.rb_div = c.rb_div; f.rb_slack = c.rb_slack; f.bk_cap = c.bk_cap; f.promoteC = c.promoteC; }
    }

    // ---- T-dominance query for q = Tr(g)+Tr(h)
    inline bool domT(const size_t* q) {
        ++st.domT_calls;
        if (!st.prof) return F[target].dom(q, st.cmp_ch, cfg.branchless);
        int b = lg2b(F[target].size()); uint64_t t0 = tick();
        bool r = F[target].dom(q, st.cmp_ch, cfg.branchless);
        st.T_cyc[b] += tick() - t0; ++st.T_cnt[b]; return r;
    }

    bool global_check(const std::vector<size_t>& f) { return F[target].dom(f.data(), st.cmpchk, cfg.branchless); }

    bool local_check(const std::vector<size_t>& g, size_t id) {
        ++st.loc_calls;
        Front<D>& fr = F[id];
        ++st.loc_size_hist[lg2b(fr.size())];
        struct Acc { Stats& s; int b; uint64_t t; ~Acc() { if (s.prof) { s.loc_cyc[b] += tick() - t; ++s.loc_cnt[b]; } } } acc{st, lg2b(fr.size()), tick()};
        if (cfg.witness) {
            int w = fr.dom_witness(g.data(), st.cmpchk);
            if (w == 0) { ++st.loc_notr; return false; }
            if (w == 2) { ++st.loc_tdisc; return true; }
            if (fr.maxg0 <= g[0]) { ++st.loc_tdisc; return true; }
        } else {
            if (!fr.dom(g.data(), st.cmpchk, cfg.branchless)) { ++st.loc_notr; return false; }
            if (fr.maxg0 <= g[0]) { ++st.loc_tdisc; return true; }
        }
        uint64_t t0 = tick();
        const auto& G = Gcl[id]; const size_t n = G.size() / D; const size_t* a = G.data();
        bool dom = false;
        for (size_t i = 0; i < n; ++i, a += D) {
            ++st.cmpfull; int k = 0; for (; k < D; ++k) if (g[k] < a[k]) break;
            if (k == D) { dom = true; break; }
        }
        st.t_full += tick() - t0;
        if (dom) { ++st.loc_bad; return true; }
        ++st.loc_good; return false;
    }

    void gcl_insert(const std::vector<size_t>& g, size_t id) {
        auto& G = Gcl[id];
        if (cfg.gcl_prune) {
            size_t w = 0, n = G.size() / D;
            for (size_t i = 0; i < n; ++i) {
                const size_t* a = &G[i * D]; int k = 0; for (; k < D; ++k) if (g[k] > a[k]) break;
                if (k == D) continue;  // g <= a : a redundant for the existence test
                if (w != i) std::memmove(&G[w * D], a, sizeof(size_t) * D); ++w;
            }
            G.resize(w * D);
        }
        G.insert(G.end(), g.begin(), g.end());
    }

    // ---- CHOOSEH: first idx >= start with Tr(g)+Tr(h_idx) not dominated by T. Returns K if none.
    size_t chooseh(size_t s, const std::vector<size_t>& g, size_t start, bool reins = false) {
        const auto& Hs = (*H)[s];
        const size_t K = Hs.size();
        st.ch_H_sum += K; st.ch_T_sum += F[target].size();
        ++st.T_size_hist[lg2b(F[target].size())];
        if (start >= K) { ++st.ch_none; return K; }
        size_t q[D]; q[0] = 0;
        if (F[target].size() == 0) { ++st.ch_first; return start; }
        auto test = [&](const size_t* htr) { for (int k = 0; k < P; ++k) q[k + 1] = g[k + 1] + htr[k]; return domT(q); };
        bool use_tree = cfg.chooseh != 0 && K >= cfg.hX && K > 1;
        if (!use_tree && cfg.flatH) {
            HTree<D>& T = tree(s, false);
            if (cfg.suffix_ideal && start < K && test(&T.sufmin[start * P])) { ++st.ch_none; ++st.ch_ideal_prune; return K; }
            if (cfg.redund) {
                const size_t lo1 = reins ? start : start + 1;  // rptr stores i+1; refuted range is [lo1-1, i)
                for (size_t i = start; i < K; ++i) {
                    if (T.rptr[i] >= lo1) { ++st.ch_rskip; continue; }
                    if (!test(&T.htr[i * P])) { account(i, start); return i; }
                }
                ++st.ch_none; return K;
            }
            for (size_t i = start; i < K; ++i) if (!test(&T.htr[i * P])) { account(i, start); return i; }
            ++st.ch_none; return K;
        }
        if (!use_tree) {
            for (size_t i = start; i < K; ++i) {
                for (int k = 0; k < P; ++k) q[k + 1] = g[k + 1] + Hs[i][k + 1];
                if (!domT(q)) { account(i, start); return i; }
            }
            ++st.ch_none; return K;
        }
        HTree<D>& T = tree(s);
        cur_lo1 = reins ? start : start + 1; use_r = cfg.redund && !T.rptr.empty() && cfg.chooseh != 3;
        if (cfg.fastpath) { if (!test(&T.htr[start * P])) { account(start, start); return start; } ++start; if (start >= K) { ++st.ch_none; return K; } }
        if (cfg.suffix_ideal) { if (test(&T.sufmin[start * P])) { ++st.ch_none; ++st.ch_ideal_prune; return K; } }
        ++st.ch_tree_calls;
        size_t r = K;
        if (cfg.chooseh == 1) r = lex_topdown(T, g, start, 1);
        else if (cfg.chooseh == 2) r = lex_bottomup(T, g, start);
        else if (cfg.chooseh == 3) r = roi_topdown(T, g, start);
        if (r >= K) { ++st.ch_none; return K; }
        account(r, start); return r;
    }

    void account(size_t r, size_t start) {
        if (r == start) ++st.ch_first; else ++st.ch_later;
        st.ch_skip_sum += r - start;
    }

    HTree<D>& tree(size_t s, bool with_index = true) {
        if (HT.empty()) { HT.resize(H->size()); ht_state.assign(H->size(), 0); }
        if (ht_state[s] == 0) {
            uint64_t t0 = tick();
            HT[s].load((*H)[s]);
            if (with_index) { if (cfg.chooseh == 3) HT[s].build_roi(cfg.leafB); else HT[s].build_lex(cfg.leafB); }
            if (cfg.suffix_ideal) HT[s].build_suffix();
            if (cfg.redund) { HT[s].build_rptr(); st.h_redundant += HT[s].n_redundant; st.h_total += HT[s].K; }
            ht_state[s] = 1; ++st.htrees;
            st.t_build += tick() - t0;
        }
        return HT[s];
    }

    // q = Tr(g)+box ; Dom(T,q)
    bool dom_box(const std::vector<size_t>& g, const size_t* box) {
        size_t q[D]; q[0] = 0; for (int k = 0; k < P; ++k) q[k + 1] = g[k + 1] + box[k];
        return domT(q);
    }
    // filtered-T variants: test q against a candidate subset
    bool dom_cand(const size_t* q, size_t lo, size_t hi) {
        for (size_t i = lo; i < hi; ++i) {
            ++st.cmp_ch; const size_t* a = cand[i]; int k = 1; for (; k < D; ++k) if (a[k] > q[k]) break;
            if (k == D) return true;
        }
        return false;
    }

    // DFS over subtree n of the lex tree, all indices >= start. Returns K if none.
    size_t lex_search(HTree<D>& T, const std::vector<size_t>& g, size_t n, size_t start, size_t clo, size_t chi) {
        ++st.ch_nodes;
        const size_t K = T.K;
        size_t fi = T.first_idx(n);
        if (fi >= K) return K;  // empty padding
        // node fully >= start?  (callers guarantee fi >= start, or n partially covers start)
        size_t q[D]; q[0] = 0;
        if (cfg.filterT) {
            // candidates relevant for this node: t <= Tr(g)+max
            for (int k = 0; k < P; ++k) q[k + 1] = g[k + 1] + T.bmax[n * P + k];
            size_t nlo = cand.size();
            for (size_t i = clo; i < chi; ++i) {
                ++st.cmp_ch; const size_t* a = cand[i]; int k = 1; for (; k < D; ++k) if (a[k] > q[k]) break;
                if (k == D) cand.push_back(a);
            }
            size_t nhi = cand.size();
            size_t res = K;
            if (nhi == nlo) { res = std::max(fi, start); }  // accept rule: nothing in T can dominate any h in n
            else {
                for (int k = 0; k < P; ++k) q[k + 1] = g[k + 1] + T.bmin[n * P + k];
                if (!dom_cand(q, nlo, nhi)) {
                    if (n >= T.Lp) {
                        size_t lf = n - T.Lp;
                        for (size_t i = std::max(start, lf * T.B); i < std::min(K, (lf + 1) * T.B); ++i) {
                            for (int k = 0; k < P; ++k) q[k + 1] = g[k + 1] + T.htr[i * P + k];
                            if (!dom_cand(q, nlo, nhi)) { res = i; break; }
                        }
                    } else {
                        res = lex_search(T, g, 2 * n, start, nlo, nhi);
                        if (res >= K) res = lex_search(T, g, 2 * n + 1, start, nlo, nhi);
                    }
                }
            }
            cand.resize(nlo);
            return res;
        }
        if (dom_box(g, &T.bmin[n * P])) return K;
        if (!dom_box(g, &T.bmax[n * P])) return std::max(fi, start);
        if (n >= T.Lp) {
            size_t lf = n - T.Lp;
            for (size_t i = std::max(start, lf * T.B); i < std::min(K, (lf + 1) * T.B); ++i) {
                if (use_r && T.rptr[i] >= cur_lo1) { ++st.ch_rskip; continue; }
                if (!dom_box(g, &T.htr[i * P])) return i;
            }
            return K;
        }
        // children: skip left child if it lies entirely below start
        size_t mid = T.first_idx(2 * n + 1);
        if (start < mid) { size_t r = lex_search(T, g, 2 * n, start, clo, chi); if (r < K) return r; }
        return lex_search(T, g, 2 * n + 1, start, clo, chi);
    }

    void seed_cand(HTree<D>& T, const std::vector<size_t>& g, size_t n) {
        cand.clear();
        size_t ub[D]; ub[0] = 0; for (int k = 0; k < P; ++k) ub[k + 1] = g[k + 1] + T.bmax[n * P + k];
        F[target].collect_le(ub, cand, st.cmp_ch);
    }

    size_t lex_topdown(HTree<D>& T, const std::vector<size_t>& g, size_t start, size_t n) {
        if (cfg.filterT) { seed_cand(T, g, n); return lex_search(T, g, n, start, 0, cand.size()); }
        return lex_search(T, g, n, start, 0, 0);
    }

    size_t lex_bottomup(HTree<D>& T, const std::vector<size_t>& g, size_t start) {
        const size_t K = T.K;
        size_t lf = start / T.B;
        // scan remainder of the start leaf
        for (size_t i = start; i < std::min(K, (lf + 1) * T.B); ++i) {
            if (use_r && T.rptr[i] >= cur_lo1) { ++st.ch_rskip; continue; }
            if (!dom_box(g, &T.htr[i * P])) return i;
        }
        size_t n = T.Lp + lf;
        while (n > 1) {
            if ((n & 1) == 0) {
                size_t sib = n + 1;
                if (T.first_idx(sib) >= K) return K;
                size_t r;
                if (cfg.filterT) { seed_cand(T, g, sib); r = lex_search(T, g, sib, start, 0, cand.size()); }
                else r = lex_search(T, g, sib, start, 0, 0);
                if (r < K) return r;
            }
            n >>= 1;
        }
        return K;
    }

    size_t roi_topdown(HTree<D>& T, const std::vector<size_t>& g, size_t start) {
        size_t best = INF;
        auto rec = [&](auto& self, uint32_t id) -> void {
            ++st.ch_nodes;
            const auto& nd = T.rn[id];
            if (nd.max_idx < start || nd.min_idx >= best) return;
            if (dom_box(g, &T.rmin[id * P])) return;
            if (!dom_box(g, &T.rmax[id * P])) {
                auto b = T.ridx.begin() + nd.ioff, e = b + nd.icnt;
                auto it = std::lower_bound(b, e, (uint32_t)start);
                if (it != e && *it < best) best = *it;
                return;
            }
            if (nd.l == UINT32_MAX) {
                for (uint32_t j = 0; j < nd.count; ++j) {
                    uint32_t i = T.rleaf[nd.first + j];  // sorted ascending
                    if (i < start) continue;
                    if (i >= best) break;
                    if (!dom_box(g, &T.htr[i * P])) { best = i; break; }
                }
                return;
            }
            uint32_t a = nd.l, b = nd.r;
            if (T.rn[b].min_idx < T.rn[a].min_idx) std::swap(a, b);
            self(self, a); self(self, b);
        };
        rec(rec, 0);
        return best == INF ? T.K : best;
    }

    void run(size_t source, size_t tgt, const MultiValuedHeuristic& heur, SolutionSet& solutions) {
        H = &heur; target = tgt;
        const size_t Xl = cfg.localX, Xt = cfg.targetX;
        uint64_t T0 = tick();
        std::pmr::polymorphic_allocator<Node> alloc{&node_pool};
        std::priority_queue<NodePtr, std::vector<NodePtr>, CompareNodeByFValue> open;
        std::vector<size_t> zero(D, 0);
        open.push(std::allocate_shared<Node>(alloc, source, zero, zero, nullptr, zero));
        while (!open.empty()) {
            auto node = open.top(); open.pop();
            ++st.generations;
            uint64_t t0 = tick();
            bool gd = global_check(node->f);
            st.t_global += tick() - t0;
            if (gd) {
                t0 = tick(); ++st.ch_calls_re;
                size_t r = chooseh(node->id, node->g, node->h_idx + 1, true);
                st.t_ch_re += tick() - t0;
                if (r >= heur[node->id].size()) continue;
                open.push(std::allocate_shared<Node>(alloc, node->id, node->g, heur[node->id][r], node->parent, node->c, r));
                ++st.reinsertions;
                continue;
            }
            t0 = tick();
            bool ld = local_check(node->g, node->id);
            st.t_local += tick() - t0;
            if (ld) continue;
            t0 = tick();
            int ub = lg2b(F[node->id].size());
            F[node->id].insert(node->g.data(), st.cmpupd, node->id == target ? Xt : Xl, cfg.exact_max, st);
            if (st.prof) { st.upd_cyc[ub] += tick() - t0; ++st.upd_cnt[ub]; }
            gcl_insert(node->g, node->id);
            st.t_update += tick() - t0;
            ++st.expansions;
            if (node->id == target) { solutions.push_back(node); continue; }
            for (const auto& e : adj[node->id]) {
                std::vector<size_t> ng(node->g);
                for (int i = 0; i < D; ++i) ng[i] += e.cost[i];
                size_t r;
                if (cfg.local_first) {
                    t0 = tick(); ld = local_check(ng, e.target); st.t_local += tick() - t0;
                    if (ld) continue;
                    t0 = tick(); ++st.ch_calls_gen; r = chooseh(e.target, ng, 0); st.t_ch_gen += tick() - t0;
                    if (r >= heur[e.target].size()) continue;
                } else {
                    t0 = tick(); ++st.ch_calls_gen; r = chooseh(e.target, ng, 0); st.t_ch_gen += tick() - t0;
                    if (r >= heur[e.target].size()) continue;
                    t0 = tick(); ld = local_check(ng, e.target); st.t_local += tick() - t0;
                    if (ld) continue;
                }
                open.push(std::allocate_shared<Node>(alloc, e.target, ng, heur[e.target][r], node, e.cost, r));
            }
        }
        st.t_total = tick() - T0;
    }
};

}  // namespace proto






