// test/test_dom_cmp_simd.cpp
#include "bench/dom_cmp.hpp"
#include "bench/cost_store.hpp"
#include "bench/test_util.hpp"
using namespace rzq::bench;
int main() {
#ifdef __AVX2__
  ArrDouble<4>::Point a = {1,2,3,4}, b = {1,2,3,5}, c = {1,2,3,3};
  BCHECK(CmpSimd::dominates(a, b, 4) == true,  "simd a<=b");
  BCHECK(CmpSimd::dominates(a, c, 4) == false, "simd a!<=c");
  ArrInt<4>::Point ai = {1,2,3,4}, bi = {1,2,3,5};
  BCHECK(CmpSimd::dominates(ai, bi, 4) == true, "simd int a<=b");
#endif
  return 0;
}
