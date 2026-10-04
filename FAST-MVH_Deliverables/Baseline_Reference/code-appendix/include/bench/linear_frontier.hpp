// include/bench/linear_frontier.hpp
#ifndef RZQ_BENCH_LINEAR_FRONTIER_H_
#define RZQ_BENCH_LINEAR_FRONTIER_H_
#include "bench/ifrontier.hpp"
namespace rzq { namespace bench {
class LinearFrontier : public IFrontier {
public:
  bool Check(const CostVec& g) override;
  void Update(const CostVec& g) override;
  size_t Size() const override { return _pts.size(); }
  const std::vector<CostVec>& Points() const { return _pts; } // for oracle comparison
private:
  std::vector<CostVec> _pts;
};
} }
#endif
