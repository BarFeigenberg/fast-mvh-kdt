// test/test_learn_kd.cpp
// Differential test: every learning scheme must agree with the linear oracle
// (RunDiff) on random streams across dim 1..5. Soundness is the hard gate.
#include "bench/kdinc_dc_frontier.hpp"
#include "bench/kdinc_vo_frontier.hpp"
#include "bench/kdinc_qa_frontier.hpp"
#include "bench/hybrid_frontier.hpp"
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
  for (size_t dim = 1; dim <= 5; ++dim) {
    for (int t = 0; t < 3; ++t) {
      OpStream s = RandStream(100 + (unsigned)(dim * 10 + t), dim, 500);
      std::string e;
      { KDIncDCFrontier<1> f(dim); BCHECK(RunDiff(&f, s, &e), e.c_str()); }
      { KDIncDCFrontier<4> f(dim); BCHECK(RunDiff(&f, s, &e), e.c_str()); }
      { KDIncDCFrontier<64> f(dim); BCHECK(RunDiff(&f, s, &e), e.c_str()); }
      { KDIncVOFrontier f(dim); BCHECK(RunDiff(&f, s, &e), e.c_str()); }
      { KDIncQAFrontier f(dim); BCHECK(RunDiff(&f, s, &e), e.c_str()); }
      { HybridFrontier<8> f(dim); BCHECK(RunDiff(&f, s, &e), e.c_str()); }
    }
  }
  return 0;
}
