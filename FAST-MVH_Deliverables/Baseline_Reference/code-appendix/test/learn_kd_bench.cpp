// test/learn_kd_bench.cpp
// Phase-1 replay triage: replay one captured stream through baseline kdinc and
// the four learning schemes, printing cmp/check + timings for each. Uses the
// concrete KDIncFrontier (EpsDom over vector<double>) as the baseline so counts
// are directly comparable to the report's kdinc figures.
// usage: learn_kd_bench <stream_file>
#include "bench/kd_inc_frontier.hpp"
#include "bench/kdinc_dc_frontier.hpp"
#include "bench/kdinc_vo_frontier.hpp"
#include "bench/kdinc_qa_frontier.hpp"
#include "bench/hybrid_frontier.hpp"
#include "bench/replay_measure.hpp"
#include "bench/op_stream.hpp"
#include <iostream>
using namespace rzq::bench;

int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: learn_kd_bench <stream_file>\n"; return 1; }
  OpStream s; size_t dim = 0;
  if (!ReadStream(argv[1], &s, &dim)) { std::cerr << "read fail\n"; return 1; }
  std::cout << "stream=" << argv[1] << " dim=" << dim << " ops=" << s.size() << "\n";
  { KDIncFrontier   f(dim); ReplayMeasure(f, "baseline kdinc", s); }
  { KDIncQAFrontier f(dim); ReplayMeasure(f, "S1 query-aware", s); }
  { KDIncVOFrontier f(dim); ReplayMeasure(f, "S3 visit-order", s); }
  { KDIncDCFrontier<1> f(dim); ReplayMeasure(f, "S4 cache K=1", s); }
  { KDIncDCFrontier<2> f(dim); ReplayMeasure(f, "S4 cache K=2", s); }
  { KDIncDCFrontier<4> f(dim); ReplayMeasure(f, "S4 cache K=4", s); }
  { KDIncDCFrontier<8> f(dim); ReplayMeasure(f, "S4 cache K=8", s); }
  { HybridFrontier<32> f(dim); ReplayMeasure(f, "S2 hybrid t=32", s); }
  return 0;
}
