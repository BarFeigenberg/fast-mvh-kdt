// include/bench/synth_gen.hpp
#ifndef RZQ_BENCH_SYNTH_GEN_H_
#define RZQ_BENCH_SYNTH_GEN_H_
#include "bench/op_stream.hpp"
namespace rzq { namespace bench {
struct SynthParams {
  size_t dim; size_t n_ops; size_t target_frontier;
  double corr; unsigned seed;
};
OpStream GenSynthStream(const SynthParams& p);
// Measure calibration targets from a captured stream (for reporting/fitting).
struct StreamStats { double surviving_update_frac; double mean_frontier; size_t dim; };
StreamStats MeasureStats(const OpStream& s);
} }
#endif
