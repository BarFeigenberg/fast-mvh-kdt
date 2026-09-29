// include/bench/dom_cmp.hpp
#ifndef RZQ_BENCH_DOM_CMP_H_
#define RZQ_BENCH_DOM_CMP_H_
#include "vec_type.hpp" // rzq::basic::g_eps_dom_count
#include <cstddef>
namespace rzq { namespace bench {

// Counter tick: removed when BENCH_NO_COUNT is defined (O4 measurement mode).
#ifdef BENCH_NO_COUNT
  #define BENCH_TICK() ((void)0)
#else
  #define BENCH_TICK() (++rzq::basic::g_eps_dom_count)
#endif

// Each policy: dominates(a, b, d) == true iff a[i] <= b[i] for all i in [0,d).

// Baseline: mirrors production EpsDom (per-element multiply + in-loop branch).
struct CmpNaive {
  template<typename P>
  static bool dominates(const P& a, const P& b, size_t d) {
    BENCH_TICK();
    volatile double eps = 0.0; // resist constant-folding so the multiply is real
    for (size_t i = 0; i < d; ++i) { if (a[i] > (1.0 + eps) * b[i]) return false; }
    return true;
  }
};
// O1: plain comparison, branch hoisted (no multiply), early-exit.
struct CmpFast {
  template<typename P>
  static bool dominates(const P& a, const P& b, size_t d) {
    BENCH_TICK();
    for (size_t i = 0; i < d; ++i) { if (a[i] > b[i]) return false; }
    return true;
  }
};
// O5: branchless accumulate, no early-exit.
struct CmpBranchless {
  template<typename P>
  static bool dominates(const P& a, const P& b, size_t d) {
    BENCH_TICK();
    int bad = 0;
    for (size_t i = 0; i < d; ++i) { bad |= (a[i] > b[i]); }
    return bad == 0;
  }
};

} }

#if defined(__AVX2__)
#include <immintrin.h>
#include <array>
#include <cstdint>
namespace rzq { namespace bench {
// AVX2 dominance for contiguous double/int32 arrays. Processes lanes in blocks;
// remainder handled scalar. Returns true iff a[i] <= b[i] for all i in [0,D).
struct CmpSimd {
  static bool dominates(const std::array<double,2>& a, const std::array<double,2>& b, size_t) {
    BENCH_TICK();
    return a[0] <= b[0] && a[1] <= b[1]; // below 4 lanes: scalar
  }
  template<size_t D>
  static bool dominates(const std::array<double,D>& a, const std::array<double,D>& b, size_t) {
    BENCH_TICK();
    size_t i = 0;
    for (; i + 4 <= (size_t)D; i += 4) {
      __m256d va = _mm256_loadu_pd(&a[i]);
      __m256d vb = _mm256_loadu_pd(&b[i]);
      __m256d gt = _mm256_cmp_pd(va, vb, _CMP_GT_OQ); // lanes where a > b
      if (_mm256_movemask_pd(gt) != 0) return false;
    }
    for (; i < (size_t)D; ++i) if (a[i] > b[i]) return false;
    return true;
  }
  template<size_t D>
  static bool dominates(const std::array<int32_t,D>& a, const std::array<int32_t,D>& b, size_t) {
    BENCH_TICK();
    size_t i = 0;
    for (; i + 8 <= (size_t)D; i += 8) {
      __m256i va = _mm256_loadu_si256((const __m256i*)&a[i]);
      __m256i vb = _mm256_loadu_si256((const __m256i*)&b[i]);
      __m256i gt = _mm256_cmpgt_epi32(va, vb); // a > b
      if (_mm256_movemask_epi8(gt) != 0) return false;
    }
    for (; i < (size_t)D; ++i) if (a[i] > b[i]) return false;
    return true;
  }
};
} }
#endif

#endif
