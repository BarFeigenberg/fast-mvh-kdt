// test/kdinc_tuning.cpp
// Ablate the two kdinc query-tuning flags (WidestAxis split, OrderedDescent
// query) against the captured replay streams. Uses VecDouble + CmpNaive to
// mirror the concrete headline KDIncFrontier (EpsDom over std::vector<double>),
// so the measured deltas transfer to the production frontier.
// usage: kdinc_tuning <stream_file>
#include "bench/kd_inc_frontier_t.hpp"
#include "bench/op_stream.hpp"
#include "bench/dom_metric.hpp"
#include <chrono>
#include <iostream>
#include <string>
using namespace rzq::bench;

template<bool WA, bool OD>
static void Run(const char* label, const OpStream& s, size_t dim) {
  KDIncFrontierT<VecDouble, CmpNaive, false, WA, OD> f(dim);
  ResetDomCmpCount();
  double t_check = 0, t_update = 0;
  long long cc = 0, cu = 0;
  size_t checks = 0, updates = 0;
  using clk = std::chrono::high_resolution_clock;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i].type == OP_CHECK) {
      long long c0 = GetDomCmpCount();
      auto a = clk::now(); volatile bool b = f.Check(s[i].vec); (void)b; auto z = clk::now();
      t_check += std::chrono::duration<double,std::micro>(z-a).count();
      cc += GetDomCmpCount() - c0; ++checks;
    } else {
      long long c0 = GetDomCmpCount();
      auto a = clk::now(); f.Update(s[i].vec); auto z = clk::now();
      t_update += std::chrono::duration<double,std::micro>(z-a).count();
      cu += GetDomCmpCount() - c0; ++updates;
    }
  }
  double cpc = checks ? (double)cc / checks : 0.0;
  printf("%-14s t_check=%11.1f t_update=%9.1f total=%11.1f cmp/check=%6.2f size=%zu\n",
         label, t_check, t_update, t_check + t_update, cpc, f.Size());
}

int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: kdinc_tuning <stream_file>\n"; return 1; }
  OpStream s; size_t dim = 0;
  if (!ReadStream(argv[1], &s, &dim)) { std::cerr << "read fail\n"; return 1; }
  std::cout << "stream=" << argv[1] << " dim=" << dim << " ops=" << s.size() << "\n";
  Run<false,false>("base",          s, dim);
  Run<true, false>("+widest",       s, dim);
  Run<false,true >("+ordered",      s, dim);
  Run<true, true >("+widest+ord",   s, dim);
  return 0;
}
