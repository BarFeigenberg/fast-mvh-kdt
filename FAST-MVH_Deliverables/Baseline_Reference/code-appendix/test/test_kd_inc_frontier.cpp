// test/test_kd_inc_frontier.cpp
// Differential test for the concrete (EpsDom / std::vector<double>) incremental
// k-d tree used in the headline comparison.
#include "bench/kd_inc_frontier.hpp"
#include "bench/diff_harness.hpp"
#include "bench/test_util.hpp"
#include <cstdlib>
using namespace rzq::bench;
static OpStream RandStream(unsigned seed, size_t dim, int n) {
  std::srand(seed);
  OpStream s;
  for (int i = 0; i < n; ++i) {
    Op op; op.type = (std::rand() % 3 == 0) ? OP_UPDATE : OP_CHECK;
    for (size_t d = 0; d < dim; ++d) { op.vec.push_back(std::rand() % 20); }
    s.push_back(op);
  }
  return s;
}
int main() {
  for (size_t dim = 1; dim <= 5; ++dim) {
    KDIncFrontier f(dim);
    std::string err;
    BCHECK(RunDiff(&f, RandStream(42 + dim, dim, 1200), &err), err.c_str());
  }
  return 0;
}
