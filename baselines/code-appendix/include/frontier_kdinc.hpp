/*******************************************
 * Incremental k-d tree EMOA* frontier (opt-in via EMOA_FRONTIER=kdinc).
 *******************************************/
#ifndef EMOAREF_SEARCH_FRONTIER_KDINC_H_
#define EMOAREF_SEARCH_FRONTIER_KDINC_H_

#include "search_emoa.hpp"
#include "bench/kd_inc_frontier.hpp"
#include <memory>

namespace rzq {
namespace search {

// Drop-in replacement for the AVL-backed Frontier whose dominance index is an
// incremental k-d tree (insert + lazy deletion + amortized rebalancing,
// bench::KDIncFrontier) instead of a lexicographic AVL tree. Selected at
// runtime by EMOA_FRONTIER=kdinc; the default frontier is unchanged.
//
// Only Check/Update are overridden. The projection (_p, drops the first cost
// component) and the label_ids bookkeeping are identical to the baseline, so
// solution reconstruction at the destination vertex is unaffected: at vd the
// lexicographic pop order guarantees no stored solution is ever dominated by a
// later one, so the frontier there only grows, exactly as for the AVL version.
// The underlying index uses basic::EpsDom, so the dominance semantics (and the
// g_eps_dom_count metric) match the production check.
class FrontierKdInc : public Frontier {
public:
  explicit FrontierKdInc(size_t proj_dim)
    : _idx(new rzq::bench::KDIncFrontier(proj_dim)) {}
  ~FrontierKdInc() override {}

  bool Check(CostVec g) override {
    CostVec pg = _p(g);
    CaptureOp(vid, 0, pg);
    return _idx->Check(pg);
  }
  void Update(Label l) override {
    CostVec pg = _p(l.g);
    CaptureOp(vid, 1, pg);
    label_ids.push_back(l.id);
    _idx->Update(pg); // removes stored points dominated by pg, then inserts pg
  }

private:
  std::unique_ptr<rzq::bench::KDIncFrontier> _idx;
};

} // namespace search
} // namespace rzq
#endif
