// include/bench/cost_store.hpp
#ifndef RZQ_BENCH_COST_STORE_H_
#define RZQ_BENCH_COST_STORE_H_
#include "bench/ifrontier.hpp" // CostVec
#include <array>
#include <cstdint>
#include <vector>
namespace rzq { namespace bench {

// Baseline: heap std::vector<double>.
struct VecDouble {
  typedef std::vector<double> Point;
  static Point convert(const CostVec& g) { return g; }
};
// O2: fixed-capacity contiguous doubles.
template<int D>
struct ArrDouble {
  typedef std::array<double, D> Point;
  static Point convert(const CostVec& g) {
    Point p; for (int i = 0; i < D; ++i) p[i] = g[i]; return p;
  }
};
// O6: fixed-capacity contiguous int32 (costs are non-negative integers here).
template<int D>
struct ArrInt {
  typedef std::array<int32_t, D> Point;
  static Point convert(const CostVec& g) {
    Point p; for (int i = 0; i < D; ++i) p[i] = (int32_t)g[i]; return p;
  }
};

} }
#endif
