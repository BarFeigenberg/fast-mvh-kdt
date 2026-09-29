// include/bench/nd_inc_frontier.hpp
//
// Concrete incremental ND-tree frontier for the headline comparison. Uses the
// production dominance (basic::EpsDom) over std::vector<double>, exactly like
// NDTreeFrontier, so the only variable that differs from NDTreeFrontier is the
// update strategy: incremental insert + physical leaf deletion + amortized
// rebuild instead of full reconstruction on every Update. See
// nd_inc_frontier_t.hpp for the templated version and the correctness argument.
#ifndef RZQ_BENCH_ND_INC_FRONTIER_H_
#define RZQ_BENCH_ND_INC_FRONTIER_H_
#include "bench/ifrontier.hpp"
#include <stdexcept>
namespace rzq { namespace bench {
class NDIncFrontier : public IFrontier {
public:
  explicit NDIncFrontier(size_t dim, size_t leaf_cap = 8)
    : _dim(dim), _leaf_cap(leaf_cap) {
    if (dim == 0) throw std::invalid_argument("NDIncFrontier: dim must be >= 1");
  }
  NDIncFrontier(const NDIncFrontier&) = delete;
  NDIncFrontier& operator=(const NDIncFrontier&) = delete;
  ~NDIncFrontier() { _free(_root); }
  bool Check(const CostVec& g) override;
  void Update(const CostVec& g) override;
  size_t Size() const override { return _live; }
private:
  struct Node {
    bool leaf; CostVec ideal; CostVec nadir;
    std::vector<CostVec> pts;
    std::vector<Node*> kids;
    Node() : leaf(true) {}
  };
  static const int REBUILD_FACTOR = 2;
  static const int REBUILD_SLACK = 8;
  size_t _dim, _leaf_cap, _live = 0, _built = 0;
  Node* _root = NULL;
  void _free(Node* n);
  void _foldIn(Node* n, const CostVec& z) const;
  void _recomputeLU(Node* n) const;
  double _sqdist(const CostVec& a, const CostVec& b) const;
  Node* _makeLeaf(std::vector<CostVec>& g) const;
  void _splitLeaf(Node* n);
  void _insert(const CostVec& z);
  bool _dominatedBy(Node* n, const CostVec& g) const;
  void _removeDominated(Node* n, const CostVec& z);
  void _collect(Node* n, std::vector<CostVec>* out) const;
  Node* _bulk(std::vector<CostVec>& pts) const;
  void _rebuild();
};
} }
#endif
