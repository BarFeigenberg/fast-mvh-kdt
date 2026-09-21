// test/test_nd_inc_t.cpp
// Differential test: replay random streams against the incremental ND-tree
// (templated, across stores/comparators) and a linear oracle in lockstep.
#include "bench/nd_inc_frontier_t.hpp"
#include "bench/diff_harness.hpp"
#include "bench/test_util.hpp"
#include <cstdlib>
using namespace rzq::bench;
static OpStream RandStream(unsigned seed, size_t dim, int n) {
  std::srand(seed); OpStream s;
  for (int i = 0; i < n; ++i) {
    Op op; op.type = (std::rand() % 3 == 0) ? OP_UPDATE : OP_CHECK;
    for (size_t d = 0; d < dim; ++d) op.vec.push_back(std::rand() % 20);
    s.push_back(op);
  }
  return s;
}
int main() {
  std::string err;
  { NDIncFrontierT<VecDouble, CmpFast> f(2); BCHECK(RunDiff(&f, RandStream(1,2,800), &err), err.c_str()); }
  { NDIncFrontierT<VecDouble, CmpFast> f(3); BCHECK(RunDiff(&f, RandStream(2,3,800), &err), err.c_str()); }
  { NDIncFrontierT<VecDouble, CmpNaive> f(4); BCHECK(RunDiff(&f, RandStream(3,4,800), &err), err.c_str()); }
  { NDIncFrontierT<ArrDouble<3>, CmpFast> f(3); BCHECK(RunDiff(&f, RandStream(4,3,800), &err), err.c_str()); }
  { NDIncFrontierT<ArrInt<3>, CmpBranchless> f(3); BCHECK(RunDiff(&f, RandStream(5,3,800), &err), err.c_str()); }
  { NDIncFrontierT<ArrInt<5>, CmpFast> f(5); BCHECK(RunDiff(&f, RandStream(6,5,800), &err), err.c_str()); }
  // Narrow-range, deletion-heavy, frequent rebuilds.
  { NDIncFrontierT<VecDouble, CmpFast> f(2); BCHECK(RunDiff(&f, RandStream(7,2,2000), &err), err.c_str()); }
  // Tiny leaf capacity forces frequent splits.
  { NDIncFrontierT<VecDouble, CmpFast> f(3, 2); BCHECK(RunDiff(&f, RandStream(8,3,1200), &err), err.c_str()); }
  return 0;
}
