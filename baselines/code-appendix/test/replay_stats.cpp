// test/replay_stats.cpp
// Replays a captured op stream K times per backend and reports wall-clock mean with a 95%
// confidence interval (Student-t), plus exact (deterministic) dominance-comparison counts.
// usage: replay_stats <stream_file> <K>
#include "bench/bench_run.hpp"
#include "bench/op_stream.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
using namespace rzq::bench;

// 95% two-sided Student-t critical values for small dof (index = dof).
static double tcrit(int dof) {
  static const double t[] = {0,12.706,4.303,3.182,2.776,2.571,2.447,2.365,2.306,2.262,2.228,
                             2.201,2.179,2.160,2.145,2.131,2.120,2.110,2.101,2.093,2.086};
  if (dof < 1) return 0; if (dof <= 20) return t[dof]; return 1.96;
}
int main(int argc, char** argv) {
  if (argc < 3) { std::cerr << "usage: replay_stats <stream> <K>\n"; return 1; }
  OpStream s; size_t dim = 0;
  if (!ReadStream(argv[1], &s, &dim)) { std::cerr << "read fail: " << argv[1] << "\n"; return 1; }
  int K = atoi(argv[2]);
  std::cout << "backend,dim,checks,updates,t_total_mean_us,t_total_ci95_us,dom_cmps_check,dom_cmps_update,final_frontier\n";
  const char* names[] = {"linear","sortlinear","avlfast","kdinc","ndinc"};
  for (const char* nm : names) {
    std::vector<double> totals;
    long long cc = 0, cu = 0; size_t checks = 0, updates = 0, final_size = 0;
    for (int r = 0; r < K; ++r) {
      BenchResult br = RunBackend(nm, s, dim);
      totals.push_back(br.t_check_us + br.t_update_us);
      cc = br.dom_cmps_check; cu = br.dom_cmps_update; checks = br.checks; updates = br.updates;
      final_size = br.final_size;
    }
    double mean = 0; for (double x : totals) mean += x; mean /= K;
    double var = 0; for (double x : totals) var += (x-mean)*(x-mean);
    var = (K > 1) ? var/(K-1) : 0;
    double ci = (K > 1) ? tcrit(K-1) * std::sqrt(var) / std::sqrt((double)K) : 0;
    std::cout << nm << "," << dim << "," << checks << "," << updates << ","
              << mean << "," << ci << "," << cc << "," << cu << "," << final_size << "\n";
  }
  return 0;
}
