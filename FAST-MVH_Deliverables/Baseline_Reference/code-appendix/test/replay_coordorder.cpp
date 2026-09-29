// test/replay_coordorder.cpp
// Coordinate-order sensitivity study (vector-level dominance-check counts).
// Replays a captured op stream once through each of five backends and reports the
// exact, deterministic dominance-check counts (g_eps_dom_count), split into
// check vs update. Counts are compiler/optimization-independent, so one pass
// suffices. Built without COORD_CMP_COUNT, so it runs at -O2.
//
// Backends: linear (unsorted scan, does not use coordinate order -> invariant),
// sortlinear (lex-sorted scan with early termination -> uses coordinate order),
// avlfast (lexicographic AVL), kdinc (incremental k-d), ndinc (incremental ND-tree).
//
// usage: replay_coordorder <stream_file>
#include "bench/op_stream.hpp"
#include "bench/dom_metric.hpp"
#include "bench/ifrontier.hpp"
#include "bench/linear_frontier.hpp"
#include "bench/sorted_linear_frontier.hpp"
#include "bench/avlfast_frontier.hpp"
#include "bench/kd_inc_frontier.hpp"
#include "bench/nd_inc_frontier.hpp"
#include <iostream>
#include <memory>
#include <string>
using namespace rzq::bench;

static IFrontier* Make(const std::string& n, size_t dim) {
  if (n == "linear")     return new LinearFrontier();
  if (n == "sortlinear") return new SortedLinearFrontier();
  if (n == "avlfast")    return new AVLFastFrontier();
  if (n == "kdinc")      return new KDIncFrontier(dim);
  if (n == "ndinc")      return new NDIncFrontier(dim);
  return NULL;
}

int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: replay_coordorder <stream>\n"; return 1; }
  OpStream s; size_t dim = 0;
  if (!ReadStream(argv[1], &s, &dim)) { std::cerr << "read fail: " << argv[1] << "\n"; return 1; }

  std::cout << "backend,dim,checks,updates,dom_cmps_check,dom_cmps_update,final_frontier\n";
  const char* names[] = {"linear", "sortlinear", "avlfast", "kdinc", "ndinc"};
  for (const char* nm : names) {
    std::unique_ptr<IFrontier> f(Make(nm, dim));
    long long dc = 0, du = 0; size_t checks = 0, updates = 0;
    for (size_t i = 0; i < s.size(); ++i) {
      if (s[i].type == OP_CHECK) {
        long long d0 = GetDomCmpCount();
        volatile bool b = f->Check(s[i].vec); (void)b;
        dc += GetDomCmpCount() - d0; ++checks;
      } else {
        long long d0 = GetDomCmpCount();
        f->Update(s[i].vec);
        du += GetDomCmpCount() - d0; ++updates;
      }
    }
    std::cout << nm << "," << dim << "," << checks << "," << updates << ","
              << dc << "," << du << "," << f->Size() << "\n";
  }
  return 0;
}
