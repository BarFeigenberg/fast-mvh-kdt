// test/test_avlfast_frontier.cpp
#include "bench/avlfast_frontier.hpp"
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
  // A fixed scenario plus random differential across dims 1-5 vs the linear oracle.
  {
    OpStream s;
    s.push_back({OP_UPDATE, {2.0, 2.0}});
    s.push_back({OP_CHECK,  {3.0, 3.0}});
    s.push_back({OP_UPDATE, {1.0, 3.0}});
    s.push_back({OP_UPDATE, {1.0, 1.0}});
    s.push_back({OP_CHECK,  {2.0, 2.0}});
    s.push_back({OP_CHECK,  {0.0, 9.0}});
    AVLFastFrontier f;
    std::string err;
    BCHECK(RunDiff(&f, s, &err), err.c_str());
  }
  for (size_t dim = 1; dim <= 5; ++dim) {
    AVLFastFrontier f;
    std::string err;
    BCHECK(RunDiff(&f, RandStream(99 + dim, dim, 600), &err), err.c_str());
  }
  return 0;
}
