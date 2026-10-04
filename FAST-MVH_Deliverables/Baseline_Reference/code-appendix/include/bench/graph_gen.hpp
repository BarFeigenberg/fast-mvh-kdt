// include/bench/graph_gen.hpp
#ifndef RZQ_BENCH_GRAPH_GEN_H_
#define RZQ_BENCH_GRAPH_GEN_H_
#include "graph.hpp"
namespace rzq { namespace bench {
// 4-connected grid, rows*cols vertices id = r*cols + c, bidirectional arcs.
// Each arc gets an M-dim positive integer cost; objective 0 is a base cost,
// objectives 1..M-1 are correlated to it by `corr` in [0,1] plus noise.
// Seeded and reproducible.
void GenGridGraph(basic::SparseGraph* g, int rows, int cols, int M,
                  double corr, unsigned seed);
} }
#endif
