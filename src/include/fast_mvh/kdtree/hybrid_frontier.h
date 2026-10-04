#ifndef FAST_MVH_HYBRID_FRONTIER_H
#define FAST_MVH_HYBRID_FRONTIER_H

// Truncated closed set G_cl^Tr(s) of L-NAMOA*dr-mvh with a size/cost-adaptive representation.
//
// * Small states: contiguous flat array (D-strided). One linear pass per update both removes the
//   Tr-weakly-dominated points and recomputes max g_1 exactly (Maya keeps the list sorted, so her
//   max is back()[0]; decisions only depend on the max, not on the order).
// * Once the state holds >= min_size points AND the running mean scan length of flat queries reaches
//   mean_scan comparisons, the set is promoted (once) into a dynamic (D-1)-d K-d tree
//   (Shahaf: insertion + tombstones + amortized rebuild, lo/hi corner pruning), with
//   dimension-sized nodes and iterative traversal.
//
// Both representations answer the same existence predicates over the same live set, so every
// search decision is identical to Maya's linear list.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fast_mvh {

struct PromotionPolicy {
    size_t min_size = 8;      // never promote below this many live points
    uint64_t mean_scan = 64;  // promote when mean flat comparisons per query >= this
};

template <int D>
class HybridFrontier {
    static_assert(D >= 2, "need at least two objectives");
    static constexpr int P = D - 1;

public:
    // Witness results for LOCALDOMCHECK.
    enum Witness : int { NONE = 0, TR_ONLY = 1, FULL = 2 };

    explicit HybridFrontier(PromotionPolicy pol = {}, size_t drop_dim = 0) : pol_(pol), drop_dim_(0) {}

    size_t size() const noexcept { return live_; }
    size_t max_g0() const noexcept { return max_g0_; }
    bool is_tree() const noexcept { return tree_; }

    void clear() {
        flat_.clear(); nodes_.clear(); root_ = NIL; tree_ = false;
        live_ = total_ = built_ = max_g0_ = max_cnt_ = 0; q_n_ = q_cmp_ = 0;
    }

    // exists live p with p[k] <= q[k] for k = 1..D-1
    bool dominated_tr(const size_t* q, uint64_t& cmp) const {
        if (!tree_) {
            const size_t* a = flat_.data();
            const size_t n = flat_.size() / D;
            uint64_t c = 0;
            bool hit = false;
            for (size_t i = 0; i < n; ++i, a += D) {
                ++c;
                if (le_tr(a, q)) { hit = true; break; }
            }
            cmp += c; ++q_n_; q_cmp_ += c;
            return hit;
        }
        return tree_query<false>(q, cmp) != NONE;
    }

    // FULL: a live p with p <= q in all D coordinates exists (then q is dominated by an expanded path).
    // TR_ONLY: only Tr-dominators with p_1 > q_1 exist.  NONE: not Tr-dominated.
    Witness witness(const size_t* q, uint64_t& cmp) const {
        if (!tree_) {
            const size_t* a = flat_.data();
            const size_t n = flat_.size() / D;
            Witness res = NONE;
            uint64_t c = 0;
            for (size_t i = 0; i < n; ++i, a += D) {
                ++c;
                if (le_tr(a, q)) {
                    if (a[drop_dim_] <= q[drop_dim_]) { res = FULL; break; }
                    res = TR_ONLY;
                }
            }
            cmp += c; ++q_n_; q_cmp_ += c;
            return res;
        }
        return tree_query<true>(q, cmp);
    }

    // Remove live p with g[k] <= p[k] (k >= 1), then insert g (Maya's UPDATE, same live set).
    void update(const size_t* g, uint64_t& cmp) {
        if (!tree_) {
            size_t w = 0, mx = g[drop_dim_];
            const size_t n = flat_.size() / D;
            for (size_t i = 0; i < n; ++i) {
                const size_t* a = &flat_[i * D];
                ++cmp;
                if (le_tr(g, a)) continue;
                if (a[drop_dim_] > mx) mx = a[drop_dim_];
                if (w != i) std::memmove(&flat_[w * D], a, sizeof(size_t) * D);
                ++w;
            }
            flat_.resize(w * D);
            flat_.insert(flat_.end(), g, g + D);
            live_ = w + 1;
            max_g0_ = mx;
            maybe_promote();
            return;
        }
        size_t killed_at_max = 0;
        if (root_ != NIL) mark(g, cmp, killed_at_max);
        insert(g);
        if (g[drop_dim_] > max_g0_) { max_g0_ = g[drop_dim_]; max_cnt_ = 1; }
        else {
            if (g[drop_dim_] == max_g0_) ++max_cnt_;
            max_cnt_ -= std::min(max_cnt_, killed_at_max);
            if (max_cnt_ == 0) recompute_max();
        }
        if (total_ > built_ + built_ / 4 + 64) rebuild();
    }

private:
    static constexpr uint32_t NIL = UINT32_MAX;
    struct Node {
        size_t pt[D];
        size_t lo[P];
        size_t hi[P];
        uint32_t l, r;
        uint32_t axis;
        uint32_t dead;
    };

    bool le_tr(const size_t* a, const size_t* b) const {
        for (int k = 0; k < D; ++k) {
            if (k == drop_dim_) continue;
            if (a[k] > b[k]) return false;
        }
        return true;
    }

    void maybe_promote() {
        if (live_ < pol_.min_size || q_n_ < 16) return;
        if (q_cmp_ >= pol_.mean_scan * q_n_) { promote(); return; }
        if (q_n_ > 4096) { q_n_ >>= 1; q_cmp_ >>= 1; }  // track recent behaviour
    }

    void promote() {
        std::vector<const size_t*> pts; pts.reserve(live_);
        for (size_t i = 0; i < live_; ++i) pts.push_back(&flat_[i * D]);
        nodes_.clear(); nodes_.reserve(live_ * 2);
        root_ = build(pts, 0, pts.size(), 0);
        tree_ = true; total_ = built_ = live_;
        recompute_max();
        std::vector<size_t>().swap(flat_);
    }

    template <bool WITNESS>
    Witness tree_query(const size_t* q, uint64_t& cmp) const {
        if (root_ == NIL) return NONE;
        Witness res = NONE;
        stack_.clear(); stack_.push_back(root_);
        while (!stack_.empty()) {
            const Node& n = nodes_[stack_.back()]; stack_.pop_back();
            bool prune = false;
            for (int k = 0; k < P; ++k) if (n.lo[k] > q[mapped(k)]) { prune = true; break; }
            if (prune) continue;
            if (!n.dead) {
                ++cmp;
                if (le_tr(n.pt, q)) {
                    if (!WITNESS || n.pt[drop_dim_] <= q[drop_dim_]) return WITNESS ? FULL : TR_ONLY;
                    res = TR_ONLY;
                }
            }
            if (n.r != NIL) stack_.push_back(n.r);
            if (n.l != NIL) stack_.push_back(n.l);
        }
        return res;
    }

    void mark(const size_t* g, uint64_t& cmp, size_t& killed_at_max) {
        stack_.clear(); stack_.push_back(root_);
        while (!stack_.empty()) {
            Node& n = nodes_[stack_.back()]; stack_.pop_back();
            bool prune = false;
            for (int k = 0; k < P; ++k) if (n.hi[k] < g[mapped(k)]) { prune = true; break; }
            if (prune) continue;
            if (!n.dead) {
                ++cmp;
                if (le_tr(g, n.pt)) { n.dead = 1; --live_; if (n.pt[drop_dim_] == max_g0_) ++killed_at_max; }
            }
            if (n.r != NIL) stack_.push_back(n.r);
            if (n.l != NIL) stack_.push_back(n.l);
        }
    }

    int mapped(int k) const { return k >= drop_dim_ ? k + 1 : k; }

    void init_node(Node& n, const size_t* p, uint32_t axis) const {
        std::memcpy(n.pt, p, sizeof(size_t) * D);
        for (int k = 0; k < P; ++k) n.lo[k] = n.hi[k] = p[mapped(k)];
        n.l = n.r = NIL; n.axis = axis; n.dead = 0;
    }

    void insert(const size_t* p) {
        ++total_; ++live_;
        if (root_ == NIL) { nodes_.emplace_back(); init_node(nodes_.back(), p, 0); root_ = 0; return; }
        uint32_t cur = root_;
        while (true) {
            Node& n = nodes_[cur];
            for (int k = 0; k < P; ++k) {
                if (p[mapped(k)] < n.lo[k]) n.lo[k] = p[mapped(k)];
                if (p[mapped(k)] > n.hi[k]) n.hi[k] = p[mapped(k)];
            }
            const uint32_t a = n.axis;
            const bool left = p[mapped(a)] < n.pt[mapped(a)];
            const uint32_t c = left ? n.l : n.r;
            if (c == NIL) {
                const uint32_t id = static_cast<uint32_t>(nodes_.size());
                nodes_.emplace_back();  // may reallocate: re-index the parent below
                if (left) nodes_[cur].l = id; else nodes_[cur].r = id;
                init_node(nodes_[id], p, (a + 1) % P);
                return;
            }
            cur = c;
        }
    }

    uint32_t build(std::vector<const size_t*>& pts, size_t lo, size_t hi, int depth) {
        if (lo >= hi) return NIL;
        const uint32_t axis = static_cast<uint32_t>(depth % P);
        const size_t mid = lo + (hi - lo) / 2;
        int m_axis = mapped(axis);
        std::nth_element(pts.begin() + lo, pts.begin() + mid, pts.begin() + hi,
                         [m_axis](const size_t* a, const size_t* b) { return a[m_axis] < b[m_axis]; });
        const uint32_t id = static_cast<uint32_t>(nodes_.size());
        nodes_.emplace_back();
        init_node(nodes_[id], pts[mid], axis);
        const uint32_t l = build(pts, lo, mid, depth + 1);
        const uint32_t r = build(pts, mid + 1, hi, depth + 1);
        Node& n = nodes_[id];
        n.l = l; n.r = r;
        for (uint32_t c : {l, r}) {
            if (c == NIL) continue;
            for (int k = 0; k < P; ++k) {
                n.lo[k] = std::min(n.lo[k], nodes_[c].lo[k]);
                n.hi[k] = std::max(n.hi[k], nodes_[c].hi[k]);
            }
        }
        return id;
    }

    void rebuild() {
        std::vector<size_t> buf; buf.reserve(live_ * D);
        for (const Node& n : nodes_) if (!n.dead) buf.insert(buf.end(), n.pt, n.pt + D);
        std::vector<const size_t*> pts; pts.reserve(live_);
        for (size_t i = 0; i < buf.size(); i += D) pts.push_back(&buf[i]);
        nodes_.clear();
        root_ = build(pts, 0, pts.size(), 0);
        total_ = built_ = live_ = pts.size();
        recompute_max();
    }

    void recompute_max() {
        max_g0_ = 0; max_cnt_ = 0;
        for (const Node& n : nodes_) {
            if (n.dead) continue;
            if (n.pt[drop_dim_] > max_g0_) { max_g0_ = n.pt[drop_dim_]; max_cnt_ = 1; }
            else if (n.pt[drop_dim_] == max_g0_) ++max_cnt_;
        }
    }

    PromotionPolicy pol_;
    std::vector<size_t> flat_;
    std::vector<Node> nodes_;
    mutable std::vector<uint32_t> stack_;
    uint32_t root_ = NIL;
    bool tree_ = false;
    size_t live_ = 0, total_ = 0, built_ = 0;
    size_t max_g0_ = 0, max_cnt_ = 0;
    mutable uint64_t q_n_ = 0, q_cmp_ = 0;
    size_t drop_dim_ = 0;
};

}  // namespace fast_mvh

#endif  // FAST_MVH_HYBRID_FRONTIER_H
