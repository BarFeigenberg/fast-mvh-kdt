// include/bench/ifrontier.hpp
#ifndef RZQ_BENCH_IFRONTIER_H_
#define RZQ_BENCH_IFRONTIER_H_
#include <vector>
#include "bench/dom_metric.hpp"
namespace rzq { namespace bench {
typedef std::vector<double> CostVec;
// Frontier of mutually non-dominated, already-projected (M-1)-D cost vectors.
class IFrontier {
public:
  virtual ~IFrontier() {}
  // True iff some stored vector weakly dominates g (=> g is pruned).
  virtual bool Check(const CostVec& g) = 0;
  // Remove stored vectors dominated by g, then insert g.
  virtual void Update(const CostVec& g) = 0;
  virtual size_t Size() const = 0;
};
} }
#endif
