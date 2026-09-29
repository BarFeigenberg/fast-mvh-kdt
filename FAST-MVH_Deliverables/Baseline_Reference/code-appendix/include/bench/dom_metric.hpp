// include/bench/dom_metric.hpp
#ifndef RZQ_BENCH_DOM_METRIC_H_
#define RZQ_BENCH_DOM_METRIC_H_
#include "vec_type.hpp"
namespace rzq { namespace bench {
inline void ResetDomCmpCount() { basic::g_eps_dom_count = 0; }
inline long long GetDomCmpCount() { return basic::g_eps_dom_count; }
inline void ResetCoordCmpCount() { basic::g_coord_cmp_count = 0; }
inline long long GetCoordCmpCount() { return basic::g_coord_cmp_count; }
} }
#endif
