// test/test_kd_inc_t.cpp
// Differential test: replay random streams against the incremental k-d tree
// (templated, across stores/comparators/arena) and a linear oracle in lockstep.
#include "bench/kd_inc_frontier_t.hpp"
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
  // Vector store, plain comparator, no arena.
  { KDIncFrontierT<VecDouble, CmpFast, false> f(2); BCHECK(RunDiff(&f, RandStream(1,2,800), &err), err.c_str()); }
  { KDIncFrontierT<VecDouble, CmpFast, false> f(3); BCHECK(RunDiff(&f, RandStream(2,3,800), &err), err.c_str()); }
  { KDIncFrontierT<VecDouble, CmpNaive, false> f(4); BCHECK(RunDiff(&f, RandStream(3,4,800), &err), err.c_str()); }
  // Array stores with arena (exercises tombstone reuse across rebuilds).
  { KDIncFrontierT<ArrDouble<3>, CmpFast, true> f(3); BCHECK(RunDiff(&f, RandStream(4,3,800), &err), err.c_str()); }
  { KDIncFrontierT<ArrInt<3>, CmpBranchless, true> f(3); BCHECK(RunDiff(&f, RandStream(5,3,800), &err), err.c_str()); }
  { KDIncFrontierT<ArrInt<5>, CmpFast, true> f(5); BCHECK(RunDiff(&f, RandStream(6,5,800), &err), err.c_str()); }
  // Small narrow-range stream: heavy domination + frequent rebuilds.
  { KDIncFrontierT<VecDouble, CmpFast, false> f(2); BCHECK(RunDiff(&f, RandStream(7,2,1500), &err), err.c_str()); }
  // WidestAxis / OrderedDescent tuning flags must not change correctness.
  { KDIncFrontierT<VecDouble, CmpFast, false, true,  false> f(3); BCHECK(RunDiff(&f, RandStream(8,3,1000), &err), err.c_str()); }
  { KDIncFrontierT<VecDouble, CmpFast, false, false, true > f(3); BCHECK(RunDiff(&f, RandStream(9,3,1000), &err), err.c_str()); }
  { KDIncFrontierT<VecDouble, CmpFast, false, true,  true > f(4); BCHECK(RunDiff(&f, RandStream(10,4,1000), &err), err.c_str()); }
  { KDIncFrontierT<ArrInt<3>,  CmpFast, true,  true,  true > f(3); BCHECK(RunDiff(&f, RandStream(11,3,1000), &err), err.c_str()); }
  return 0;
}
