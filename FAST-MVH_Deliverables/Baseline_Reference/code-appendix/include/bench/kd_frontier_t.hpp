// include/bench/kd_frontier_t.hpp
#ifndef RZQ_BENCH_KD_FRONTIER_T_H_
#define RZQ_BENCH_KD_FRONTIER_T_H_
#include <stdexcept>
#include <algorithm>
#include <type_traits>
#include <vector>
#include "bench/ifrontier.hpp"
#include "bench/cost_store.hpp"
#include "bench/dom_cmp.hpp"
#include "bench/node_arena.hpp"
namespace rzq { namespace bench {
template<typename Store, typename Cmp, bool Arena>
class KDFrontierT : public IFrontier {
  typedef typename Store::Point Point;
  struct Node { int idx; int axis; Point lo; Node* l; Node* r; };
  static_assert(!Arena || std::is_trivially_destructible<Node>::value,
    "KDFrontierT: Arena=true requires a trivially destructible Node (array-backed Store), not VecDouble");
public:
  explicit KDFrontierT(size_t dim) : _dim(dim) {
    if (dim == 0) throw std::invalid_argument("KDFrontierT: dim must be >= 1");
  }
  KDFrontierT(const KDFrontierT&) = delete;
  KDFrontierT& operator=(const KDFrontierT&) = delete;
  ~KDFrontierT() { if (!Arena) _free(_root); }
  bool Check(const CostVec& g) override {
    if (g.empty()) return false;
    _d = g.size();
    Point gp = Store::convert(g); return _query(_root, gp);
  }
  void Update(const CostVec& g) override {
    _d = g.size(); Point gp = Store::convert(g);
    std::vector<Point> kept; kept.reserve(_pts.size() + 1);
    for (size_t i = 0; i < _pts.size(); ++i)
      if (!Cmp::dominates(gp, _pts[i], _d)) kept.push_back(_pts[i]);
    kept.push_back(gp); _pts.swap(kept); _rebuild();
  }
  size_t Size() const override { return _pts.size(); }
private:
  size_t _dim, _d = 0;
  std::vector<Point> _pts;
  Node* _root = NULL;
  NodeArena<Node> _arena;
  Node* newNode() { return Arena ? _arena.alloc() : new Node(); }
  void _free(Node* n) { if (!n) return; _free(n->l); _free(n->r); delete n; }
  Node* _build(std::vector<int>& ids, int depth) {
    if (ids.empty()) return NULL;
    int axis = depth % (int)_dim;
    std::sort(ids.begin(), ids.end(),
              [&](int a, int b){ return _pts[a][axis] < _pts[b][axis]; });
    size_t mid = ids.size() / 2;
    Node* n = newNode();
    n->idx = ids[mid]; n->axis = axis; n->l = NULL; n->r = NULL;
    std::vector<int> left(ids.begin(), ids.begin() + mid);
    std::vector<int> right(ids.begin() + mid + 1, ids.end());
    n->l = _build(left, depth + 1);
    n->r = _build(right, depth + 1);
    n->lo = _pts[n->idx];
    if (n->l) for (size_t d = 0; d < _dim; ++d) n->lo[d] = std::min(n->lo[d], n->l->lo[d]);
    if (n->r) for (size_t d = 0; d < _dim; ++d) n->lo[d] = std::min(n->lo[d], n->r->lo[d]);
    return n;
  }
  void _rebuild() {
    if (Arena) _arena.reset(); else _free(_root);
    _root = NULL;
    std::vector<int> ids(_pts.size());
    for (size_t i = 0; i < _pts.size(); ++i) ids[i] = (int)i;
    _root = _build(ids, 0);
  }
  bool _query(Node* n, const Point& g) const {
    if (!n) return false;
    for (size_t d = 0; d < _dim; ++d) { if (n->lo[d] > g[d]) return false; }
    if (Cmp::dominates(_pts[n->idx], g, _d)) return true;
    if (_query(n->l, g)) return true;
    return _query(n->r, g);
  }
};
} }
#endif
