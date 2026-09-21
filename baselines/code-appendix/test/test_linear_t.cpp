// test/test_linear_t.cpp
#include "bench/linear_frontier_t.hpp"
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
  // dim=3 fixed for the array stores; vector store also tested.
  { LinearFrontierT<VecDouble, CmpNaive> f; BCHECK(RunDiff(&f, RandStream(1,3,500), &err), err.c_str()); }
  { LinearFrontierT<VecDouble, CmpFast> f; BCHECK(RunDiff(&f, RandStream(2,3,500), &err), err.c_str()); }
  { LinearFrontierT<VecDouble, CmpBranchless> f; BCHECK(RunDiff(&f, RandStream(3,3,500), &err), err.c_str()); }
  { LinearFrontierT<ArrDouble<3>, CmpFast> f; BCHECK(RunDiff(&f, RandStream(4,3,500), &err), err.c_str()); }
  { LinearFrontierT<ArrInt<3>, CmpBranchless> f; BCHECK(RunDiff(&f, RandStream(5,3,500), &err), err.c_str()); }
  return 0;
}
