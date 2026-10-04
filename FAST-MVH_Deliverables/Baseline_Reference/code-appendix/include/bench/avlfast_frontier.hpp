// include/bench/avlfast_frontier.hpp
#ifndef RZQ_BENCH_AVLFAST_FRONTIER_H_
#define RZQ_BENCH_AVLFAST_FRONTIER_H_
#include "bench/dom_cmp.hpp"
#include "bench/ifrontier.hpp"
#include "search_emoa.hpp"
namespace rzq { namespace bench {

// Subclass that reaches the production tree's protected dominance primitives and
// operates directly on already-projected (M-1)-D vectors. This isolates the AVL
// data structure from the per-operation allocation overhead that AVLFrontier
// measures (the lift, the by-value Frontier::Check parameter, the redundant
// `auto pg = g`, and the _p projection copy). The dominance algorithm itself
// (_check, _filter via Filter, Add) is the unmodified production code.
class ExposedFrontier : public rzq::search::Frontier {
public:
  // Query: call the production _check directly on the projected vector. _check
  // takes its key by const reference, so the query path allocates nothing.
  bool CheckProjected(const CostVec& g) { return _check(_root, g); }
  // Maintenance: production incremental filter-then-insert (no rebuild). Mirrors
  // Frontier::Update minus the _p projection and the path-only label bookkeeping.
  void UpdateProjected(const CostVec& g) {
    if (this->Size() == 0) { Add(g); return; }
    Filter(g);
    Add(g);
  }
  // Query variant: same tree-walk shape as the production _check, but with the
  // dominance test swapped for Cmp::dominates. Lets the ablation vary O1/O5-style
  // comparisons on the production AVL storage (whose std::vector<double> layout
  // cannot itself be varied).
  template<typename Cmp>
  bool CheckCmp(const CostVec& g) { return _checkCmp<Cmp>(_root, g); }
private:
  template<typename Cmp>
  bool _checkCmp(rzq::basic::AVLNode* n, const CostVec& k) {
    if (n == NULL) return false;
    if (Cmp::dominates(_key[n->id], k, k.size())) return true;
    if (_key[n->id] > k) return _checkCmp<Cmp>(n->left, k);
    if (_checkCmp<Cmp>(n->left, k)) return true;
    return _checkCmp<Cmp>(n->right, k);
  }
public:
};

// Allocation-light measurement of the production AVL frontier.
class AVLFastFrontier : public IFrontier {
public:
  bool Check(const CostVec& g) override { return _f.CheckProjected(g); }
  void Update(const CostVec& g) override { _f.UpdateProjected(g); }
  size_t Size() const override { return _f.Size(); }
private:
  ExposedFrontier _f;
};

// Cmp-templated variant: production AVL storage/maintenance, swappable dominance
// comparison at query time. Update stays on the production incremental path
// (UpdateProjected); only Check is templated.
template<typename Cmp>
class AVLFastCmpFrontier : public IFrontier {
public:
  bool Check(const CostVec& g) override { return _f.template CheckCmp<Cmp>(g); }
  void Update(const CostVec& g) override { _f.UpdateProjected(g); }
  size_t Size() const override { return _f.Size(); }
private:
  ExposedFrontier _f;
};

} }
#endif
