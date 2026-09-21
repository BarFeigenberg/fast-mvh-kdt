// test/test_bench_smoke.cpp
#include "bench/bench_run.hpp"
#include "bench/synth_gen.hpp"
#include "bench/test_util.hpp"
using namespace rzq::bench;
int main() {
  SynthParams p; p.dim=3; p.n_ops=2000; p.target_frontier=20; p.corr=0.5; p.seed=1;
  OpStream s = GenSynthStream(p);
  const char* names[] = {"linear","avl","avlfast","kd","nd","range"};
  for (int i = 0; i < 6; ++i) {
    BenchResult r = RunBackend(names[i], s, p.dim);
    BCHECK(r.checks + r.updates > 0, "ops counted");
    BCHECK(r.dom_cmps_check + r.dom_cmps_update >= 0, "dom cmp recorded");
  }
  return 0;
}
