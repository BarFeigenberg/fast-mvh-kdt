// source/bench/graph_gen.cpp
#include "bench/graph_gen.hpp"
namespace rzq { namespace bench {
// simple reproducible LCG returning a value in [0, 32767]
static inline unsigned LcgNext(unsigned& st) {
  st = st * 1103515245u + 12345u;
  return (st >> 16) & 0x7fff;
}
static std::vector<double> MakeCost(int M, double corr, unsigned& st) {
  std::vector<double> c(M);
  double base = 1.0 + (LcgNext(st) % 20);
  c[0] = base;
  for (int k = 1; k < M; ++k) {
    double indep = (double)(LcgNext(st) % 20);
    double v = corr * base + (1.0 - corr) * indep;
    if (v < 1.0) v = 1.0;
    c[k] = (double)((long)v); // positive integer cost
  }
  return c;
}
void GenGridGraph(basic::SparseGraph* g, int rows, int cols, int M,
                  double corr, unsigned seed) {
  unsigned st = seed;
  g->ChangeCostDim((size_t)M);
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c)
      g->AddVertex((long)(r * cols + c));
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c) {
      int v = r * cols + c;
      if (c + 1 < cols) { // east neighbor
        int u = v + 1;
        g->AddArc((long)v, (long)u, MakeCost(M, corr, st));
        g->AddArc((long)u, (long)v, MakeCost(M, corr, st));
      }
      if (r + 1 < rows) { // south neighbor
        int u = v + cols;
        g->AddArc((long)v, (long)u, MakeCost(M, corr, st));
        g->AddArc((long)u, (long)v, MakeCost(M, corr, st));
      }
    }
}
} }
