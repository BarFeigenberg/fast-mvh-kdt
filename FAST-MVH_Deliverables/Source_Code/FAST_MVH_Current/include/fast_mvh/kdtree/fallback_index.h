#ifndef FAST_MVH_FALLBACK_INDEX_H
#define FAST_MVH_FALLBACK_INDEX_H

// Exact full-D existence index "is there a stored p with p <= q (all D coordinates)?" over an
// append-only point set. Used by FAST3 for the LOCALDOMCHECK fallback set R(s).
//
// * insert() is an O(1) append. Indexing is lazy: no work until a query finds >= `buffer`
//   unindexed points, so states that never fall back never pay for a tree.
// * The indexed prefix is a Bentley-Saxe forest of O(log n) static implicit K-d trees: each block
//   is a contiguous range of pts_ permuted into K-d order (median at mid, axis = depth % D), and a
//   block is merged into its predecessor while the predecessor is < 2x its size.
// * lo_[mid] holds the componentwise minimum of the subtree rooted at mid (box rejection).
// * Ranges of <= LEAF points and the unindexed tail are scanned linearly.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include <algorithm>

namespace fast_mvh {

template <int D>
class FallbackIndex {
public:
    static constexpr size_t LEAF = 8;

    size_t size() const noexcept { return pts_.size() / D; }

    void insert(const size_t* p) { pts_.insert(pts_.end(), p, p + D); }

    bool contains_dominator(const size_t* q, size_t buffer, uint64_t& cmp) {
        const size_t n = size();
        if (n - indexed_ >= buffer && n > indexed_) index_tail();
        for (size_t i = indexed_; i < n; ++i) {
            ++cmp;
            if (le(&pts_[i * D], q)) return true;
        }
        for (size_t b = blocks_.size(); b-- > 0;)
            if (query_block(blocks_[b], q, cmp)) return true;
        return false;
    }

private:
    struct Block { size_t begin, end; };
    struct Frame { size_t a, b; uint32_t depth; };

    static bool le(const size_t* a, const size_t* b) {
        for (int k = 0; k < D; ++k) if (a[k] > b[k]) return false;
        return true;
    }

    void index_tail() {
        blocks_.push_back({indexed_, size()});
        indexed_ = size();
        while (blocks_.size() >= 2) {
            Block& prev = blocks_[blocks_.size() - 2];
            const Block& last = blocks_.back();
            if (prev.end - prev.begin >= 2 * (last.end - last.begin)) break;
            prev.end = last.end;
            blocks_.pop_back();
        }
        build(blocks_.back());
    }

    void build(const Block& blk) {
        const size_t m = blk.end - blk.begin;
        idx_.resize(m);
        for (size_t i = 0; i < m; ++i) idx_[i] = static_cast<uint32_t>(blk.begin + i);
        arrange(0, m, 0);
        tmp_.resize(m * D);
        for (size_t i = 0; i < m; ++i) std::memcpy(&tmp_[i * D], &pts_[idx_[i] * size_t(D)], sizeof(size_t) * D);
        std::memcpy(&pts_[blk.begin * D], tmp_.data(), sizeof(size_t) * D * m);
        lo_.resize(pts_.size());
        size_t root_lo[D];
        compute_lo(blk.begin, blk.end, root_lo);
    }

    void arrange(size_t l, size_t r, uint32_t depth) {
        if (r - l <= LEAF) return;
        const size_t mid = l + (r - l) / 2;
        const int ax = static_cast<int>(depth % D);
        std::nth_element(idx_.begin() + l, idx_.begin() + mid, idx_.begin() + r,
                         [&](uint32_t a, uint32_t b) { return pts_[a * size_t(D) + ax] < pts_[b * size_t(D) + ax]; });
        arrange(l, mid, depth + 1);
        arrange(mid + 1, r, depth + 1);
    }

    void compute_lo(size_t a, size_t b, size_t* out) {
        for (int k = 0; k < D; ++k) out[k] = std::numeric_limits<size_t>::max();
        if (b - a <= LEAF) {
            for (size_t i = a; i < b; ++i)
                for (int k = 0; k < D; ++k) out[k] = std::min(out[k], pts_[i * D + k]);
            return;
        }
        const size_t mid = a + (b - a) / 2;
        size_t* L = &lo_[mid * D];
        std::memcpy(L, &pts_[mid * D], sizeof(size_t) * D);
        size_t c[D];
        compute_lo(a, mid, c);
        for (int k = 0; k < D; ++k) L[k] = std::min(L[k], c[k]);
        compute_lo(mid + 1, b, c);
        for (int k = 0; k < D; ++k) L[k] = std::min(L[k], c[k]);
        std::memcpy(out, L, sizeof(size_t) * D);
    }

    bool query_block(const Block& blk, const size_t* q, uint64_t& cmp) const {
        Frame st[128];
        int top = 0;
        st[top++] = {blk.begin, blk.end, 0};
        while (top > 0) {
            const Frame f = st[--top];
            if (f.b - f.a <= LEAF) {
                for (size_t i = f.a; i < f.b; ++i) {
                    ++cmp;
                    if (le(&pts_[i * D], q)) return true;
                }
                continue;
            }
            const size_t mid = f.a + (f.b - f.a) / 2;
            if (!le(&lo_[mid * D], q)) continue;  // some lo[k] > q[k]: no dominator in subtree
            const size_t* p = &pts_[mid * D];
            ++cmp;
            if (le(p, q)) return true;
            const int ax = static_cast<int>(f.depth % D);
            if (q[ax] >= p[ax]) st[top++] = {mid + 1, f.b, f.depth + 1};  // right side has p'[ax] >= p[ax]
            st[top++] = {f.a, mid, f.depth + 1};
        }
        return false;
    }

    std::vector<size_t> pts_;   // D-strided; [0, indexed_) arranged into blocks_
    std::vector<size_t> lo_;    // D-strided, valid at internal-node positions
    std::vector<Block> blocks_; // sizes decrease by >= 2x
    size_t indexed_ = 0;
    std::vector<uint32_t> idx_;
    std::vector<size_t> tmp_;
};

}  // namespace fast_mvh

#endif  // FAST_MVH_FALLBACK_INDEX_H
