#ifndef FAST_MVH_DYNAMIC_FRONTIER_KDTREE_IMPL_H
#define FAST_MVH_DYNAMIC_FRONTIER_KDTREE_IMPL_H

namespace fast_mvh {

template<MemoryBackend Backend>
DynamicFrontierKDTree<Backend>::DynamicFrontierKDTree(size_t proj_dim) : proj_dim_(proj_dim) {
    if (proj_dim == 0) {
        throw std::invalid_argument("proj_dim must be >= 1");
    }
}

template<MemoryBackend Backend>
DynamicFrontierKDTree<Backend>::~DynamicFrontierKDTree() {
    clear();
}

template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::clear() {
    if constexpr (Backend == MemoryBackend::HEAP) {
        free_tree_ptr(root_ptr_);
        root_ptr_ = nullptr;
    } else if constexpr (Backend == MemoryBackend::EXPONENTIAL_ARENA) {
        // Blocks' vectors will self-destruct properly
        arena_blocks_.clear();
        root_ptr_ = nullptr;
    } else if constexpr (Backend == MemoryBackend::VECTOR_ARENA) {
        tree_nodes_.clear();
        root_idx_ = UINT32_MAX;
    }
    live_ = 0;
    total_ = 0;
    built_ = 0;
    max_g0_ = 0;
}

template<MemoryBackend Backend>
size_t DynamicFrontierKDTree<Backend>::get_max_g0() const {
    return max_g0_;
}

template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::update_max_g0() {
    // Only called after rebuild
    max_g0_ = 0;
    std::vector<std::vector<size_t>> live_pts;
    if constexpr (Backend == MemoryBackend::VECTOR_ARENA) {
        collect_live_idx(root_idx_, live_pts);
    } else {
        collect_live_ptr(root_ptr_, live_pts);
    }
    for (const auto& pt : live_pts) {
        if (pt[0] > max_g0_) max_g0_ = pt[0];
    }
}

template<MemoryBackend Backend>
bool DynamicFrontierKDTree<Backend>::check_dominated(const std::vector<size_t>& query, uint64_t& cmpchk_counter) const {
    if (live_ == 0) return false;
    
    if constexpr (Backend == MemoryBackend::VECTOR_ARENA) {
        return query_idx(root_idx_, query, cmpchk_counter);
    } else {
        return query_ptr(root_ptr_, query, cmpchk_counter);
    }
}

template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::update(const std::vector<size_t>& candidate, uint64_t& cmpupd_counter) {
    d_ = candidate.size();
    
    if constexpr (Backend == MemoryBackend::VECTOR_ARENA) {
        if (root_idx_ != UINT32_MAX) mark_dominated_idx(root_idx_, candidate, cmpupd_counter);
        insert_idx(candidate);
    } else {
        if (root_ptr_ != nullptr) mark_dominated_ptr(root_ptr_, candidate, cmpupd_counter);
        insert_ptr(candidate);
    }

    if (candidate[0] > max_g0_) {
        max_g0_ = candidate[0];
    }

    if (total_ > built_ + (built_ / 4) + 64) {
        rebuild();
    }
}

template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::init_leaf(Node& n, const std::vector<size_t>& pt, uint32_t axis) {
    n.axis = axis;
    n.dead = false;
    for (size_t i = 0; i < pt.size() && i < MAX_DIM; ++i) {
        n.pt[i] = pt[i];
        n.lo[i] = pt[i];
        n.hi[i] = pt[i];
    }
    n.l_ptr = nullptr;
    n.r_ptr = nullptr;
    n.l_idx = UINT32_MAX;
    n.r_idx = UINT32_MAX;
}

template<MemoryBackend Backend>
typename DynamicFrontierKDTree<Backend>::Node* DynamicFrontierKDTree<Backend>::alloc_node_ptr() {
    if constexpr (Backend == MemoryBackend::HEAP) {
        return new Node();
    } else if constexpr (Backend == MemoryBackend::EXPONENTIAL_ARENA) {
        if (arena_blocks_.empty() || arena_blocks_.back().used == arena_blocks_.back().nodes.size()) {
            size_t next_capacity = arena_blocks_.empty() ? 2 : std::min<size_t>(4096, arena_blocks_.back().nodes.size() * 2);
            arena_blocks_.emplace_back(next_capacity);
        }
        auto& block = arena_blocks_.back();
        Node* p = &block.nodes[block.used++];
        *p = Node(); // call constructor
        return p;
    }
    return nullptr;
}

template<MemoryBackend Backend>
uint32_t DynamicFrontierKDTree<Backend>::alloc_node_idx() {
    uint32_t idx = static_cast<uint32_t>(tree_nodes_.size());
    tree_nodes_.emplace_back();
    return idx;
}

template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::free_tree_ptr(Node* n) {
    if (!n) return;
    free_tree_ptr(n->l_ptr);
    free_tree_ptr(n->r_ptr);
    delete n;
}

// ---------------------------------------------------------
// INSERT
// ---------------------------------------------------------
template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::insert_ptr(const std::vector<size_t>& pt) {
    if (!root_ptr_) {
        root_ptr_ = alloc_node_ptr();
        init_leaf(*root_ptr_, pt, 0);
        ++total_; ++live_;
        return;
    }
    Node* n = root_ptr_;
    while (true) {
        for (size_t d = 1; d <= proj_dim_; ++d) {
            if (pt[d] < n->lo[d]) n->lo[d] = pt[d];
            if (pt[d] > n->hi[d]) n->hi[d] = pt[d];
        }
        uint32_t a = n->axis;
        Node** child = (pt[a + 1] < n->pt[a + 1]) ? &n->l_ptr : &n->r_ptr;
        if (*child == nullptr) {
            *child = alloc_node_ptr();
            init_leaf(**child, pt, (a + 1) % proj_dim_);
            break;
        }
        n = *child;
    }
    ++total_; ++live_;
}

template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::insert_idx(const std::vector<size_t>& pt) {
    if (root_idx_ == UINT32_MAX) {
        root_idx_ = alloc_node_idx();
        init_leaf(tree_nodes_[root_idx_], pt, 0);
        ++total_; ++live_;
        return;
    }
    uint32_t curr = root_idx_;
    while (true) {
        for (size_t d = 1; d <= proj_dim_; ++d) {
            if (pt[d] < tree_nodes_[curr].lo[d]) tree_nodes_[curr].lo[d] = pt[d];
            if (pt[d] > tree_nodes_[curr].hi[d]) tree_nodes_[curr].hi[d] = pt[d];
        }
        uint32_t a = tree_nodes_[curr].axis;
        bool go_left = (pt[a + 1] < tree_nodes_[curr].pt[a + 1]);
        uint32_t child_idx = go_left ? tree_nodes_[curr].l_idx : tree_nodes_[curr].r_idx;
        
        if (child_idx == UINT32_MAX) {
            uint32_t new_idx = alloc_node_idx();
            // Re-fetch parent after potential reallocation
            if (go_left) tree_nodes_[curr].l_idx = new_idx;
            else tree_nodes_[curr].r_idx = new_idx;
            init_leaf(tree_nodes_[new_idx], pt, (a + 1) % proj_dim_);
            break;
        }
        curr = child_idx;
    }
    ++total_; ++live_;
}

// ---------------------------------------------------------
// QUERY
// ---------------------------------------------------------
template<MemoryBackend Backend>
bool DynamicFrontierKDTree<Backend>::query_ptr(Node* n, const std::vector<size_t>& query, uint64_t& cmpchk_counter) const {
    if (!n) return false;
    for (size_t d = 1; d <= proj_dim_; ++d) {
        if (n->lo[d] > query[d]) return false;
    }
    
    if (!n->dead) {
        cmpchk_counter++;
        bool dominates = true;
        for (size_t d = 1; d <= proj_dim_; ++d) {
            if (n->pt[d] > query[d]) {
                dominates = false;
                break;
            }
        }
        if (dominates) return true;
    }

    if (query_ptr(n->l_ptr, query, cmpchk_counter)) return true;
    return query_ptr(n->r_ptr, query, cmpchk_counter);
}

template<MemoryBackend Backend>
bool DynamicFrontierKDTree<Backend>::query_idx(uint32_t n_idx, const std::vector<size_t>& query, uint64_t& cmpchk_counter) const {
    if (n_idx == UINT32_MAX) return false;
    const Node& n = tree_nodes_[n_idx];
    for (size_t d = 1; d <= proj_dim_; ++d) {
        if (n.lo[d] > query[d]) return false;
    }
    
    if (!n.dead) {
        cmpchk_counter++;
        bool dominates = true;
        for (size_t d = 1; d <= proj_dim_; ++d) {
            if (n.pt[d] > query[d]) {
                dominates = false;
                break;
            }
        }
        if (dominates) return true;
    }

    if (query_idx(n.l_idx, query, cmpchk_counter)) return true;
    return query_idx(n.r_idx, query, cmpchk_counter);
}

// ---------------------------------------------------------
// MARK DOMINATED
// ---------------------------------------------------------
template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::mark_dominated_ptr(Node* n, const std::vector<size_t>& pt, uint64_t& cmpupd_counter) {
    if (!n) return;
    for (size_t d = 1; d <= proj_dim_; ++d) {
        if (n->hi[d] < pt[d]) return;
    }
    
    if (!n->dead) {
        cmpupd_counter++;
        bool dominated = true;
        for (size_t d = 1; d <= proj_dim_; ++d) {
            if (pt[d] > n->pt[d]) {
                dominated = false;
                break;
            }
        }
        if (dominated) {
            n->dead = true;
            --live_;
        }
    }
    mark_dominated_ptr(n->l_ptr, pt, cmpupd_counter);
    mark_dominated_ptr(n->r_ptr, pt, cmpupd_counter);
}

template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::mark_dominated_idx(uint32_t n_idx, const std::vector<size_t>& pt, uint64_t& cmpupd_counter) {
    if (n_idx == UINT32_MAX) return;
    Node& n = tree_nodes_[n_idx];
    for (size_t d = 1; d <= proj_dim_; ++d) {
        if (n.hi[d] < pt[d]) return;
    }
    
    if (!n.dead) {
        cmpupd_counter++;
        bool dominated = true;
        for (size_t d = 1; d <= proj_dim_; ++d) {
            if (pt[d] > n.pt[d]) {
                dominated = false;
                break;
            }
        }
        if (dominated) {
            n.dead = true;
            --live_;
        }
    }
    mark_dominated_idx(n.l_idx, pt, cmpupd_counter);
    mark_dominated_idx(n.r_idx, pt, cmpupd_counter);
}

// ---------------------------------------------------------
// COLLECT LIVE
// ---------------------------------------------------------
template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::collect_live_ptr(Node* n, std::vector<std::vector<size_t>>& out) const {
    if (!n) return;
    if (!n->dead) out.push_back(std::vector<size_t>(n->pt.begin(), n->pt.begin() + d_));
    collect_live_ptr(n->l_ptr, out);
    collect_live_ptr(n->r_ptr, out);
}

template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::collect_live_idx(uint32_t n_idx, std::vector<std::vector<size_t>>& out) const {
    if (n_idx == UINT32_MAX) return;
    const Node& n = tree_nodes_[n_idx];
    if (!n.dead) out.push_back(std::vector<size_t>(n.pt.begin(), n.pt.begin() + d_));
    collect_live_idx(n.l_idx, out);
    collect_live_idx(n.r_idx, out);
}

// ---------------------------------------------------------
// BUILD
// ---------------------------------------------------------
template<MemoryBackend Backend>
typename DynamicFrontierKDTree<Backend>::Node* DynamicFrontierKDTree<Backend>::build_ptr(std::vector<std::vector<size_t>>& pts, size_t lo, size_t hi, int depth) {
    if (lo >= hi) return nullptr;
    uint32_t axis = depth % proj_dim_;
    size_t mid = lo + (hi - lo) / 2;
    
    std::nth_element(pts.begin() + lo, pts.begin() + mid, pts.begin() + hi,
                     [axis](const std::vector<size_t>& a, const std::vector<size_t>& b){ return a[axis + 1] < b[axis + 1]; });
                     
    Node* n = alloc_node_ptr();
    init_leaf(*n, pts[mid], axis);
    n->l_ptr = build_ptr(pts, lo, mid, depth + 1);
    n->r_ptr = build_ptr(pts, mid + 1, hi, depth + 1);
    
    if (n->l_ptr) {
        for (size_t d = 1; d <= proj_dim_; ++d) {
            n->lo[d] = std::min(n->lo[d], n->l_ptr->lo[d]);
            n->hi[d] = std::max(n->hi[d], n->l_ptr->hi[d]);
        }
    }
    if (n->r_ptr) {
        for (size_t d = 1; d <= proj_dim_; ++d) {
            n->lo[d] = std::min(n->lo[d], n->r_ptr->lo[d]);
            n->hi[d] = std::max(n->hi[d], n->r_ptr->hi[d]);
        }
    }
    return n;
}

template<MemoryBackend Backend>
uint32_t DynamicFrontierKDTree<Backend>::build_idx(std::vector<std::vector<size_t>>& pts, size_t lo, size_t hi, int depth) {
    if (lo >= hi) return UINT32_MAX;
    uint32_t axis = depth % proj_dim_;
    size_t mid = lo + (hi - lo) / 2;
    
    std::nth_element(pts.begin() + lo, pts.begin() + mid, pts.begin() + hi,
                     [axis](const std::vector<size_t>& a, const std::vector<size_t>& b){ return a[axis + 1] < b[axis + 1]; });
                     
    uint32_t n_idx = alloc_node_idx();
    // After alloc_node_idx, tree_nodes_ might reallocate, so don't hold references across alloc!
    init_leaf(tree_nodes_[n_idx], pts[mid], axis);
    
    uint32_t left_child = build_idx(pts, lo, mid, depth + 1);
    uint32_t right_child = build_idx(pts, mid + 1, hi, depth + 1);
    
    tree_nodes_[n_idx].l_idx = left_child;
    tree_nodes_[n_idx].r_idx = right_child;
    
    if (left_child != UINT32_MAX) {
        for (size_t d = 1; d <= proj_dim_; ++d) {
            tree_nodes_[n_idx].lo[d] = std::min(tree_nodes_[n_idx].lo[d], tree_nodes_[left_child].lo[d]);
            tree_nodes_[n_idx].hi[d] = std::max(tree_nodes_[n_idx].hi[d], tree_nodes_[left_child].hi[d]);
        }
    }
    if (right_child != UINT32_MAX) {
        for (size_t d = 1; d <= proj_dim_; ++d) {
            tree_nodes_[n_idx].lo[d] = std::min(tree_nodes_[n_idx].lo[d], tree_nodes_[right_child].lo[d]);
            tree_nodes_[n_idx].hi[d] = std::max(tree_nodes_[n_idx].hi[d], tree_nodes_[right_child].hi[d]);
        }
    }
    return n_idx;
}

template<MemoryBackend Backend>
void DynamicFrontierKDTree<Backend>::rebuild() {
    std::vector<std::vector<size_t>> live_pts;
    live_pts.reserve(live_);
    
    if constexpr (Backend == MemoryBackend::VECTOR_ARENA) {
        collect_live_idx(root_idx_, live_pts);
        tree_nodes_.clear(); // Invokes destructors correctly!
        root_idx_ = build_idx(live_pts, 0, live_pts.size(), 0);
    } else {
        collect_live_ptr(root_ptr_, live_pts);
        if constexpr (Backend == MemoryBackend::HEAP) {
            free_tree_ptr(root_ptr_);
            root_ptr_ = nullptr;
        } else {
            arena_blocks_.clear(); // Invokes destructors on all inner vectors
            root_ptr_ = nullptr;
        }
        root_ptr_ = build_ptr(live_pts, 0, live_pts.size(), 0);
    }
    
    total_ = live_pts.size();
    live_ = live_pts.size();
    built_ = live_;
    
    update_max_g0();
}

} // namespace fast_mvh

#endif // FAST_MVH_DYNAMIC_FRONTIER_KDTREE_IMPL_H
