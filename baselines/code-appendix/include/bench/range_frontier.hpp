// include/bench/range_frontier.hpp
#ifndef RZQ_BENCH_RANGE_FRONTIER_H_
#define RZQ_BENCH_RANGE_FRONTIER_H_
#include <stdexcept>
#include "bench/ifrontier.hpp"
namespace rzq { namespace bench {
// Layered range tree for dominance-emptiness, rebuilt on batch.
class RangeTreeFrontier : public IFrontier {
public:
  explicit RangeTreeFrontier(size_t dim) : _dim(dim), _root(NULL) {
    if (dim == 0) throw std::invalid_argument("RangeTreeFrontier: dim must be >= 1");
  }
  RangeTreeFrontier(const RangeTreeFrontier&) = delete;
  RangeTreeFrontier& operator=(const RangeTreeFrontier&) = delete;
  ~RangeTreeFrontier() { _free(_root); }
  bool Check(const CostVec& g) override;
  void Update(const CostVec& g) override;
  size_t Size() const override { return _pts.size(); }
private:
  struct Node {
    int axis;
    int mid;            // index into _pts of this node's median point
    double maxv;        // max axis-`axis` value over subtree
    double submin_last; // (last axis only) min axis-`axis` value over subtree
    Node* left; Node* right; Node* assoc;
    Node() : axis(0), mid(-1), maxv(0.0), submin_last(0.0),
             left(NULL), right(NULL), assoc(NULL) {}
  };
  size_t _dim;
  std::vector<CostVec> _pts; // current non-dominated set
  Node* _root;
  Node* _build(std::vector<int> ids, int axis); // ids sorted by _pts[id][axis]
  bool _query(Node* n, const CostVec& g) const;
  void _free(Node* n);
  void _rebuild();
};
} }
#endif
