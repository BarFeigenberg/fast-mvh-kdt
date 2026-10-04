#ifndef FAST_MVH_CHUNKED_GCL_H
#define FAST_MVH_CHUNKED_GCL_H

#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <memory>

namespace fast_mvh {

template <int D>
class ChunkedGcl {
public:
    static constexpr uint32_t NIL = UINT32_MAX;

    struct Node {
        size_t pt[D];
        size_t lo[D];
        size_t hi[D];
        uint32_t l, r;
        uint8_t axis;
    };

    struct Arena {
        std::vector<Node> nodes;
        
        Arena() {
            nodes.reserve(1024 * 1024); // Reserve 1M nodes
        }
        
        uint32_t allocate() {
            uint32_t id = static_cast<uint32_t>(nodes.size());
            nodes.emplace_back();
            return id;
        }
    };

    explicit ChunkedGcl(Arena& arena, size_t chunk_size = 128) 
        : arena_(arena), chunk_size_(chunk_size) {
        flat_.reserve(chunk_size * D);
    }

    void insert(const size_t* g) {
        flat_.insert(flat_.end(), g, g + D);
        if (flat_.size() / D >= chunk_size_) {
            build_tree();
        }
    }

    bool contains_dominator(const size_t* q, uint64_t& cmp) const {
        // 1. Check flat buffer
        const size_t* a = flat_.data();
        size_t n_flat = flat_.size() / D;
        for (size_t i = 0; i < n_flat; ++i, a += D) {
            ++cmp;
            int k = 0;
            for (; k < D; ++k) if (q[k] < a[k]) break;
            if (k == D) return true;
        }

        // 2. Check all frozen trees
        if (trees_.empty()) return false;

        uint32_t stack[64];

        const auto& nodes = arena_.nodes;
        for (uint32_t root : trees_) {
            int top = 0;
            stack[top++] = root;

            while (top > 0) {
                uint32_t cur = stack[--top];
                const Node& n = nodes[cur];

            bool reject = false;
            for (int k = 0; k < D; ++k) {
                if (n.lo[k] > q[k]) { reject = true; break; }
            }
            if (reject) continue;

            bool accept = true;
            for (int k = 0; k < D; ++k) {
                if (n.hi[k] > q[k]) { accept = false; break; }
            }
            if (accept) return true;

            ++cmp;
            bool dom = true;
            for (int k = 0; k < D; ++k) {
                if (q[k] < n.pt[k]) { dom = false; break; }
            }
            if (dom) return true;

            const uint8_t ax = n.axis;
            if (q[ax] >= n.pt[ax]) {
                if (n.l != NIL) stack[top++] = n.l;
                if (n.r != NIL) stack[top++] = n.r;
            } else {
                if (n.l != NIL) stack[top++] = n.l;
            }
            } // end while
        } // end for
        return false;
    }

private:
    void build_tree() {
        size_t n = flat_.size() / D;
        std::vector<const size_t*> pts(n);
        for (size_t i = 0; i < n; ++i) {
            pts[i] = &flat_[i * D];
        }
        uint32_t root = build_recursive(pts, 0, n, 0);
        trees_.push_back(root);
        flat_.clear();
    }

    uint32_t build_recursive(std::vector<const size_t*>& pts, size_t l, size_t r, int depth) {
        if (l >= r) return NIL;
        const uint8_t axis = depth % D;
        const size_t mid = l + (r - l) / 2;

        std::nth_element(pts.begin() + l, pts.begin() + mid, pts.begin() + r,
            [axis](const size_t* a, const size_t* b) { return a[axis] < b[axis]; });

        uint32_t id = arena_.allocate();
        // Do not hold a reference to arena_.nodes[id] across another allocate()
        
        std::memcpy(arena_.nodes[id].pt, pts[mid], D * sizeof(size_t));
        std::memcpy(arena_.nodes[id].lo, pts[mid], D * sizeof(size_t));
        std::memcpy(arena_.nodes[id].hi, pts[mid], D * sizeof(size_t));
        arena_.nodes[id].axis = axis;

        uint32_t left_child = build_recursive(pts, l, mid, depth + 1);
        uint32_t right_child = build_recursive(pts, mid + 1, r, depth + 1);

        // Re-acquire reference because arena might have reallocated!
        Node& current = arena_.nodes[id];
        current.l = left_child;
        current.r = right_child;

        if (left_child != NIL) {
            const Node& lc = arena_.nodes[left_child];
            for (int k = 0; k < D; ++k) {
                if (lc.lo[k] < current.lo[k]) current.lo[k] = lc.lo[k];
                if (lc.hi[k] > current.hi[k]) current.hi[k] = lc.hi[k];
            }
        }
        if (right_child != NIL) {
            const Node& rc = arena_.nodes[right_child];
            for (int k = 0; k < D; ++k) {
                if (rc.lo[k] < current.lo[k]) current.lo[k] = rc.lo[k];
                if (rc.hi[k] > current.hi[k]) current.hi[k] = rc.hi[k];
            }
        }
        return id;
    }

    Arena& arena_;
    size_t chunk_size_;
    std::vector<uint32_t> trees_;
    std::vector<size_t> flat_;
};

} // namespace fast_mvh

#endif // FAST_MVH_CHUNKED_GCL_H
