// test/lowerbound_construction.cpp
// Empirical backing for the separation theorem. Builds an n=m^2 point antichain in d=3
// near the plane x+y+z=2m+1, then issues identical Check queries just outside the
// dominance cone, and measures dom comparisons/Check for a total-order index (avl) vs a
// geometric index (kd). Asserts the total-order index scales ~linearly in n while the
// geometric index scales clearly slower. HONEST: prints all measured numbers.
#include "bench/bench_run.hpp"
#include "bench/op_stream.hpp"
#include <iostream>
#include <vector>
using namespace rzq::bench;

static OpStream BuildConstruction(size_t m, size_t n_checks) {
  OpStream s;
  for (size_t j = 1; j <= m; ++j)
    for (size_t k = 1; k <= m; ++k) {
      Op u; u.type = OP_UPDATE;
      u.vec = { (double)j, (double)k, (double)(2*m - j - k + 1) };
      s.push_back(u);
    }
  for (size_t q = 0; q < n_checks; ++q) {
    Op c; c.type = OP_CHECK; c.vec = { (double)m, (double)m, 0.0 };
    s.push_back(c);
  }
  return s;
}

int main() {
  std::cout << "m,n,avl_cmp_per_check,kd_cmp_per_check\n";
  double avl_prev = 0; size_t n_prev = 0;
  bool ok = true;
  for (size_t m : {16, 32, 64, 128}) {   // n = 256, 1024, 4096, 16384
    size_t n = m * m, nc = 50;
    OpStream s = BuildConstruction(m, nc);
    BenchResult a = RunBackend("avl", s, 3);
    BenchResult k = RunBackend("kd",  s, 3);
    double avl = (double)a.dom_cmps_check / a.checks;
    double kd  = (double)k.dom_cmps_check / k.checks;
    std::cout << m << "," << n << "," << avl << "," << kd << "\n";
    if (n_prev) {
      // 4x more points: total-order comparisons should ~quadruple (linear in n).
      double avl_ratio = avl / avl_prev;
      if (!(avl_ratio > 3.0)) { std::cerr << "FAIL: avl not ~linear (ratio "<<avl_ratio<<")\n"; ok = false; }
    }
    // Separation: the geometric index does a tiny fraction of the total-order work
    // (on this construction it prunes the whole tree at the root, so kd==0). An
    // absolute-fraction test certifies the separation without a 0/0 ratio degeneracy.
    if (!(kd < avl * 0.25)) { std::cerr << "FAIL: kd not clearly below avl (kd "<<kd<<" avl "<<avl<<")\n"; ok = false; }
    avl_prev = avl; n_prev = n;
  }
  if (!ok) { std::cout << "[CONCERN] separation NOT cleanly observed -- see numbers above\n"; return 1; }
  std::cout << "[OK] separation construction validated\n";
  return 0;
}
