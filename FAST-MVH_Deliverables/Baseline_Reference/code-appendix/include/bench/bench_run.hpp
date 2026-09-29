// include/bench/bench_run.hpp
#ifndef RZQ_BENCH_BENCH_RUN_H_
#define RZQ_BENCH_BENCH_RUN_H_
#include <string>
#include "bench/op_stream.hpp"
#include "bench/variant_factory.hpp"
namespace rzq { namespace bench {
struct BenchResult {
  std::string backend; size_t dim; size_t checks; size_t updates;
  double t_check_us; double t_update_us; long long dom_cmps_check; long long dom_cmps_update; size_t final_size;
};
BenchResult RunBackend(const std::string& name, const OpStream& s, size_t dim);
BenchResult RunVariant(const std::string& name, const VariantCfg& cfg,
                       const OpStream& s, size_t dim);
} }
#endif
