// include/bench/sorted_linear_frontier.hpp
//
// Lexicographically-sorted linear frontier with early termination (BOA*-style).
// Unlike LinearFrontier (unsorted insertion-order scan), this backend keeps its
// points sorted by lexicographic order and can stop a Check as soon as the
// current point's leading coordinate exceeds the query's -- no later point can
// then dominate. Because the leading (primary) coordinate is what the sort and
// the early-termination test key on, the number of dominance checks depends on
// WHICH objective is primary, i.e. on the coordinate order. This is the linear
// analog of the trees' coordinate-order dependence, and the contrast with the
// unsorted LinearFrontier isolates that "does the structure use coordinate order"
// is exactly what makes a backend order-sensitive.
#ifndef RZQ_BENCH_SORTED_LINEAR_FRONTIER_H_
#define RZQ_BENCH_SORTED_LINEAR_FRONTIER_H_
#include "bench/ifrontier.hpp"
namespace rzq { namespace bench {
class SortedLinearFrontier : public IFrontier {
public:
  bool Check(const CostVec& g) override;
  void Update(const CostVec& g) override;
  size_t Size() const override { return _pts.size(); }
private:
  // kept sorted ascending by lexicographic order of the cost vector
  std::vector<CostVec> _pts;
};
} }
#endif
