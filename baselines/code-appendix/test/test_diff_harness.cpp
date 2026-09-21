// test/test_diff_harness.cpp
#include "bench/diff_harness.hpp"
#include "bench/linear_frontier.hpp"
#include "bench/test_util.hpp"
using namespace rzq::bench;
int main() {
  OpStream s;
  s.push_back({OP_UPDATE, {2.0, 2.0}});
  s.push_back({OP_CHECK,  {3.0, 3.0}});
  s.push_back({OP_UPDATE, {1.0, 1.0}});
  s.push_back({OP_CHECK,  {2.0, 2.0}});
  LinearFrontier backend;
  std::string err;
  BCHECK(RunDiff(&backend, s, &err), err.c_str());
  return 0;
}
