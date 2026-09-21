// test/test_graph_gen.cpp
#include "bench/graph_gen.hpp"
#include "bench/test_util.hpp"
using namespace rzq;
int main() {
  basic::SparseGraph g;
  bench::GenGridGraph(&g, /*rows*/8, /*cols*/8, /*M*/4, /*corr*/0.5, /*seed*/123);
  BCHECK(g.NumVertex() == 64, "64 vertices");
  BCHECK(g.CostDim() == 4, "M=4 cost dims");
  std::vector<long> succ = g.GetSuccs(9); // interior node
  BCHECK(!succ.empty(), "interior node has successors");
  std::vector<double> c = g.GetCost(9, succ[0]);
  BCHECK(c.size() == 4, "cost vector has M entries");
  return 0;
}
