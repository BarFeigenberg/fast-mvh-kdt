// test/test_dom_metric.cpp
#include "bench/dom_metric.hpp"
#include "bench/test_util.hpp"
#include "vec_type.hpp"
using namespace rzq;
int main() {
  bench::ResetDomCmpCount();
  BCHECK(bench::GetDomCmpCount() == 0, "counter should reset to 0");
  std::vector<double> a = {1.0, 2.0}, b = {1.0, 3.0};
  (void) basic::EpsDom(a, b);
  (void) basic::EpsDom(b, a);
  BCHECK(bench::GetDomCmpCount() == 2, "two EpsDom calls counted");
  return 0;
}
