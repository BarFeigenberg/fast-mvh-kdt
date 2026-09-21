// include/bench/kd_inc_frontier.hpp
//
// Concrete incremental k-d tree frontier for the headline comparison. Uses the
// production dominance (basic::EpsDom) over std::vector<double>, exactly like
// KDFrontier, so the only variable that differs from KDFrontier is the update
// strategy: incremental insert + lazy deletion + amortized rebuild instead of
// full reconstruction on every Update. See kd_inc_frontier_t.hpp for the
// templated (storage/comparator-parameterized) version and the correctness
// argument.
#ifndef RZQ_BENCH_KD_INC_FRONTIER_H_
#define RZQ_BENCH_KD_INC_FRONTIER_H_
#include "bench/ifrontier.hpp"
#include <stdexcept>
namespace rzq { namespace bench {
class KDIncFrontier : public IFrontier {
public:
  explicit KDIncFrontier(size_t dim) : _dim(dim) {
    if (dim == 0) throw std::invalid_argument("KDIncFrontier: dim must be >= 1");
  }
  KDIncFrontier(const KDIncFrontier&) = delete;
  KDIncFrontier& operator=(const KDIncFrontier&) = delete;
  ~KDIncFrontier() { _free(_root); }
  bool Check(const CostVec& g) override;
  void Update(const CostVec& g) override;
  size_t Size() const override { return _live; }
private:
  struct Node {
    int axis; bool dead; CostVec pt; CostVec lo; CostVec hi; Node* l; Node* r;
    Node() : axis(0), dead(false), l(NULL), r(NULL) {}
  };
  static const int REBUILD_FACTOR = 2;
  static const int REBUILD_SLACK = 8;
  size_t _dim;
  Node* _root = NULL;
  size_t _live = 0, _total = 0, _built = 0;
  void _initLeaf(Node* n, const CostVec& gp, int axis);
  void _insert(const CostVec& gp);
  bool _query(Node* n, const CostVec& g) const;
  void _markDominated(Node* n, const CostVec& gp);
  void _collectLive(Node* n, std::vector<CostVec>& out) const;
  Node* _build(std::vector<CostVec>& pts, size_t lo, size_t hi, int depth);
  void _rebuild();
  void _free(Node* n);
};
} }
#endif
