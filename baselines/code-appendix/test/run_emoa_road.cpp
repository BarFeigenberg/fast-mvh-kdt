// test/run_emoa_road.cpp
// End-to-end EMOA* on a REAL road network (TNTP format, e.g. the
// TransportationNetworks instances), AVL frontier vs incremental k-d tree.
// Loads a directed multi-attribute road graph and runs the same A/B comparison
// as run_emoa_bench: verify identical Pareto fronts, then report search time
// and dominance-check counts.
//
// Costs (first M of): c0 length, c1 free-flow time (x1000, int), c2 congestion
// (length*1e5/capacity, int), c3 hop count (1). M>=4 gives projected d>=3, the
// regime where the lexicographic AVL loses its pruning.
//
// usage: run_emoa_road <net.tntp> <M> <vo> <vd> [time_limit_sec] [real|synth]
//
// Networks (TNTP) from the TransportationNetworks repository, e.g.:
//   raw.githubusercontent.com/bstabler/TransportationNetworks/master/Philadelphia/Philadelphia_net.tntp
//   raw.githubusercontent.com/bstabler/TransportationNetworks/master/Chicago-Sketch/ChicagoSketch_net.tntp
#include "graph.hpp"
#include "search_emoa.hpp"
#include "road_costs.hpp"   // GeoBfs/GeoGauss/LoadPACEGeo/LoadTNTP/LoadPACE/LoadRoad (DRY)
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <set>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include <utility>
using namespace rzq;

static void SetFrontier(const char* val) {
#ifdef _WIN32
  _putenv_s("EMOA_FRONTIER", val ? val : "");
#else
  if (val) setenv("EMOA_FRONTIER", val, 1); else unsetenv("EMOA_FRONTIER");
#endif
}
static std::set<std::vector<double> > CostSet(const search::EMOAResult& r) {
  std::set<std::vector<double> > s;
  for (std::unordered_map<long, search::CostVec>::const_iterator it = r.costs.begin();
       it != r.costs.end(); ++it) s.insert(it->second);
  return s;
}

static search::EMOAResult RunOnce(basic::SparseGraph* g, long vo, long vd, double tl, const char* be) {
  SetFrontier(be);
  search::EMOAResult res;
  search::RunEMOA(g, vo, vd, tl, &res);
  SetFrontier(NULL);
  return res;
}

int main(int argc, char** argv) {
  if (argc < 5) {
    std::cerr << "usage: run_emoa_road <net> <M> <vo> <vd> [time_limit_sec] [real|synth|geo] [corr/rho] [seed]\n"
                 "  geo: landmark-field geometric costs on a PACE topology (rho in [0,1], lower => bigger front)\n"
                 "  seed: cost-model seed (default 123; vary for sensitivity studies)\n";
    return 1;
  }
  std::string net = argv[1];
  int M = atoi(argv[2]);
  long vo = atol(argv[3]), vd = atol(argv[4]);
  double tl = (argc >= 6) ? atof(argv[5]) : 120.0;
  std::string mode = (argc >= 7) ? std::string(argv[6]) : "synth"; // real|synth|geo
  double corr = (argc >= 8) ? atof(argv[7]) : 0.5; // cost correlation (rho); lower => bigger fronts
  unsigned seed = (argc >= 9) ? (unsigned)strtoul(argv[8], 0, 10) : 123u; // cost-model seed (sensitivity)

  basic::SparseGraph g; long maxNode = 0;
  bool loaded = (mode == "geo")
      ? LoadPACEGeo(net, M, corr, seed, &g, &maxNode)             // landmark-field geometric costs
      : LoadRoad(net, M, mode == "synth", corr, seed, &g, &maxNode); // real attrs / random synth
  if (!loaded) return 1;
  std::cout << "[ROAD] M=" << M << " vo=" << vo << " vd=" << vd << " time_limit=" << tl << "s\n";

  search::EMOAResult a = RunOnce(&g, vo, vd, tl, NULL);     // AVL baseline
  search::EMOAResult k = RunOnce(&g, vo, vd, tl, "kdinc");  // incremental k-d tree

  std::set<std::vector<double> > sa = CostSet(a), sk = CostSet(k);
  bool ok = (sa == sk) && !a.timeout && !k.timeout;
  std::cout << std::fixed << std::setprecision(4);
  std::cout << "\n[RESULT]            avl(baseline)        kdinc\n";
  std::cout << "  solutions       " << std::setw(12) << a.costs.size() << "      " << std::setw(12) << k.costs.size() << "\n";
  std::cout << "  rt_search(s)    " << std::setw(12) << a.rt_search   << "      " << std::setw(12) << k.rt_search   << "\n";
  std::cout << "  n_domCheck      " << std::setw(12) << a.n_domCheck  << "      " << std::setw(12) << k.n_domCheck  << "\n";
  std::cout << "  timeout         " << std::setw(12) << a.timeout     << "      " << std::setw(12) << k.timeout     << "\n";
  if (k.rt_search > 0.0) std::cout << "  speedup (avl/kdinc) = " << a.rt_search / k.rt_search << "x\n";
  std::cout << "  Pareto sets identical: " << (sa == sk ? "YES" : "NO") << "\n";
  if (!ok) { std::cerr << "[FAIL] disagree or timeout (|avl|=" << sa.size() << " |kdinc|=" << sk.size() << ")\n"; return 1; }
  std::cout << "[OK] identical Pareto fronts.\n";
  return 0;
}
