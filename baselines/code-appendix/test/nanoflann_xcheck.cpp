// test/nanoflann_xcheck.cpp
// Cross-check our hand-rolled k-d tree against nanoflann (a tuned header-only
// k-d library) on the captured streams.
//
// IMPORTANT framing: nanoflann answers nearest-neighbor / radius queries, which
// CANNOT express the Pareto dominance query (an orthogonal-region "is any stored
// point <= g componentwise" test). So nanoflann is not a query-cost oracle for
// our use; the freshly-rebuilt static k-d tree (the `kd` backend) is, and kdinc
// is already within ~10-15% of it. What we CAN compare like-for-like is the one
// operation both structures share -- building a static k-d tree from a point
// cloud -- which is the dominant remaining cost in kdinc's amortized update. We
// report that, plus a knn-vs-dominance query time as a rough traversal yardstick
// (different operations; read with care).
//
// usage: nanoflann_xcheck <stream_file>
#include "nanoflann/nanoflann.hpp"
#include "bench/op_stream.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
using rzq::bench::OpStream;
using clk = std::chrono::high_resolution_clock;
static double us(clk::time_point a, clk::time_point b) {
  return std::chrono::duration<double, std::micro>(b - a).count();
}

// ---- nanoflann dataset adaptor over a vector<vector<double>> point cloud ----
struct Cloud {
  const std::vector<std::vector<double> >& pts;
  size_t dim;
  inline size_t kdtree_get_point_count() const { return pts.size(); }
  inline double kdtree_get_pt(size_t i, size_t d) const { return pts[i][d]; }
  template <class BBOX> bool kdtree_get_bbox(BBOX&) const { return false; }
};
typedef nanoflann::KDTreeSingleIndexAdaptor<
    nanoflann::L2_Simple_Adaptor<double, Cloud>, Cloud, -1> NFTree;

// ---- our static k-d tree: identical split rule and dominance query as kdinc ----
struct KNode { int axis; std::vector<double> pt, lo; KNode* l; KNode* r; };
static KNode* OurBuild(std::vector<const std::vector<double>*>& p, int lo, int hi,
                       int depth, int dim) {
  if (lo >= hi) return NULL;
  int axis = depth % dim;
  int mid = (lo + hi) / 2;
  std::nth_element(p.begin() + lo, p.begin() + mid, p.begin() + hi,
                   [&](const std::vector<double>* a, const std::vector<double>* b){
                     return (*a)[axis] < (*b)[axis]; });
  KNode* n = new KNode();
  n->axis = axis; n->pt = *p[mid]; n->l = NULL; n->r = NULL;
  n->l = OurBuild(p, lo, mid, depth + 1, dim);
  n->r = OurBuild(p, mid + 1, hi, depth + 1, dim);
  n->lo = n->pt;
  if (n->l) for (int d = 0; d < dim; ++d) n->lo[d] = std::min(n->lo[d], n->l->lo[d]);
  if (n->r) for (int d = 0; d < dim; ++d) n->lo[d] = std::min(n->lo[d], n->r->lo[d]);
  return n;
}
static void OurFree(KNode* n) { if (!n) return; OurFree(n->l); OurFree(n->r); delete n; }
static bool OurDom(KNode* n, const std::vector<double>& g, int dim) {
  if (!n) return false;
  for (int d = 0; d < dim; ++d) if (n->lo[d] > g[d]) return false;
  bool dom = true; for (int d = 0; d < dim; ++d) if (n->pt[d] > g[d]) { dom = false; break; }
  if (dom) return true;
  if (OurDom(n->l, g, dim)) return true;
  return OurDom(n->r, g, dim);
}

int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: nanoflann_xcheck <stream_file>\n"; return 1; }
  OpStream s; size_t dim = 0;
  if (!rzq::bench::ReadStream(argv[1], &s, &dim)) { std::cerr << "read fail\n"; return 1; }
  std::vector<std::vector<double> > P; std::vector<std::vector<double> > Q;
  for (size_t i = 0; i < s.size(); ++i)
    (s[i].type == rzq::bench::OP_UPDATE ? P : Q).push_back(s[i].vec);
  std::cout << "stream=" << argv[1] << " dim=" << dim
            << " cloud(updates)=" << P.size() << " queries(checks)=" << Q.size() << "\n";
  if (P.empty()) { std::cerr << "empty cloud\n"; return 1; }

  const int REP = 5;
  // ---- build cost (like-for-like: both build a static k-d tree over P) ----
  Cloud cloud{P, dim};
  double nf_build = 0, our_build = 0;
  for (int r = 0; r < REP; ++r) {
    auto a = clk::now();
    NFTree tree(dim, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(
        10, nanoflann::KDTreeSingleIndexAdaptorFlags::SkipInitialBuildIndex));
    tree.buildIndex();
    auto b = clk::now(); nf_build += us(a, b);

    std::vector<const std::vector<double>*> ptr(P.size());
    for (size_t i = 0; i < P.size(); ++i) ptr[i] = &P[i];
    auto c = clk::now();
    KNode* root = OurBuild(ptr, 0, (int)P.size(), 0, (int)dim);
    auto d = clk::now(); our_build += us(c, d);
    OurFree(root);
  }
  nf_build /= REP; our_build /= REP;

  // ---- query yardstick (DIFFERENT operations: knn vs dominance) ----
  NFTree tree(dim, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
  double nf_q = 0;
  auto qa = clk::now();
  for (size_t i = 0; i < Q.size(); ++i) {
    size_t idx; double dist; nanoflann::KNNResultSet<double> rs(1); rs.init(&idx, &dist);
    tree.findNeighbors(rs, Q[i].data());
  }
  nf_q = us(qa, clk::now());

  std::vector<const std::vector<double>*> ptr(P.size());
  for (size_t i = 0; i < P.size(); ++i) ptr[i] = &P[i];
  KNode* root = OurBuild(ptr, 0, (int)P.size(), 0, (int)dim);
  double our_q = 0; volatile bool sink = false;
  auto qc = clk::now();
  for (size_t i = 0; i < Q.size(); ++i) sink = OurDom(root, Q[i], (int)dim);
  our_q = us(qc, clk::now()); (void)sink;
  OurFree(root);

  printf("\n[BUILD over %zu pts, avg of %d]\n", P.size(), REP);
  printf("  nanoflann buildIndex : %10.1f us   (%.2f M pts/s)\n", nf_build, P.size()/nf_build);
  printf("  ours (kdinc _build)  : %10.1f us   (%.2f M pts/s)\n", our_build, P.size()/our_build);
  printf("  ratio ours/nanoflann : %6.2fx\n", our_build / nf_build);
  printf("[QUERY over %zu queries] (knn k=1 vs dominance -- different ops, rough yardstick)\n", Q.size());
  printf("  nanoflann knn(1)     : %10.1f us   (%.3f us/query)\n", nf_q, nf_q/Q.size());
  printf("  ours dominance       : %10.1f us   (%.3f us/query)\n", our_q, our_q/Q.size());
  return 0;
}
