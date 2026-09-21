// test/ablation_frontier.cpp
#include "bench/bench_run.hpp"
#include "bench/variant_factory.hpp"
#include "bench/op_stream.hpp"
#include <iostream>
#include <string>
using namespace rzq::bench;
// usage: ablation_frontier <stream_file>
// emits one CSV row per (backend, variant) from AllVariants, skipping invalid combos.
int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: ablation_frontier <stream_file>\n"; return 1; }
  OpStream s; size_t dim = 0;
  if (!ReadStream(argv[1], &s, &dim)) { std::cerr << "read fail\n"; return 1; }
  std::cout << "backend,store,cmp,arena,checks,updates,t_check_us,t_update_us,"
               "dom_cmps_check,dom_cmps_update,final_size\n";
  const char* backends[] = {"linear","avlfast","kd","nd","range"};
  for (int b = 0; b < 5; ++b) {
    std::string name = backends[b];
    std::vector<VariantCfg> cfgs = AllVariants(name, dim);
    for (size_t i = 0; i < cfgs.size(); ++i) {
      IFrontier* probe = MakeVariant(name, dim, cfgs[i]);
      if (!probe) continue; // skip invalid combos
      delete probe;
      BenchResult r = RunVariant(name, cfgs[i], s, dim);
      std::cout << name << "," << StoreName(cfgs[i].store) << "," << CmpName(cfgs[i].cmp)
                << "," << (cfgs[i].arena ? 1 : 0) << "," << r.checks << "," << r.updates << ","
                << r.t_check_us << "," << r.t_update_us << "," << r.dom_cmps_check << ","
                << r.dom_cmps_update << "," << r.final_size << std::endl;
    }
  }
  return 0;
}
