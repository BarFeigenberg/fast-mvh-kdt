// test/test_synth_gen.cpp
#include "bench/synth_gen.hpp"
#include "bench/linear_frontier.hpp"
#include "bench/test_util.hpp"
#include <cmath>
using namespace rzq::bench;
int main() {
  SynthParams p;
  p.dim = 3; p.n_ops = 5000; p.target_frontier = 40; p.corr = 0.5; p.seed = 99;
  OpStream s = GenSynthStream(p);
  BCHECK(s.size() == 5000, "op count honored");
  // replay and confirm the steady-state frontier size lands near target.
  LinearFrontier f;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i].type == OP_CHECK) f.Check(s[i].vec);
    else if (!f.Check(s[i].vec)) f.Update(s[i].vec);
  }
  BCHECK(std::abs((double)f.Size() - 40.0) < 25.0, "frontier near target size");
  return 0;
}
