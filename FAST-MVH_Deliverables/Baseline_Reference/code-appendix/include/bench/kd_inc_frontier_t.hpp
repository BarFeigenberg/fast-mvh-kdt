// include/bench/kd_inc_frontier_t.hpp
//
// Incremental k-d tree frontier with lazy (tombstone) deletion and amortized
// rebuild. Unlike KDFrontierT, which reconstructs the whole tree on every
// Update (O(n log n) per update), this backend mutates the tree in place:
//
//   * Insert: walk to a leaf by axis comparisons and attach there, refreshing
//     the per-subtree min/max corners (lo/hi) along the path. O(tree depth).
//   * Delete (points dominated by the inserted vector): mark the nodes dead
//     instead of restructuring. A dead node is skipped when testing dominance
//     but still routes to its children, so the lo/hi bounds remain valid
//     without recomputation. Found via an hi-bounded domination traversal.
//   * Rebuild: only when total nodes exceed REBUILD_FACTOR * live(at last
//     build) + REBUILD_SLACK. Rebuilding from the live set restores balance
//     and reclaims tombstones. Bounding total to ~2x the live size at the
//     previous build caps both the dead fraction and the number of
//     incremental inserts since the last balanced build, so the rebuild cost
//     amortizes to O(log n) per update.
//
// Correctness vs. a linear oracle. For minimization, a stored point p prunes a
// query g iff p weakly dominates g (p[i] <= g[i] for all i). Keeping points
// that a later vector dominates is harmless for Check: if g dominates p
// (g <= p) and p dominates a future query q (p <= q), then g <= q, so g would
// catch q anyway. We still tombstone dominated points to keep Size() equal to
// the non-dominated-set size and to keep the tree lean. The live count tracks
// inserts minus tombstones, which equals the oracle's set size op-for-op
// (every insert adds one to both; a tombstone fires exactly when the oracle
// drops a point).
#ifndef RZQ_BENCH_KD_INC_FRONTIER_T_H_
#define RZQ_BENCH_KD_INC_FRONTIER_T_H_
#include <stdexcept>
#include <algorithm>
#include <type_traits>
#include <vector>
#include "bench/ifrontier.hpp"
#include "bench/cost_store.hpp"
#include "bench/dom_cmp.hpp"
#include "bench/node_arena.hpp"
namespace rzq { namespace bench {
// WidestAxis: rebuild splits on the widest-spread axis instead of round-robin
// depth%dim (tighter child corners -> more query pruning). OrderedDescent: the
// query visits the child with the smaller lo-corner first, so the early-exit on
// a found dominator fires sooner. Both default off (the baseline kdinc behavior).
template<typename Store, typename Cmp, bool Arena,
         bool WidestAxis = false, bool OrderedDescent = false>
class KDIncFrontierT : public IFrontier {
  typedef typename Store::Point Point;
  struct Node { int axis; bool dead; Point pt; Point lo; Point hi; Node* l; Node* r; };
  static_assert(!Arena || std::is_trivially_destructible<Node>::value,
    "KDIncFrontierT: Arena=true requires a trivially destructible Node (array-backed Store), not VecDouble");
  static const int REBUILD_FACTOR = 2;
  static const int REBUILD_SLACK = 8;
public:
  explicit KDIncFrontierT(size_t dim) : _dim(dim) {
    if (dim == 0) throw std::invalid_argument("KDIncFrontierT: dim must be >= 1");
  }
  KDIncFrontierT(const KDIncFrontierT&) = delete;
  KDIncFrontierT& operator=(const KDIncFrontierT&) = delete;
  ~KDIncFrontierT() { if (!Arena) _free(_root); }
  bool Check(const CostVec& g) override {
    if (g.empty()) return false;
    _d = g.size();
    Point gp = Store::convert(g);
    return _query(_root, gp);
  }
  void Update(const CostVec& g) override {
    _d = g.size();
    Point gp = Store::convert(g);
    if (_root) _markDominated(_root, gp); // tombstone points the new vector dominates
    _insert(gp);                          // then add the new vector as a leaf
    if (_total > (size_t)REBUILD_FACTOR * _built + REBUILD_SLACK) _rebuild();
  }
  size_t Size() const override { return _live; }
private:
  size_t _dim, _d = 0;
  Node* _root = NULL;
  size_t _live = 0;   // live (non-dominated) points
  size_t _total = 0;  // total nodes in tree (live + tombstoned)
  size_t _built = 0;  // live count at the last rebuild
  NodeArena<Node> _arena;

  Node* newNode() { return Arena ? _arena.alloc() : new Node(); }
  void _free(Node* n) { if (!n) return; _free(n->l); _free(n->r); delete n; }

  void _initLeaf(Node* n, const Point& gp, int axis) {
    n->axis = axis; n->dead = false; n->pt = gp; n->lo = gp; n->hi = gp;
    n->l = NULL; n->r = NULL;
  }
  // Walk to a leaf, attaching gp; refresh lo/hi corners along the descent.
  void _insert(const Point& gp) {
    if (!_root) { _root = newNode(); _initLeaf(_root, gp, 0); ++_total; ++_live; return; }
    Node* n = _root;
    for (;;) {
      for (size_t d = 0; d < _dim; ++d) {
        if (gp[d] < n->lo[d]) n->lo[d] = gp[d];
        if (gp[d] > n->hi[d]) n->hi[d] = gp[d];
      }
      int a = n->axis;
      Node** child = (gp[a] < n->pt[a]) ? &n->l : &n->r;
      if (*child == NULL) { *child = newNode(); _initLeaf(*child, gp, (a + 1) % (int)_dim); break; }
      n = *child;
    }
    ++_total; ++_live;
  }
  // Does any live point weakly dominate g? Prune subtrees whose lo corner
  // already exceeds g on some axis (then no contained point can dominate g).
  double _loKey(Node* n) const { // smaller corner => more likely to dominate
    double s = 0.0; for (size_t d = 0; d < _dim; ++d) s += (double)n->lo[d]; return s;
  }
  bool _query(Node* n, const Point& g) const {
    if (!n) return false;
    for (size_t d = 0; d < _dim; ++d) { if (n->lo[d] > g[d]) return false; }
    if (!n->dead && Cmp::dominates(n->pt, g, _d)) return true;
    if (OrderedDescent && n->l && n->r) {
      Node* a = n->l; Node* b = n->r;
      if (_loKey(b) < _loKey(a)) { Node* t = a; a = b; b = t; }
      if (_query(a, g)) return true;
      return _query(b, g);
    }
    if (_query(n->l, g)) return true;
    return _query(n->r, g);
  }
  // Tombstone every live point that gp weakly dominates. Prune subtrees whose
  // hi corner is below gp on some axis (then no contained point is >= gp there,
  // so none can be dominated by gp).
  void _markDominated(Node* n, const Point& gp) {
    if (!n) return;
    for (size_t d = 0; d < _dim; ++d) { if (n->hi[d] < gp[d]) return; }
    if (!n->dead && Cmp::dominates(gp, n->pt, _d)) { n->dead = true; --_live; }
    _markDominated(n->l, gp);
    _markDominated(n->r, gp);
  }
  void _collectLive(Node* n, std::vector<Point>& out) const {
    if (!n) return;
    if (!n->dead) out.push_back(n->pt);
    _collectLive(n->l, out);
    _collectLive(n->r, out);
  }
  // Balanced bulk build over pts[lo,hi). nth_element finds the per-axis median
  // in O(n) rather than O(n log n) for a full sort.
  int _widestAxis(const std::vector<Point>& pts, size_t lo, size_t hi) const {
    int best = 0; double bestSpread = -1.0;
    for (size_t d = 0; d < _dim; ++d) {
      double mn = (double)pts[lo][d], mx = mn;
      for (size_t i = lo + 1; i < hi; ++i) {
        double v = (double)pts[i][d]; if (v < mn) mn = v; if (v > mx) mx = v;
      }
      double spread = mx - mn;
      if (spread > bestSpread) { bestSpread = spread; best = (int)d; }
    }
    return best;
  }
  Node* _build(std::vector<Point>& pts, size_t lo, size_t hi, int depth) {
    if (lo >= hi) return NULL;
    int axis = WidestAxis ? _widestAxis(pts, lo, hi) : (depth % (int)_dim);
    size_t mid = lo + (hi - lo) / 2;
    std::nth_element(pts.begin() + lo, pts.begin() + mid, pts.begin() + hi,
                     [&](const Point& a, const Point& b){ return a[axis] < b[axis]; });
    Node* n = newNode();
    n->axis = axis; n->dead = false; n->pt = pts[mid];
    n->l = _build(pts, lo, mid, depth + 1);
    n->r = _build(pts, mid + 1, hi, depth + 1);
    n->lo = n->pt; n->hi = n->pt;
    if (n->l) for (size_t d = 0; d < _dim; ++d) { n->lo[d] = std::min(n->lo[d], n->l->lo[d]); n->hi[d] = std::max(n->hi[d], n->l->hi[d]); }
    if (n->r) for (size_t d = 0; d < _dim; ++d) { n->lo[d] = std::min(n->lo[d], n->r->lo[d]); n->hi[d] = std::max(n->hi[d], n->r->hi[d]); }
    return n;
  }
  void _rebuild() {
    std::vector<Point> live; live.reserve(_live);
    _collectLive(_root, live);
    if (Arena) _arena.reset(); else _free(_root);
    _root = _build(live, 0, live.size(), 0);
    _total = live.size(); _live = live.size(); _built = _live;
  }
};
} }
#endif
