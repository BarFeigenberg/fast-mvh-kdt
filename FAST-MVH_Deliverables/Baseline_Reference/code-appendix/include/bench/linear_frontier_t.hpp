// include/bench/linear_frontier_t.hpp
#ifndef RZQ_BENCH_LINEAR_FRONTIER_T_H_
#define RZQ_BENCH_LINEAR_FRONTIER_T_H_
#include "bench/ifrontier.hpp"
#include "bench/cost_store.hpp"
#include "bench/dom_cmp.hpp"
#include <vector>
namespace rzq { namespace bench {
template<typename Store, typename Cmp>
class LinearFrontierT : public IFrontier {
  typedef typename Store::Point Point;
public:
  bool Check(const CostVec& g) override {
    if (g.empty()) return false;
    _d = g.size();
    Point gp = Store::convert(g);
    for (size_t i = 0; i < _pts.size(); ++i)
      if (Cmp::dominates(_pts[i], gp, _d)) return true;
    return false;
  }
  void Update(const CostVec& g) override {
    _d = g.size();
    Point gp = Store::convert(g);
    std::vector<Point> kept; kept.reserve(_pts.size() + 1);
    for (size_t i = 0; i < _pts.size(); ++i)
      if (!Cmp::dominates(gp, _pts[i], _d)) kept.push_back(_pts[i]);
    kept.push_back(gp);
    _pts.swap(kept);
  }
  size_t Size() const override { return _pts.size(); }
private:
  std::vector<Point> _pts;
  size_t _d = 0;
};
} }
#endif
