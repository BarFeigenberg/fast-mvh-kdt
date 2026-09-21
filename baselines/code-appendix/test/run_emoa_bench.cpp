// test/run_emoa_bench.cpp
// End-to-end EMOA* search time with the AVL frontier (baseline) vs. the
// incremental k-d tree frontier (EMOA_FRONTIER=kdinc), on the same generated
// grid graph used to capture the replay streams. Verifies that both backends
// return identical Pareto-optimal cost sets, then reports search time and the
// dominance-check count for each.
//
// usage: run_emoa_bench <rows> <cols> <M> <vo> <vd> [time_limit_sec]
#include "bench/graph_gen.hpp"
#include "search_emoa.hpp"
#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>
#include <iomanip>
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
       it != r.costs.end(); ++it) {
    s.insert(it->second);
  }
  return s;
}

int main(int argc, char** argv) {
  if (argc < 6) {
    std::cerr << "usage: run_emoa_bench <rows> <cols> <M> <vo> <vd> [time_limit_sec]\n";
    return 1;
  }
  int rows = atoi(argv[1]), cols = atoi(argv[2]), M = atoi(argv[3]);
  long vo = atol(argv[4]), vd = atol(argv[5]);
  double tl = (argc >= 7) ? atof(argv[6]) : 120.0;

  basic::SparseGraph g;
  bench::GenGridGraph(&g, rows, cols, M, 0.5, 123);
  std::cout << "[BENCH] grid " << rows << "x" << cols << " M=" << M
            << " vo=" << vo << " vd=" << vd << " time_limit=" << tl << "s\n";

  SetFrontier(NULL); // AVL baseline
  search::EMOAResult res_avl;
  search::RunEMOA(&g, vo, vd, tl, &res_avl);

  SetFrontier("kdinc"); // incremental k-d tree
  search::EMOAResult res_kd;
  search::RunEMOA(&g, vo, vd, tl, &res_kd);
  SetFrontier(NULL);

  std::set<std::vector<double> > s_avl = CostSet(res_avl), s_kd = CostSet(res_kd);
  bool same = (s_avl == s_kd) && !res_avl.timeout && !res_kd.timeout;

  std::cout << std::fixed << std::setprecision(4);
  std::cout << "\n[RESULT]            avl(baseline)        kdinc\n";
  std::cout << "  solutions       " << std::setw(12) << res_avl.costs.size()
            << "      " << std::setw(12) << res_kd.costs.size() << "\n";
  std::cout << "  rt_search(s)    " << std::setw(12) << res_avl.rt_search
            << "      " << std::setw(12) << res_kd.rt_search << "\n";
  std::cout << "  n_domCheck      " << std::setw(12) << res_avl.n_domCheck
            << "      " << std::setw(12) << res_kd.n_domCheck << "\n";
  std::cout << "  timeout         " << std::setw(12) << res_avl.timeout
            << "      " << std::setw(12) << res_kd.timeout << "\n";
  if (res_kd.rt_search > 0.0)
    std::cout << "  speedup (avl/kdinc rt_search) = "
              << res_avl.rt_search / res_kd.rt_search << "x\n";
  std::cout << "  Pareto sets identical: " << (s_avl == s_kd ? "YES" : "NO") << "\n";

  if (!same) {
    std::cerr << "[FAIL] backends disagree or timed out (|avl|=" << s_avl.size()
              << " |kdinc|=" << s_kd.size() << ")\n";
    return 1;
  }
  std::cout << "[OK] identical Pareto fronts.\n";
  return 0;
}
