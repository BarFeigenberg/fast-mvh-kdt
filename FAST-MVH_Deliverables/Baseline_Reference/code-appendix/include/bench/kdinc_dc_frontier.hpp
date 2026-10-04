// include/bench/kdinc_dc_frontier.hpp
// S4: incremental k-d frontier + frequency-adaptive dominator cache. SOUND:
// the cache only reorders exact EpsDom tests (the K hottest live dominators
// are tested before the tree descent); it never changes a Check result.
// New class so the baseline KDIncFrontier is untouched (spec isolation).
//
// Cross-rebuild adaptivity: each Node counts how often it was the found
// dominator. At rebuild we fold cache-entry hits back into matching live nodes
// (value match), then reselect the top-K by hits. Hits reset per rebuild epoch
// (new nodes start at 0); this is an intentional, documented approximation for
// the triage study.
#ifndef RZQ_BENCH_KDINC_DC_FRONTIER_H_
#define RZQ_BENCH_KDINC_DC_FRONTIER_H_
#include "bench/ifrontier.hpp"
#include "vec_type.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
namespace rzq { namespace bench {

template<int K>
class KDIncDCFrontier : public IFrontier {
  static_assert(K > 0, "KDIncDCFrontier: K must be >= 1");
public:
  explicit KDIncDCFrontier(size_t dim) : _dim(dim) {
    if (dim == 0) throw std::invalid_argument("KDIncDCFrontier: dim must be >= 1");
  }
  KDIncDCFrontier(const KDIncDCFrontier&) = delete;
  KDIncDCFrontier& operator=(const KDIncDCFrontier&) = delete;
  ~KDIncDCFrontier() { _free(_root); }

  bool Check(const CostVec& g) override {
    if (g.empty()) return false;
    for (size_t i = 0; i < _cache.size(); ++i)
      if (basic::EpsDom(_cache[i].pt, g)) { ++_cache[i].hits; return true; }
    Node* d = _query(_root, g);
    if (d) { ++d->hits; return true; }
    return false;
  }
  void Update(const CostVec& g) override {
    if (_root) _markDominated(_root, g);
    _insert(g);
    if (_total > (size_t)REBUILD_FACTOR * _built + REBUILD_SLACK) _rebuild();
  }
  size_t Size() const override { return _live; }

private:
  struct Node { int axis; bool dead; CostVec pt, lo, hi; Node* l; Node* r; long long hits;
                Node() : axis(0), dead(false), l(NULL), r(NULL), hits(0) {} };
  struct CacheEnt { CostVec pt; long long hits; };
  static const int REBUILD_FACTOR = 2, REBUILD_SLACK = 8;
  size_t _dim;
  Node* _root = NULL;
  size_t _live = 0, _total = 0, _built = 0;
  std::vector<CacheEnt> _cache;

  void _free(Node* n) { if (!n) return; _free(n->l); _free(n->r); delete n; }
  void _initLeaf(Node* n, const CostVec& gp, int axis) {
    n->axis = axis; n->dead = false; n->pt = gp; n->lo = gp; n->hi = gp;
    n->l = NULL; n->r = NULL; n->hits = 0;
  }
  void _insert(const CostVec& gp) {
    if (!_root) { _root = new Node(); _initLeaf(_root, gp, 0); ++_total; ++_live; return; }
    Node* n = _root;
    for (;;) {
      for (size_t d = 0; d < _dim; ++d) {
        if (gp[d] < n->lo[d]) n->lo[d] = gp[d];
        if (gp[d] > n->hi[d]) n->hi[d] = gp[d];
      }
      int a = n->axis;
      Node** child = (gp[a] < n->pt[a]) ? &n->l : &n->r;
      if (*child == NULL) { *child = new Node(); _initLeaf(*child, gp, (a + 1) % (int)_dim); break; }
      n = *child;
    }
    ++_total; ++_live;
  }
  Node* _query(Node* n, const CostVec& g) const {
    if (!n) return NULL;
    for (size_t d = 0; d < _dim; ++d) { if (n->lo[d] > g[d]) return NULL; }
    if (!n->dead && basic::EpsDom(n->pt, g)) return n;
    Node* x = _query(n->l, g); if (x) return x;
    return _query(n->r, g);
  }
  void _markDominated(Node* n, const CostVec& gp) {
    if (!n) return;
    for (size_t d = 0; d < _dim; ++d) { if (n->hi[d] < gp[d]) return; }
    if (!n->dead && basic::EpsDom(gp, n->pt)) { n->dead = true; --_live; }
    _markDominated(n->l, gp);
    _markDominated(n->r, gp);
  }
  void _collectLiveNodes(Node* n, std::vector<Node*>& out) const {
    if (!n) return;
    if (!n->dead) out.push_back(n);
    _collectLiveNodes(n->l, out);
    _collectLiveNodes(n->r, out);
  }
  Node* _build(std::vector<CostVec>& pts, size_t lo, size_t hi, int depth) {
    if (lo >= hi) return NULL;
    int axis = depth % (int)_dim;
    size_t mid = lo + (hi - lo) / 2;
    std::nth_element(pts.begin() + lo, pts.begin() + mid, pts.begin() + hi,
                     [&](const CostVec& a, const CostVec& b){ return a[axis] < b[axis]; });
    Node* n = new Node();
    n->axis = axis; n->dead = false; n->pt = pts[mid];
    n->l = _build(pts, lo, mid, depth + 1);
    n->r = _build(pts, mid + 1, hi, depth + 1);
    n->lo = n->pt; n->hi = n->pt;
    if (n->l) for (size_t d = 0; d < _dim; ++d) { n->lo[d] = std::min(n->lo[d], n->l->lo[d]); n->hi[d] = std::max(n->hi[d], n->l->hi[d]); }
    if (n->r) for (size_t d = 0; d < _dim; ++d) { n->lo[d] = std::min(n->lo[d], n->r->lo[d]); n->hi[d] = std::max(n->hi[d], n->r->hi[d]); }
    return n;
  }
  void _selectCacheFrom(std::vector<Node*>& nodes) {
    for (size_t c = 0; c < _cache.size(); ++c)
      for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i]->pt == _cache[c].pt) { nodes[i]->hits += _cache[c].hits; break; }
    std::sort(nodes.begin(), nodes.end(),
              [](Node* a, Node* b){ return a->hits > b->hits; });
    _cache.clear();
    for (size_t i = 0; i < nodes.size() && i < static_cast<size_t>(K); ++i)
      if (nodes[i]->hits > 0) { CacheEnt e; e.pt = nodes[i]->pt; e.hits = 0; _cache.push_back(e); }
  }
  void _rebuild() {
    std::vector<Node*> liveNodes; _collectLiveNodes(_root, liveNodes);
    _selectCacheFrom(liveNodes);
    std::vector<CostVec> live; live.reserve(liveNodes.size());
    for (size_t i = 0; i < liveNodes.size(); ++i) live.push_back(liveNodes[i]->pt);
    _free(_root);
    _root = _build(live, 0, live.size(), 0);
    _total = _live = _built = live.size();
  }
};
} }
#endif
