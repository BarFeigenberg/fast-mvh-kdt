// include/bench/test_util.hpp
#ifndef RZQ_BENCH_TEST_UTIL_H_
#define RZQ_BENCH_TEST_UTIL_H_
#include <iostream>
#include <cstdlib>
#define BCHECK(cond, msg) do { \
  if (!(cond)) { \
    std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " " << (msg) << std::endl; \
    std::exit(1); \
  } \
} while (0)
#endif
