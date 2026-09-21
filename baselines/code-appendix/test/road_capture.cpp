// test/road_capture.cpp
// Road-graph analogue of gen_and_capture: build a real DIMACS/PACE road graph
// with a chosen cost model, run EMOA* with op-capture enabled (EMOA_CAPTURE),
// and report calibration statistics of the captured (busiest-vertex) stream.
//
// usage:
//   road_capture <net> <M> <vo> <vd> <out.stream> <mode:geo|synth> <rho> <seed> <time_limit_s>
//
//   mode=geo   -> landmark-field GEOMETRIC costs (LoadPACEGeo); lower rho => bigger front
//   mode=synth -> reproducible LCG grid cost model per arc (LoadRoad synth)
#include "graph.hpp"
#include "search_emoa.hpp"
#include "road_costs.hpp"          // LoadPACEGeo / LoadRoad (DRY, shared with run_emoa_road)
#include "bench/op_stream.hpp"
#include "bench/synth_gen.hpp"     // MeasureStats / StreamStats
#include <cstdlib>
#include <string>
#include <iostream>
using namespace rzq;

// basename: strip directory (handles both / and \) from a path.
static std::string BaseName(const std::string& p) {
  size_t s = p.find_last_of("/\\");
  return (s == std::string::npos) ? p : p.substr(s + 1);
}

int main(int argc, char** argv) {
  if (argc < 10) {
    std::cerr << "usage: road_capture <net> <M> <vo> <vd> <out.stream>"
                 " <mode:geo|synth> <rho> <seed> <time_limit_s>\n";
    return 1;
  }
  std::string net = argv[1];
  int M = atoi(argv[2]);
  long vo = atol(argv[3]), vd = atol(argv[4]);
  std::string out = argv[5];
  std::string mode = argv[6];
  double rho = atof(argv[7]);
  unsigned seed = (unsigned)strtoul(argv[8], 0, 10);
  double time_limit = atof(argv[9]);

#ifdef _WIN32
  _putenv_s("EMOA_CAPTURE", out.c_str());
#else
  setenv("EMOA_CAPTURE", out.c_str(), 1);
#endif

  basic::SparseGraph g; long maxNode = 0;
  bool loaded = (mode == "geo")
      ? LoadPACEGeo(net, M, rho, seed, &g, &maxNode)                 // landmark-field geometric costs
      : LoadRoad(net, M, /*synth=*/true, rho, seed, &g, &maxNode);   // reproducible synth grid costs
  if (!loaded) { std::cerr << "[STATS] load failed for " << net << "\n"; return 1; }

  search::EMOAResult res;
  search::RunEMOA(&g, vo, vd, time_limit, &res);

  bench::OpStream s; size_t dim = 0;
  if (!bench::ReadStream(out.c_str(), &s, &dim)) {
    std::cerr << "[STATS] could not read captured stream " << out << std::endl;
    return 1;
  }
  bench::StreamStats st = bench::MeasureStats(s);
  std::cout << "[STATS] map=" << BaseName(net) << " M=" << M << " rho=" << rho
            << " mode=" << mode << " ops=" << s.size()
            << " mean_frontier=" << st.mean_frontier
            << " solutions=" << res.paths.size() << std::endl;
  return 0;
}
