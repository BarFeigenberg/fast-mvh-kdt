// test/gen_and_capture.cpp
// Generate a multi-cost grid graph, run EMOA* with op-capture enabled, and
// report calibration statistics of the captured (busiest-vertex) stream.
// usage: gen_and_capture <rows> <cols> <M> <vo> <vd> <out.stream> [time_limit_sec]
#include "bench/graph_gen.hpp"
#include "bench/synth_gen.hpp"   // MeasureStats / StreamStats
#include "bench/op_stream.hpp"
#include "search_emoa.hpp"
#include <cstdlib>
#include <iostream>
using namespace rzq;

int main(int argc, char** argv) {
  if (argc < 7) {
    std::cerr << "usage: gen_and_capture <rows> <cols> <M> <vo> <vd> <out.stream>"
                 " [time_limit_sec]\n";
    return 1;
  }
  int rows = atoi(argv[1]), cols = atoi(argv[2]), M = atoi(argv[3]);
  long vo = atol(argv[4]), vd = atol(argv[5]);
  double time_limit = (argc >= 8) ? atof(argv[7]) : 120.0;
#ifdef _WIN32
  _putenv_s("EMOA_CAPTURE", argv[6]);
#else
  setenv("EMOA_CAPTURE", argv[6], 1);
#endif
  basic::SparseGraph g;
  bench::GenGridGraph(&g, rows, cols, M, 0.5, 123);
  search::EMOAResult res;
  search::RunEMOA(&g, vo, vd, time_limit, &res);

  bench::OpStream s; size_t dim = 0;
  if (bench::ReadStream(argv[6], &s, &dim)) {
    bench::StreamStats st = bench::MeasureStats(s);
    std::cout << "[STATS] dim=" << st.dim << " ops=" << s.size()
              << " surviving_update_frac=" << st.surviving_update_frac
              << " mean_frontier=" << st.mean_frontier << std::endl;
  } else {
    std::cerr << "[STATS] could not read captured stream " << argv[6] << std::endl;
    return 1;
  }
  return 0;
}
