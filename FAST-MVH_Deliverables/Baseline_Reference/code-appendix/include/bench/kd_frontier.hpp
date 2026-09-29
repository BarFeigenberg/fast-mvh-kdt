// include/bench/kd_frontier.hpp
#ifndef RZQ_BENCH_KD_FRONTIER_H_
#define RZQ_BENCH_KD_FRONTIER_H_
#include "bench/ifrontier.hpp"
#include <stdexcept>
namespace rzq { namespace bench {
class KDFrontier : public IFrontier {
public:
  explicit KDFrontier(size_t dim) : _dim(dim) {
    if (dim == 0) throw std::invalid_argument("KDFrontier: dim must be >= 1");
  }
  KDFrontier(const KDFrontier&) = delete;
  KDFrontier& operator=(const KDFrontier&) = delete;
  ~KDFrontier() { _free(_root); }
  bool Check(const CostVec& g) override;
  void Update(const CostVec& g) override;
  size_t Size() const override { return _pts.size(); }
private:
  struct Node {
    int idx; int axis; CostVec lo; // per-axis min over subtree
    Node* l; Node* r;
    Node() : idx(-1), axis(0), l(NULL), r(NULL) {}
  };
  size_t _dim;
  std::vector<CostVec> _pts; // current non-dominated set
  Node* _root = NULL;
  Node* _build(std::vector<int>& ids, int depth);
  bool _query(Node* n, const CostVec& g) const;
  void _free(Node* n);
  void _rebuild();
};
} }
#endif
