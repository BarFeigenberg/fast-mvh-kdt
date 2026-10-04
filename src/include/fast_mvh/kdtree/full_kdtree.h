#ifndef FAST_MVH_FULL_KDTREE_H
#define FAST_MVH_FULL_KDTREE_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace fast_mvh {

template <int D>
class FullKDTree {
public:
    static constexpr int32_t NIL = -1;

    struct Node {
        std::array<size_t, D> pt;
        std::array<size_t, D> lo;
        std::array<size_t, D> hi;
        int32_t left{NIL};
        int32_t right{NIL};
        uint8_t axis{0};
    };

    explicit FullKDTree(size_t promote_threshold = 16)
        : promote_threshold_(promote_threshold) {}

    size_t size() const noexcept { return count_; }

    void clear() {
        flat_.clear();
        nodes_.clear();
        root_ = NIL;
        is_tree_ = false;
        count_ = 0;
    }

    void insert(const size_t* g) {
        flat_.insert(flat_.end(), g, g + D);
        ++count_;
        // When size crosses threshold, or doubles, rebuild tree
        if (count_ >= promote_threshold_ && (!is_tree_ || count_ >= nodes_.size() * 2)) {
            rebuild();
        }
    }

    // Returns true if ANY point in the tree dominates q (p[k] <= q[k] for all k in [0, D-1])
    bool contains_dominator(const size_t* q, uint64_t& cmp) const {
        if (count_ == 0) return false;
        if (!is_tree_) {
            const size_t* a = flat_.data();
            for (size_t i = 0; i < count_; ++i, a += D) {
                ++cmp;
                int k = 0;
                for (; k < D; ++k) if (q[k] < a[k]) break;
                if (k == D) return true;
            }
            return false;
        }

        // Tree query using bounding box rejection and acceptance
        int32_t stack[64];
        int top = 0;
        stack[top++] = root_;

        while (top > 0) {
            int32_t cur = stack[--top];
            if (cur == NIL) continue;
            const auto& n = nodes_[cur];

            // Rejection test: if any lo[k] > q[k], no point in subtree can dominate q
            bool reject = false;
            for (int k = 0; k < D; ++k) {
                if (n.lo[k] > q[k]) {
                    reject = true;
                    break;
                }
            }
            if (reject) continue;

            // Acceptance test: if all hi[k] <= q[k], every point in subtree dominates q
            bool accept = true;
            for (int k = 0; k < D; ++k) {
                if (n.hi[k] > q[k]) {
                    accept = false;
                    break;
                }
            }
            if (accept) return true;

            // Point test
            ++cmp;
            bool dom = true;
            for (int k = 0; k < D; ++k) {
                if (q[k] < n.pt[k]) {
                    dom = false;
                    break;
                }
            }
            if (dom) return true;

            // Recurse: prioritize branch that is more promising
            const uint8_t ax = n.axis;
            if (q[ax] >= n.pt[ax]) {
                // Both branches could have dominators, push left first so right is checked, or vice versa
                if (n.left != NIL) stack[top++] = n.left;
                if (n.right != NIL) stack[top++] = n.right;
            } else {
                // Since q[ax] < n.pt[ax], right child (which has pt[ax] >= n.pt[ax] > q[ax]) cannot dominate q on axis ax!
                // Only left child can have points <= q[ax]
                if (n.left != NIL) stack[top++] = n.left;
            }
        }
        return false;
    }

private:
    void rebuild() {
        std::vector<const size_t*> pts;
        pts.reserve(count_);
        for (size_t i = 0; i < count_; ++i) {
            pts.push_back(&flat_[i * D]);
        }
        nodes_.clear();
        nodes_.reserve(count_);
        root_ = build_recursive(pts, 0, count_, 0);
        is_tree_ = true;
    }

    int32_t build_recursive(std::vector<const size_t*>& pts, size_t l, size_t r, int depth) {
        if (l >= r) return NIL;
        const uint8_t axis = depth % D;
        const size_t mid = l + (r - l) / 2;

        std::nth_element(pts.begin() + l, pts.begin() + mid, pts.begin() + r,
            [axis](const size_t* a, const size_t* b) { return a[axis] < b[axis]; });

        int32_t idx = static_cast<int32_t>(nodes_.size());
        nodes_.emplace_back();
        auto& n = nodes_.back();
        n.axis = axis;
        std::memcpy(n.pt.data(), pts[mid], D * sizeof(size_t));
        std::memcpy(n.lo.data(), pts[mid], D * sizeof(size_t));
        std::memcpy(n.hi.data(), pts[mid], D * sizeof(size_t));

        n.left = build_recursive(pts, l, mid, depth + 1);
        n.right = build_recursive(pts, mid + 1, r, depth + 1);

        // Update bounding box from children
        if (n.left != NIL) {
            const auto& lc = nodes_[n.left];
            for (int k = 0; k < D; ++k) {
                if (lc.lo[k] < n.lo[k]) n.lo[k] = lc.lo[k];
                if (lc.hi[k] > n.hi[k]) n.hi[k] = lc.hi[k];
            }
        }
        if (n.right != NIL) {
            const auto& rc = nodes_[n.right];
            for (int k = 0; k < D; ++k) {
                if (rc.lo[k] < n.lo[k]) n.lo[k] = rc.lo[k];
                if (rc.hi[k] > n.hi[k]) n.hi[k] = rc.hi[k];
            }
        }
        return idx;
    }

    std::vector<size_t> flat_;
    std::vector<Node> nodes_;
    int32_t root_{NIL};
    bool is_tree_{false};
    size_t count_{0};
    size_t promote_threshold_{16};
};

} // namespace fast_mvh

#endif // FAST_MVH_FULL_KDTREE_H
