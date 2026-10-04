#ifndef FAST_MVH_HEURISTIC_INDEX_H
#define FAST_MVH_HEURISTIC_INDEX_H

// Per-state index over the static heuristic set H(s) for CHOOSEH:
//   return min{ i >= start : Tr(g) + Tr(h_i) is not weakly dominated by T }  (|H(s)| if none),
// i.e. exactly Maya's lexicographic-index semantics.
//
// Built lazily on the first CHOOSEH at s. Three mechanisms, all exact:
//  1. Contiguous copy of Tr(H(s)) (no vector<vector> pointer chasing).
//  2. Redundancy pointers: rptr[j] = 1 + max{i < j : Tr(h_i) <= Tr(h_j)}. If h_i is already refuted
//     (t <= g+h_i for some t in T) then t <= g+h_j, so h_j is refuted without touching T.
//     A 48-element prefix probe skips the O(|H(s)|^2) construction for antichain MVHs (e.g. APEX).
//  3. Roi's static (D-1)-d K-d tree (median split on max spread, N.min/N.max boxes, sorted
//     per-node index lists) -- only built when |H(s)| >= tree_min. Traversal: test h[start] first
//     (fast path), then top-down with index-range pruning (node.max_idx < start, node.min_idx >= best),
//     children in min_idx order, Rule 1 (discard: g+N.min dominated), Rule 2 (accept: g+N.max not
//     dominated -> smallest index >= start in the node).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace fast_mvh {

template <int D>
class HeuristicIndex {
    static constexpr int P = D - 1;
    static constexpr size_t INF = std::numeric_limits<size_t>::max();

public:
    bool loaded() const noexcept { return loaded_; }
    size_t size() const noexcept { return K_; }
    bool has_tree() const noexcept { return !rn_.empty(); }
    size_t redundant() const noexcept { return n_redundant_; }

    void build(const std::vector<std::vector<size_t>>& H, size_t tree_min, size_t leaf_size = 8) {
        K_ = H.size();
        htr_.resize(K_ * P);
        for (size_t i = 0; i < K_; ++i)
            for (int k = 0; k < P; ++k) htr_[i * P + k] = H[i][k + 1];
        build_rptr();
        if (K_ >= tree_min && K_ > 1) build_roi(leaf_size);
        loaded_ = true;
    }

    // dom(q) must answer Dom(T, q) for a D-array q (q[0] ignored).
    // prev_refuted: h[start-1] is already known to be dominated (reinsertion after a global check).
    template <class G, class DomT>
    size_t choose(const G& g, size_t start, bool prev_refuted, DomT&& dom, uint64_t& nodes_visited) const {
        if (start >= K_) return K_;
        size_t q[D]; q[0] = 0;
        auto refuted = [&](size_t i) {
            const size_t* h = &htr_[i * P];
            for (int k = 0; k < P; ++k) q[k + 1] = g[k + 1] + h[k];
            return dom(q);
        };
        if (rn_.empty()) {
            const size_t lo1 = prev_refuted ? start : start + 1;
            for (size_t i = start; i < K_; ++i) {
                if (rptr_[i] >= lo1) continue;
                if (!refuted(i)) return i;
            }
            return K_;
        }
        if (!refuted(start)) return start;  // fast path: current candidate is valid, skip the tree
        const size_t from = start + 1;
        if (from >= K_) return K_;
        size_t best = INF;
        auto box = [&](const size_t* b) {
            for (int k = 0; k < P; ++k) q[k + 1] = g[k + 1] + b[k];
            return dom(q);
        };
        stk_.clear(); stk_.push_back(0);
        while (!stk_.empty()) {
            const uint32_t id = stk_.back(); stk_.pop_back();
            const RNode& nd = rn_[id];
            ++nodes_visited;
            if (nd.max_idx < from || nd.min_idx >= best) continue;
            if (box(&rmin_[id * P])) continue;                       // Rule 1: whole subtree refuted
            if (!box(&rmax_[id * P])) {                              // Rule 2: whole subtree valid
                auto b = ridx_.begin() + nd.ioff, e = b + nd.icnt;
                auto it = std::lower_bound(b, e, static_cast<uint32_t>(from));
                if (it != e && *it < best) best = *it;
                continue;
            }
            if (nd.l == NIL) {
                for (uint32_t j = 0; j < nd.icnt; ++j) {                // leaf list is sorted ascending
                    const uint32_t i = ridx_[nd.ioff + j];
                    if (i < from) continue;
                    if (i >= best) break;
                    if (!refuted(i)) { best = i; break; }
                }
                continue;
            }
            uint32_t a = nd.l, c = nd.r;
            if (rn_[c].min_idx < rn_[a].min_idx) std::swap(a, c);
            stk_.push_back(c); stk_.push_back(a);                     // visit smaller min_idx first
        }
        return best == INF ? K_ : best;
    }

private:
    static constexpr uint32_t NIL = UINT32_MAX;
    struct RNode { uint32_t l, r, min_idx, max_idx, ioff, icnt; };

    void build_rptr() {
        rptr_.assign(K_, 0); n_redundant_ = 0;
        const size_t probe = std::min<size_t>(K_, 48);
        for (size_t j = 1; j < K_; ++j) {
            if (j == probe && n_redundant_ == 0) return;  // antichain prefix: skip the quadratic pass
            const size_t* b = &htr_[j * P];
            for (size_t i = j; i-- > 0;) {
                const size_t* a = &htr_[i * P];
                int k = 0; for (; k < P; ++k) if (a[k] > b[k]) break;
                if (k == P) { rptr_[j] = static_cast<uint32_t>(i + 1); ++n_redundant_; break; }
            }
        }
    }

    void build_roi(size_t leaf) {
        std::vector<uint32_t> items(K_);
        for (size_t i = 0; i < K_; ++i) items[i] = static_cast<uint32_t>(i);
        rn_.clear(); rmin_.clear(); rmax_.clear(); ridx_.clear();
        roi_rec(items, 0, K_, leaf);
    }

    uint32_t roi_rec(std::vector<uint32_t>& it, size_t lo, size_t hi, size_t leaf) {
        const uint32_t id = static_cast<uint32_t>(rn_.size());
        rn_.push_back({NIL, NIL, UINT32_MAX, 0, 0, 0});
        rmin_.resize(rn_.size() * P, INF); rmax_.resize(rn_.size() * P, 0);
        RNode nd{NIL, NIL, UINT32_MAX, 0, 0, 0};
        for (size_t j = lo; j < hi; ++j) {
            const uint32_t i = it[j];
            nd.min_idx = std::min(nd.min_idx, i); nd.max_idx = std::max(nd.max_idx, i);
            for (int k = 0; k < P; ++k) {
                rmin_[id * P + k] = std::min(rmin_[id * P + k], htr_[i * P + k]);
                rmax_[id * P + k] = std::max(rmax_[id * P + k], htr_[i * P + k]);
            }
        }
        std::vector<uint32_t> s(it.begin() + lo, it.begin() + hi);
        std::sort(s.begin(), s.end());
        nd.ioff = static_cast<uint32_t>(ridx_.size()); nd.icnt = static_cast<uint32_t>(s.size());
        ridx_.insert(ridx_.end(), s.begin(), s.end());
        if (hi - lo <= leaf) { rn_[id] = nd; return id; }
        int sd = 0; size_t spread = 0;
        for (int k = 0; k < P; ++k) {
            const size_t sp = rmax_[id * P + k] - rmin_[id * P + k];
            if (sp > spread) { spread = sp; sd = k; }
        }
        const size_t mid = lo + (hi - lo) / 2;
        std::nth_element(it.begin() + lo, it.begin() + mid, it.begin() + hi,
                         [&](uint32_t a, uint32_t b) { return htr_[a * P + sd] < htr_[b * P + sd]; });
        rn_[id] = nd;
        const uint32_t l = roi_rec(it, lo, mid, leaf);
        const uint32_t r = roi_rec(it, mid, hi, leaf);
        rn_[id].l = l; rn_[id].r = r;
        return id;
    }

    bool loaded_ = false;
    size_t K_ = 0, n_redundant_ = 0;
    std::vector<size_t> htr_;
    std::vector<uint32_t> rptr_;
    std::vector<RNode> rn_;
    std::vector<size_t> rmin_, rmax_;
    std::vector<uint32_t> ridx_;
    mutable std::vector<uint32_t> stk_;
};

}  // namespace fast_mvh

#endif  // FAST_MVH_HEURISTIC_INDEX_H
