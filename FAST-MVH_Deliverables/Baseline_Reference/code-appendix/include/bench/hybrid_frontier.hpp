// include/bench/hybrid_frontier.hpp
// S2: per-frontier dispatch. Holds a linear antichain while live size <= TAU,
// then migrates to an owned concrete KDIncFrontier. SOUND: both backends are
// exact; migration replays the antichain (mutually non-dominated points) into
// the tree, preserving the set. New class; reuses the unmodified KDIncFrontier.
#ifndef RZQ_BENCH_HYBRID_FRONTIER_H_
#define RZQ_BENCH_HYBRID_FRONTIER_H_
#include "bench/ifrontier.hpp"
#include "bench/kd_inc_frontier.hpp"
#include "vec_type.hpp"
#include <stdexcept>
#include <vector>
namespace rzq { namespace bench {

template<int TAU>
class HybridFrontier : public IFrontier {
public:
  explicit HybridFrontier(size_t dim) : _dim(dim) {
    if (dim == 0) throw std::invalid_argument("HybridFrontier: dim must be >= 1");
  }
  HybridFrontier(const HybridFrontier&) = delete;
  HybridFrontier& operator=(const HybridFrontier&) = delete;
  ~HybridFrontier() { delete _tree; }

  bool Check(const CostVec& g) override {
    if (g.empty()) return false;
    if (_tree) return _tree->Check(g);
    for (size_t i = 0; i < _lin.size(); ++i)
      if (basic::EpsDom(_lin[i], g)) return true;
    return false;
  }
  void Update(const CostVec& g) override {
    if (_tree) { _tree->Update(g); return; }
    std::vector<CostVec> keep; keep.reserve(_lin.size() + 1);
    for (size_t i = 0; i < _lin.size(); ++i)
      if (!basic::EpsDom(g, _lin[i])) keep.push_back(_lin[i]); // drop points g dominates
    keep.push_back(g);
    _lin.swap(keep);
    if (_lin.size() > (size_t)TAU) _migrate();
  }
  size_t Size() const override { return _tree ? _tree->Size() : _lin.size(); }

private:
  size_t _dim;
  std::vector<CostVec> _lin;
  KDIncFrontier* _tree = NULL;

  void _migrate() {
    _tree = new KDIncFrontier(_dim);
    for (size_t i = 0; i < _lin.size(); ++i) _tree->Update(_lin[i]);
    std::vector<CostVec>().swap(_lin);
  }
};
} }
#endif
