// include/bench/replay_measure.hpp
// DRY replay/timing loop shared by the learning-scheme drivers. Mirrors the
// loop in test/kdinc_tuning.cpp and source/bench/bench_run.cpp: replays an
// op stream against an IFrontier, timing Check/Update separately and counting
// exact dominance comparisons via the production counter.
#ifndef RZQ_BENCH_REPLAY_MEASURE_H_
#define RZQ_BENCH_REPLAY_MEASURE_H_
#include "bench/ifrontier.hpp"
#include "bench/op_stream.hpp"
#include "bench/dom_metric.hpp"
#include <chrono>
#include <cstdio>
namespace rzq { namespace bench {
inline void ReplayMeasure(IFrontier& f, const char* label, const OpStream& s) {
  ResetDomCmpCount();
  double tc = 0, tu = 0; long long cc = 0, cu = 0; size_t checks = 0, updates = 0;
  using clk = std::chrono::high_resolution_clock;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i].type == OP_CHECK) {
      long long c0 = GetDomCmpCount();
      auto a = clk::now(); volatile bool b = f.Check(s[i].vec); (void)b; auto z = clk::now();
      tc += std::chrono::duration<double, std::micro>(z - a).count();
      cc += GetDomCmpCount() - c0; ++checks;
    } else {
      long long c0 = GetDomCmpCount();
      auto a = clk::now(); f.Update(s[i].vec); auto z = clk::now();
      tu += std::chrono::duration<double, std::micro>(z - a).count();
      cu += GetDomCmpCount() - c0; ++updates;
    }
  }
  double cpc = checks ? (double)cc / checks : 0.0;
  std::printf("%-18s t_check=%11.1f t_update=%10.1f total=%11.1f cmp/check=%7.2f size=%zu\n",
              label, tc, tu, tc + tu, cpc, f.Size());
}
}}
#endif
