// test/test_cost_store.cpp
#include "bench/cost_store.hpp"
#include "bench/dom_cmp.hpp"
#include "bench/test_util.hpp"
using namespace rzq::bench;
int main() {
  CostVec g = {1.0, 2.0, 3.0};
  VecDouble::Point pv = VecDouble::convert(g);
  BCHECK(pv.size() == 3 && pv[2] == 3.0, "vec convert");
  ArrDouble<3>::Point pa = ArrDouble<3>::convert(g);
  BCHECK(pa[0] == 1.0 && pa[2] == 3.0, "arr double convert");
  ArrInt<3>::Point pi = ArrInt<3>::convert(g);
  BCHECK(pi[1] == 2 && pi[2] == 3, "arr int convert");
  // policies compose with Cmp
  ArrDouble<3>::Point qa = ArrDouble<3>::convert(CostVec{1.0, 2.0, 9.0});
  BCHECK(CmpFast::dominates(pa, qa, 3) == true, "arr dominance");
  return 0;
}
