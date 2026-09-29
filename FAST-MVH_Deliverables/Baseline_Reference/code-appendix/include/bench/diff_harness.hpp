// include/bench/diff_harness.hpp
#ifndef RZQ_BENCH_DIFF_HARNESS_H_
#define RZQ_BENCH_DIFF_HARNESS_H_
#include <string>
#include "bench/ifrontier.hpp"
#include "bench/op_stream.hpp"
namespace rzq { namespace bench {
// Replays s on `backend` and a fresh LinearFrontier oracle in lockstep.
// Returns false and fills *err on the first divergence (Check result or size).
bool RunDiff(IFrontier* backend, const OpStream& s, std::string* err);
} }
#endif
