// test/run_emoa_dimacs.cpp
// End-to-end EMOA* on a multi-objective DIMACS (.gr) instance, AVL frontier vs
// incremental k-d tree. Loads M cost files (objectives c1..cM in order) with the
// repo's LoadSparseGraphDIMAC, runs both backends, verifies their Pareto fronts
// are identical, and -- if a reference front is given -- checks the computed
// front against it. Built for the CRL-Technion Multi-Objective-Search-Benchmarks
// (e.g. panda robot-motion instances: 8 real objectives, thousands of solutions).
//
// usage: run_emoa_dimacs <vo> <vd> <time_limit> <ref.txt|-> <c1.gr> [c2.gr ...]
#include "graph_io.hpp"
#include "search_emoa.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <set>
#include <string>
#include <vector>
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
static bool LoadRef(const std::string& path, std::set<std::vector<double> >* out) {
  std::ifstream f(path.c_str());
  if (!f) return false;
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream ss(line); std::vector<double> v; double x;
    while (ss >> x) v.push_back(x);
    if (!v.empty()) out->insert(v);
  }
  return true;
}
static search::EMOAResult RunOnce(basic::SparseGraph* g, long vo, long vd, double tl, const char* be) {
  SetFrontier(be);
  search::EMOAResult res;
  search::RunEMOA(g, vo, vd, tl, &res);
  SetFrontier(NULL);
  return res;
}

int main(int argc, char** argv) {
  if (argc < 6) {
    std::cerr << "usage: run_emoa_dimacs <vo> <vd> <time_limit> <ref.txt|-> <c1.gr> [c2.gr ...]\n";
    return 1;
  }
  long vo = atol(argv[1]), vd = atol(argv[2]);
  double tl = atof(argv[3]);
  std::string ref = argv[4];
  std::vector<std::string> grs;
  for (int i = 5; i < argc; ++i) grs.push_back(argv[i]);
  int M = (int)grs.size();

  basic::SparseGraph g;
  if (basic::LoadSparseGraphDIMAC(grs, &g) < 0) { std::cerr << "load fail\n"; return 1; }
  std::cout << "[DIMACS] M=" << M << " |V|=" << g.NumVertex() << " |A|=" << g.NumArc()
            << " vo=" << vo << " vd=" << vd << " tl=" << tl << "s\n";

  search::EMOAResult a = RunOnce(&g, vo, vd, tl, NULL);
  search::EMOAResult k = RunOnce(&g, vo, vd, tl, "kdinc");
  std::set<std::vector<double> > sa = CostSet(a), sk = CostSet(k);
  bool same = (sa == sk) && !a.timeout && !k.timeout;

  std::cout << std::fixed << std::setprecision(4);
  std::cout << "\n[RESULT]            avl(baseline)        kdinc\n";
  std::cout << "  solutions       " << std::setw(12) << a.costs.size() << "      " << std::setw(12) << k.costs.size() << "\n";
  std::cout << "  rt_search(s)    " << std::setw(12) << a.rt_search   << "      " << std::setw(12) << k.rt_search   << "\n";
  std::cout << "  n_domCheck      " << std::setw(12) << a.n_domCheck  << "      " << std::setw(12) << k.n_domCheck  << "\n";
  std::cout << "  timeout         " << std::setw(12) << a.timeout     << "      " << std::setw(12) << k.timeout     << "\n";
  if (k.rt_search > 0.0) std::cout << "  speedup (avl/kdinc) = " << a.rt_search / k.rt_search << "x\n";
  std::cout << "  Pareto sets identical (avl vs kdinc): " << (sa == sk ? "YES" : "NO") << "\n";

  if (ref != "-") {
    std::set<std::vector<double> > sr;
    if (LoadRef(ref, &sr)) {
      bool match = (sa == sr);
      std::cout << "  reference front: " << sr.size() << " solutions, matches computed: "
                << (match ? "YES" : "NO") << "\n";
      if (!match) same = false;
    } else std::cout << "  reference front: could not read " << ref << "\n";
  }

  if (!same) { std::cerr << "[FAIL]\n"; return 1; }
  std::cout << "[OK]\n";
  return 0;
}
