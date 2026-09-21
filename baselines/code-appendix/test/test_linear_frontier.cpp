// test/test_linear_frontier.cpp
#include "bench/linear_frontier.hpp"
#include "bench/test_util.hpp"
using namespace rzq::bench;
int main() {
  LinearFrontier f;
  f.Update({2.0, 2.0});
  BCHECK(f.Size() == 1, "size 1 after first insert");
  BCHECK(f.Check({3.0, 3.0}) == true, "(3,3) dominated by (2,2)");
  BCHECK(f.Check({1.0, 3.0}) == false, "(1,3) not dominated by (2,2)");
  // (1,1) dominates (2,2): inserting it must evict (2,2).
  f.Update({1.0, 1.0});
  BCHECK(f.Size() == 1, "(1,1) evicts (2,2)");
  BCHECK(f.Check({2.0, 2.0}) == true, "(2,2) now dominated by (1,1)");
  // incomparable points coexist
  f.Update({0.0, 5.0});
  BCHECK(f.Size() == 2, "incomparable point coexists");
  return 0;
}
