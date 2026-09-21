// include/bench/kdinc_qa_frontier.hpp
// S1: incremental k-d frontier + query-aware construction. SOUND: only the
// build-time split-axis choice changes; Check/Update logic is identical to the
// baseline, so the result is unchanged for any axis choice (the linear oracle
// proves this). Check reservoir-samples recent queries; at each rebuild every
// node picks the axis that maximizes how many reservoir queries can prune at
// least one child after a median split on that axis. Off the hot path.
// New class so the baseline KDIncFrontier is untouched (spec isolation).
#ifndef RZQ_BENCH_KDINC_QA_FRONTIER_H_
#define RZQ_BENCH_KDINC_QA_FRONTIER_H_
#include "bench/ifrontier.hpp"
#include "vec_type.hpp"
#include <algorithm>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
namespace rzq { namespace bench {

class KDIncQAFrontier : public IFrontier {
public:
  explicit KDIncQAFrontier(size_t dim) : _dim(dim), _rng(12345u) {
    if (dim == 0) throw std::invalid_argument("KDIncQAFrontier: dim must be >= 1");
  }
  KDIncQAFrontier(const KDIncQAFrontier&) = delete;
  KDIncQAFrontier& operator=(const KDIncQAFrontier&) = delete;
  ~KDIncQAFrontier() { _free(_root); }

  bool Check(const CostVec& g) override {
    if (g.empty()) return false;
    ++_seen; // reservoir-sample for the next rebuild's axis choice
    if (_reservoir.size() < RES_CAP) _reservoir.push_back(g);
    else { size_t j = (size_t)(_rng() % _seen); if (j < RES_CAP) _reservoir[j] = g; }
    return _query(_root, g);
  }
  void Update(const CostVec& g) override {
    if (_root) _markDominated(_root, g);
    _insert(g);
    if (_total > (size_t)REBUILD_FACTOR * _built + REBUILD_SLACK) _rebuild();
  }
  size_t Size() const override { return _live; }

private:
  struct Node { int axis; bool dead; CostVec pt, lo, hi; Node* l; Node* r;
                Node() : axis(0), dead(false), l(NULL), r(NULL) {} };
  static const int REBUILD_FACTOR = 2, REBUILD_SLACK = 8;
  static const size_t RES_CAP = 256;
  size_t _dim;
  Node* _root = NULL;
  size_t _live = 0, _total = 0, _built = 0, _seen = 0;
  std::vector<CostVec> _reservoir;
  std::mt19937 _rng;

  void _free(Node* n) { if (!n) return; _free(n->l); _free(n->r); delete n; }
  void _initLeaf(Node* n, const CostVec& gp, int axis) {
    n->axis = axis; n->dead = false; n->pt = gp; n->lo = gp; n->hi = gp;
    n->l = NULL; n->r = NULL;
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
  bool _query(Node* n, const CostVec& g) const {
    if (!n) return false;
    for (size_t d = 0; d < _dim; ++d) { if (n->lo[d] > g[d]) return false; }
    if (!n->dead && basic::EpsDom(n->pt, g)) return true;
    if (_query(n->l, g)) return true;
    return _query(n->r, g);
  }
  void _markDominated(Node* n, const CostVec& gp) {
    if (!n) return;
    for (size_t d = 0; d < _dim; ++d) { if (n->hi[d] < gp[d]) return; }
    if (!n->dead && basic::EpsDom(gp, n->pt)) { n->dead = true; --_live; }
    _markDominated(n->l, gp);
    _markDominated(n->r, gp);
  }
  void _collectLive(Node* n, std::vector<CostVec>& out) const {
    if (!n) return;
    if (!n->dead) out.push_back(n->pt);
    _collectLive(n->l, out);
    _collectLive(n->r, out);
  }
  // Query-aware axis choice over pts[lo,hi): for each axis, median-split and
  // count reservoir queries that prune >= 1 child. Returns the best axis. Falls
  // back to round-robin when there is no query history or the node is tiny.
  int _qaAxis(std::vector<CostVec>& pts, size_t lo, size_t hi, int depth) {
    if (_reservoir.empty() || hi - lo < 4) return depth % (int)_dim;
    const double INF = std::numeric_limits<double>::max();
    int best = depth % (int)_dim; long bestScore = -1;
    for (size_t a = 0; a < _dim; ++a) {
      size_t mid = lo + (hi - lo) / 2;
      std::nth_element(pts.begin() + lo, pts.begin() + mid, pts.begin() + hi,
                       [&](const CostVec& x, const CostVec& y){ return x[a] < y[a]; });
      CostVec loL(_dim, INF), loR(_dim, INF);
      for (size_t i = lo; i < mid; ++i) for (size_t d = 0; d < _dim; ++d) loL[d] = std::min(loL[d], pts[i][d]);
      for (size_t i = mid + 1; i < hi; ++i) for (size_t d = 0; d < _dim; ++d) loR[d] = std::min(loR[d], pts[i][d]);
      long pr = 0;
      for (size_t q = 0; q < _reservoir.size(); ++q) {
        const CostVec& qv = _reservoir[q];
        bool visitL = true, visitR = true;
        for (size_t d = 0; d < _dim; ++d) { if (loL[d] > qv[d]) { visitL = false; break; } }
        for (size_t d = 0; d < _dim; ++d) { if (loR[d] > qv[d]) { visitR = false; break; } }
        if (!visitL || !visitR) ++pr;
      }
      if (pr > bestScore) { bestScore = pr; best = (int)a; }
    }
    return best;
  }
  Node* _build(std::vector<CostVec>& pts, size_t lo, size_t hi, int depth) {
    if (lo >= hi) return NULL;
    int axis = _qaAxis(pts, lo, hi, depth);
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
  void _rebuild() {
    std::vector<CostVec> live; live.reserve(_live);
    _collectLive(_root, live);
    _free(_root);
    _root = _build(live, 0, live.size(), 0);
    _total = _live = _built = live.size();
  }
};
} }
#endif
