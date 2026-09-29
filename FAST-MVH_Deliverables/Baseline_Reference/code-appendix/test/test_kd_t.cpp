// test/test_kd_t.cpp
#include "bench/kd_frontier_t.hpp"
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
  { KDFrontierT<VecDouble, CmpFast, false> f(3); BCHECK(RunDiff(&f, RandStream(1,3,600), &err), err.c_str()); }
  { KDFrontierT<ArrDouble<3>, CmpFast, true> f(3); BCHECK(RunDiff(&f, RandStream(2,3,600), &err), err.c_str()); }
  { KDFrontierT<ArrInt<3>, CmpBranchless, true> f(3); BCHECK(RunDiff(&f, RandStream(3,3,600), &err), err.c_str()); }
  return 0;
}
