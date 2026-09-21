// test/benchmark_frontier.cpp
#include "bench/bench_run.hpp"
#include "bench/op_stream.hpp"
#include "bench/synth_gen.hpp"
#include <iostream>
#include <string>
#include <cstdlib>
using namespace rzq::bench;
// usage: benchmark_frontier <stream_file | "synth"> [dim n_ops target corr seed]
int main(int argc, char** argv) {
  OpStream s; size_t dim = 0;
  if (argc >= 2 && std::string(argv[1]) == "synth") {
    SynthParams p;
    p.dim = argc>2?(size_t)atoi(argv[2]):3; p.n_ops = argc>3?(size_t)atol(argv[3]):20000;
    p.target_frontier = argc>4?(size_t)atol(argv[4]):40; p.corr = argc>5?atof(argv[5]):0.5;
    p.seed = argc>6?(unsigned)atoi(argv[6]):1;
    s = GenSynthStream(p); dim = p.dim;
  } else if (argc >= 2) {
    if (!ReadStream(argv[1], &s, &dim)) { std::cerr<<"read fail\n"; return 1; }
  } else { std::cerr<<"usage: benchmark_frontier <file|synth> ...\n"; return 1; }

  std::cout << "backend,dim,checks,updates,t_check_us,t_update_us,dom_cmps_check,dom_cmps_update,final_size\n";
  const char* names[] = {"linear","avl","avlfast","kd","kdinc","ndinc","nd","range"};
  for (int i = 0; i < 8; ++i) {
    BenchResult r = RunBackend(names[i], s, dim);
    std::cout << r.backend<<","<<r.dim<<","<<r.checks<<","<<r.updates<<","
              << r.t_check_us<<","<<r.t_update_us<<","<<r.dom_cmps_check<<","
              << r.dom_cmps_update<<","
              << r.final_size << std::endl; // flush per row so long runs show progress
  }
  return 0;
}
