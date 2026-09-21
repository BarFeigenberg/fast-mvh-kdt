// include/bench/avl_frontier.hpp
#ifndef RZQ_BENCH_AVL_FRONTIER_H_
#define RZQ_BENCH_AVL_FRONTIER_H_
#include "bench/ifrontier.hpp"
#include "search_emoa.hpp"
namespace rzq { namespace bench {
class AVLFrontier : public IFrontier {
public:
  bool Check(const CostVec& g) override;
  void Update(const CostVec& g) override;
  size_t Size() const override { return _f.Size(); }
private:
  static CostVec _lift(const CostVec& g); // prepend 0.0
  rzq::search::Frontier _f;
  long _id = 0;
};
} }
#endif
