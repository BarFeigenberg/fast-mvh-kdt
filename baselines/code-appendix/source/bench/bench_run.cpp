// source/bench/bench_run.cpp
#include "bench/bench_run.hpp"
#include "bench/dom_metric.hpp"
#include "bench/linear_frontier.hpp"
#include "bench/sorted_linear_frontier.hpp"
#include "bench/avl_frontier.hpp"
#include "bench/avlfast_frontier.hpp"
#include "bench/kd_frontier.hpp"
#include "bench/kd_inc_frontier.hpp"
#include "bench/nd_frontier.hpp"
#include "bench/nd_inc_frontier.hpp"
#include "bench/range_frontier.hpp"
#include "bench/variant_factory.hpp"
#include <chrono>
#include <memory>
namespace rzq { namespace bench {
static IFrontier* Make(const std::string& n, size_t dim) {
  if (n=="linear") return new LinearFrontier();
  if (n=="sortlinear") return new SortedLinearFrontier();
  if (n=="avl")     return new AVLFrontier();
  if (n=="avlfast") return new AVLFastFrontier();
  if (n=="kd")      return new KDFrontier(dim);
  if (n=="kdinc")   return new KDIncFrontier(dim);
  if (n=="nd")     return new NDTreeFrontier(dim);
  if (n=="ndinc")  return new NDIncFrontier(dim);
  if (n=="range")  return new RangeTreeFrontier(dim);
  return NULL;
}
// Timing/counter loop shared by RunBackend and RunVariant.
static BenchResult RunPtr(IFrontier* f, const std::string& name, const OpStream& s, size_t dim) {
  BenchResult r; r.backend=name; r.dim=dim; r.checks=0; r.updates=0;
  r.t_check_us=0; r.t_update_us=0;
  ResetDomCmpCount();
  r.dom_cmps_check = 0; r.dom_cmps_update = 0;
  using clk = std::chrono::high_resolution_clock;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i].type == OP_CHECK) {
      long long c0 = GetDomCmpCount();
      auto a = clk::now(); volatile bool b = f->Check(s[i].vec); (void)b;
      auto z = clk::now();
      r.t_check_us += std::chrono::duration<double,std::micro>(z-a).count();
      r.dom_cmps_check += GetDomCmpCount() - c0;
      ++r.checks;
    } else {
      long long c0 = GetDomCmpCount();
      auto a = clk::now(); f->Update(s[i].vec); auto z = clk::now();
      r.t_update_us += std::chrono::duration<double,std::micro>(z-a).count();
      r.dom_cmps_update += GetDomCmpCount() - c0;
      ++r.updates;
    }
  }
  r.final_size = f->Size();
  return r;
}
BenchResult RunBackend(const std::string& name, const OpStream& s, size_t dim) {
  std::unique_ptr<IFrontier> f(Make(name, dim));
  return RunPtr(f.get(), name, s, dim);
}
BenchResult RunVariant(const std::string& name, const VariantCfg& cfg, const OpStream& s, size_t dim) {
  std::unique_ptr<IFrontier> f(MakeVariant(name, dim, cfg));
  return RunPtr(f.get(), name, s, dim);
}
} }
