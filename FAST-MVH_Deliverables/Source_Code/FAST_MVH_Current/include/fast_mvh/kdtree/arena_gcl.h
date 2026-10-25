#ifndef FAST_MVH_ARENA_GCL_H
#define FAST_MVH_ARENA_GCL_H

#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace fast_mvh {

template <int D>
class ArenaGcl {
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
        Arena() { nodes.reserve(8 * 1024 * 1024); } // 8M nodes
        uint32_t allocate() {
            uint32_t id = static_cast<uint32_t>(nodes.size());
            nodes.emplace_back();
            return id;
        }
    };

    explicit ArenaGcl(Arena& arena) : arena_(arena), processed_(0) {}

    void update_tree(std::vector<size_t>& flat) {
        size_t n = flat.size() / D;
        size_t chunk_size = 128;
        if (n - processed_ < chunk_size) return;

        std::vector<const size_t*> pts(chunk_size);
        for (size_t i = 0; i < chunk_size; ++i) {
            pts[i] = &flat[(processed_ + i) * D];
        }
        uint32_t root = build_recursive(pts, 0, chunk_size, 0);
        trees_.push_back(root);
        processed_ += chunk_size;
    }

    bool contains_dominator(const size_t* q, const std::vector<size_t>& flat, uint64_t& cmp) const {
        size_t n = flat.size() / D;
        const size_t* a = flat.data() + processed_ * D;
        for (size_t i = processed_; i < n; ++i, a += D) {
            ++cmp;
            int k = 0;
            for (; k < D; ++k) if (q[k] < a[k]) break;
            if (k == D) return true;
        }

        if (trees_.empty()) return false;

        uint32_t stack[64];
        const auto& nodes = arena_.nodes;
        
        for (uint32_t root : trees_) {
            int top = 0;
            stack[top++] = root;

            while (top > 0) {
                uint32_t cur = stack[--top];
                const Node& node = nodes[cur];

                bool reject = false;
                for (int k = 0; k < D; ++k) {
                    if (node.lo[k] > q[k]) { reject = true; break; }
                }
                if (reject) continue;

                ++cmp;
                bool dom = true;
                for (int k = 0; k < D; ++k) {
                    if (q[k] < node.pt[k]) { dom = false; break; }
                }
                if (dom) return true;

                const uint8_t ax = node.axis;
                if (q[ax] >= node.pt[ax]) {
                    if (node.l != NIL) stack[top++] = node.l;
                    if (node.r != NIL) stack[top++] = node.r;
                } else {
                    if (node.l != NIL) stack[top++] = node.l;
                }
            }
        }
        return false;
    }

private:
    uint32_t build_recursive(std::vector<const size_t*>& pts, size_t l, size_t r, int depth) {
        if (l >= r) return NIL;
        const uint8_t axis = depth % D;
        const size_t mid = l + (r - l) / 2;

        std::nth_element(pts.begin() + l, pts.begin() + mid, pts.begin() + r,
            [axis](const size_t* a, const size_t* b) { return a[axis] < b[axis]; });

        uint32_t id = arena_.allocate();
        
        std::memcpy(arena_.nodes[id].pt, pts[mid], D * sizeof(size_t));
        std::memcpy(arena_.nodes[id].lo, pts[mid], D * sizeof(size_t));
        std::memcpy(arena_.nodes[id].hi, pts[mid], D * sizeof(size_t));
        arena_.nodes[id].axis = axis;

        uint32_t left_child = build_recursive(pts, l, mid, depth + 1);
        uint32_t right_child = build_recursive(pts, mid + 1, r, depth + 1);

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
    size_t processed_;
    std::vector<uint32_t> trees_;
};

} // namespace fast_mvh

#endif // FAST_MVH_ARENA_GCL_H
