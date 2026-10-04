// include/bench/nd_frontier.hpp
#ifndef RZQ_BENCH_ND_FRONTIER_H_
#define RZQ_BENCH_ND_FRONTIER_H_
#include <stdexcept>
#include "bench/ifrontier.hpp"
namespace rzq { namespace bench {
class NDTreeFrontier : public IFrontier {
public:
  explicit NDTreeFrontier(size_t dim, size_t leaf_cap = 8, size_t max_children = 4)
    : _dim(dim), _leaf_cap(leaf_cap), _max_children(max_children) {
    if (dim == 0) throw std::invalid_argument("NDTreeFrontier: dim must be >= 1");
  }
  NDTreeFrontier(const NDTreeFrontier&) = delete;
  NDTreeFrontier& operator=(const NDTreeFrontier&) = delete;
  ~NDTreeFrontier() { _free(_root); }
  bool Check(const CostVec& g) override;
  void Update(const CostVec& g) override;
  size_t Size() const override { return _size; }
private:
  struct Node {
    bool leaf; CostVec ideal;       // ideal = per-axis min corner (for query pruning)
    std::vector<CostVec> pts;       // leaf payload
    std::vector<Node*> kids;        // internal children
    Node() : leaf(true) {}
  };
  size_t _dim, _leaf_cap, _max_children, _size = 0;
  Node* _root = NULL;
  void _free(Node* n);
  void _recompute(Node* n);
  bool _dominatedBy(Node* n, const CostVec& g) const; // some pt in n dominates g?
  void _collect(Node* n, std::vector<CostVec>* out) const;
  Node* _bulk(std::vector<CostVec>& pts);
};
} }
#endif
