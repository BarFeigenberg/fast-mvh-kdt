// test/avg_sweep_driver.cpp
// Average-case validation sweep: for each (dim, corr, target, seed) grid point, build a
// synthetic stream and record exact dom comparisons/Check and final frontier size for three
// fast backends. CSV to stdout. Deterministic (explicit seeds). The full-rebuild "kd" backend
// is intentionally omitted: its per-Check comparison count is identical to "kdinc" (same query
// traversal), but its O(n log n) per-update rebuild makes the sweep intractable at high dim.
#include "bench/bench_run.hpp"
#include "bench/synth_gen.hpp"
#include <iostream>
using namespace rzq::bench;
int main() {
  std::cout << "dim,corr,target,seed,backend,checks,dom_cmps_check,final_size\n";
  const char* backends[] = {"linear","avlfast","kdinc"};
  for (size_t dim : {2,3,4,5})
    for (double corr : {0.0,0.3,0.5,0.8})
      for (size_t target : {20,60,150,400})
        for (unsigned seed : {1u,2u,3u}) {
          SynthParams p; p.dim=dim; p.n_ops=20000; p.target_frontier=target; p.corr=corr; p.seed=seed;
          OpStream s = GenSynthStream(p);
          for (const char* b : backends) {
            BenchResult r = RunBackend(b, s, dim);
            std::cout << dim<<","<<corr<<","<<target<<","<<seed<<","<<b<<","
                      << r.checks<<","<<r.dom_cmps_check<<","<<r.final_size<<"\n";
          }
        }
  return 0;
}
