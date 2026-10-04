// source/bench/synth_gen.cpp
#include "bench/synth_gen.hpp"
#include "bench/linear_frontier.hpp"
#include "vec_type.hpp"
namespace rzq { namespace bench {
static double frand(unsigned& st){ st = st*1103515245u + 12345u; return ((st>>16)&0x7fff)/32767.0; }
OpStream GenSynthStream(const SynthParams& p) {
  unsigned st = p.seed;
  OpStream s; s.reserve(p.n_ops);
  // Anti-correlated-ish points so a non-trivial antichain forms; the radius band
  // grows with target so steady-state frontier ~ target.
  double R = 5.0 + 3.2 * (double)p.target_frontier;
  for (size_t i = 0; i < p.n_ops; ++i) {
    Op op;
    std::vector<double> base(p.dim);
    double rem = R;
    for (size_t d = 0; d + 1 < p.dim; ++d) { base[d] = frand(st) * rem; rem -= base[d]; }
    base[p.dim - 1] = rem < 0 ? 0 : rem;
    for (size_t d = 1; d < p.dim; ++d)
      base[d] = p.corr * base[0] + (1.0 - p.corr) * base[d];
    for (size_t d = 0; d < p.dim; ++d) base[d] = (double)((long)(base[d] + 1.0));
    op.type = (frand(st) < 0.34) ? OP_UPDATE : OP_CHECK;
    op.vec = base;
    s.push_back(op);
  }
  return s;
}
StreamStats MeasureStats(const OpStream& s) {
  LinearFrontier f; size_t updates = 0, survived = 0; double area = 0; size_t checks = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i].type == OP_UPDATE) {
      ++updates;
      if (!f.Check(s[i].vec)) { ++survived; f.Update(s[i].vec); }
    } else { ++checks; area += f.Size(); }
  }
  StreamStats st;
  st.surviving_update_frac = updates ? (double)survived / updates : 0.0;
  st.mean_frontier = checks ? area / checks : 0.0;
  st.dim = s.empty() ? 0 : s[0].vec.size();
  return st;
}
} }
