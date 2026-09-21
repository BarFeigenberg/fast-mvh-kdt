// test/test_dom_cmp.cpp
#include "bench/dom_cmp.hpp"
#include "bench/test_util.hpp"
#include <vector>
using namespace rzq::bench;
int main() {
  std::vector<double> a = {1.0, 2.0}, b = {1.0, 3.0}, c = {0.0, 5.0};
  // a dominates b (1<=1, 2<=3); a does NOT dominate c (1>0)
  BCHECK(CmpNaive::dominates(a, b, 2) == true,  "naive a<=b");
  BCHECK(CmpNaive::dominates(a, c, 2) == false, "naive a!<=c");
  BCHECK(CmpFast::dominates(a, b, 2) == true,   "fast a<=b");
  BCHECK(CmpFast::dominates(a, c, 2) == false,  "fast a!<=c");
  BCHECK(CmpBranchless::dominates(a, b, 2) == true,  "bl a<=b");
  BCHECK(CmpBranchless::dominates(a, c, 2) == false, "bl a!<=c");
  // equal vectors weakly dominate
  BCHECK(CmpFast::dominates(a, a, 2) == true, "fast a<=a");
  return 0;
}
