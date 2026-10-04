// source/bench/nd_inc_frontier.cpp
#include "bench/nd_inc_frontier.hpp"
#include "vec_type.hpp"
#include <algorithm>
namespace rzq { namespace bench {

void NDIncFrontier::_free(Node* n) {
  if (!n) return;
  for (size_t i = 0; i < n->kids.size(); ++i) _free(n->kids[i]);
  delete n;
}
void NDIncFrontier::_foldIn(Node* n, const CostVec& z) const {
  for (size_t d = 0; d < _dim; ++d) {
    if (z[d] < n->ideal[d]) n->ideal[d] = z[d];
    if (z[d] > n->nadir[d]) n->nadir[d] = z[d];
  }
}
void NDIncFrontier::_recomputeLU(Node* n) const {
  if (n->leaf) { n->ideal = n->pts[0]; n->nadir = n->pts[0];
    for (size_t i = 1; i < n->pts.size(); ++i) _foldIn(n, n->pts[i]);
  } else { n->ideal = n->kids[0]->ideal; n->nadir = n->kids[0]->nadir;
    for (size_t k = 1; k < n->kids.size(); ++k)
      for (size_t d = 0; d < _dim; ++d) {
        if (n->kids[k]->ideal[d] < n->ideal[d]) n->ideal[d] = n->kids[k]->ideal[d];
        if (n->kids[k]->nadir[d] > n->nadir[d]) n->nadir[d] = n->kids[k]->nadir[d];
      }
  }
}
double NDIncFrontier::_sqdist(const CostVec& a, const CostVec& b) const {
  double sq = 0.0;
  for (size_t d = 0; d < _dim; ++d) { double t = a[d] - b[d]; sq += t*t; }
  return sq;
}
NDIncFrontier::Node* NDIncFrontier::_makeLeaf(std::vector<CostVec>& g) const {
  Node* n = new Node(); n->leaf = true; n->pts.swap(g); _recomputeLU(n); return n;
}
void NDIncFrontier::_splitLeaf(Node* n) {
  std::vector<CostVec> pts; pts.swap(n->pts);
  size_t s0 = 0, s1 = 0; double best = -1.0;
  for (size_t i = 1; i < pts.size(); ++i) {
    double dd = _sqdist(pts[i], pts[s0]);
    if (dd > best) { best = dd; s1 = i; }
  }
  std::vector<CostVec> g0, g1;
  for (size_t i = 0; i < pts.size(); ++i)
    (_sqdist(pts[i], pts[s1]) < _sqdist(pts[i], pts[s0]) ? g1 : g0).push_back(pts[i]);
  if (g0.empty() || g1.empty()) { n->pts.swap(pts); return; }
  n->leaf = false; n->kids.clear();
  Node* c0 = _makeLeaf(g0); Node* c1 = _makeLeaf(g1);
  n->kids.push_back(c0); n->kids.push_back(c1);
  _recomputeLU(n);
  if (c0->pts.size() > _leaf_cap) _splitLeaf(c0);
  if (c1->pts.size() > _leaf_cap) _splitLeaf(c1);
}
void NDIncFrontier::_insert(const CostVec& z) {
  if (!_root) { _root = new Node(); _root->leaf = true; _root->pts.push_back(z);
    _root->ideal = z; _root->nadir = z; ++_live; return; }
  Node* n = _root;
  for (;;) {
    _foldIn(n, z);
    if (n->leaf) { n->pts.push_back(z); ++_live;
      if (n->pts.size() > _leaf_cap) _splitLeaf(n); break; }
    size_t best = 0; double bd = _sqdist(z, n->kids[0]->ideal);
    for (size_t k = 1; k < n->kids.size(); ++k) {
      double dd = _sqdist(z, n->kids[k]->ideal);
      if (dd < bd) { bd = dd; best = k; }
    }
    n = n->kids[best];
  }
}
bool NDIncFrontier::_dominatedBy(Node* n, const CostVec& g) const {
  if (!n) return false;
  if (!basic::EpsDom(n->ideal, g)) return false;
  if (n->leaf) {
    for (size_t i = 0; i < n->pts.size(); ++i)
      if (basic::EpsDom(n->pts[i], g)) return true;
    return false;
  }
  for (size_t k = 0; k < n->kids.size(); ++k)
    if (_dominatedBy(n->kids[k], g)) return true;
  return false;
}
void NDIncFrontier::_removeDominated(Node* n, const CostVec& z) {
  if (!n) return;
  if (!basic::EpsDom(z, n->nadir)) return;
  if (n->leaf) {
    size_t w = 0;
    for (size_t i = 0; i < n->pts.size(); ++i) {
      if (basic::EpsDom(z, n->pts[i])) { --_live; }
      else { if (w != i) n->pts[w] = n->pts[i]; ++w; }
    }
    n->pts.resize(w);
    return;
  }
  for (size_t k = 0; k < n->kids.size(); ++k) _removeDominated(n->kids[k], z);
}
void NDIncFrontier::_collect(Node* n, std::vector<CostVec>* out) const {
  if (!n) return;
  if (n->leaf) { for (size_t i = 0; i < n->pts.size(); ++i) out->push_back(n->pts[i]); }
  else { for (size_t k = 0; k < n->kids.size(); ++k) _collect(n->kids[k], out); }
}
NDIncFrontier::Node* NDIncFrontier::_bulk(std::vector<CostVec>& pts) const {
  Node* n = new Node();
  if (pts.size() <= _leaf_cap) { n->leaf = true; n->pts = pts; _recomputeLU(n); return n; }
  n->leaf = false;
  size_t s0 = 0, s1 = 0; double best = -1.0;
  for (size_t i = 1; i < pts.size(); ++i) {
    double dd = _sqdist(pts[i], pts[s0]);
    if (dd > best) { best = dd; s1 = i; }
  }
  std::vector<CostVec> g0, g1;
  for (size_t i = 0; i < pts.size(); ++i)
    (_sqdist(pts[i], pts[s1]) < _sqdist(pts[i], pts[s0]) ? g1 : g0).push_back(pts[i]);
  if (g0.empty() || g1.empty()) { delete n; Node* leaf = new Node();
    leaf->leaf = true; leaf->pts = pts; _recomputeLU(leaf); return leaf; }
  n->kids.push_back(_bulk(g0));
  n->kids.push_back(_bulk(g1));
  _recomputeLU(n);
  return n;
}
void NDIncFrontier::_rebuild() {
  std::vector<CostVec> live; live.reserve(_live);
  _collect(_root, &live);
  _free(_root);
  _root = live.empty() ? NULL : _bulk(live);
  _built = live.size(); _live = live.size();
}
bool NDIncFrontier::Check(const CostVec& g) {
  if (g.empty()) return false;
  return _dominatedBy(_root, g);
}
void NDIncFrontier::Update(const CostVec& g) {
  if (_root) _removeDominated(_root, g);
  _insert(g);
  if (_live > (size_t)REBUILD_FACTOR * _built + REBUILD_SLACK ||
      _built > (size_t)REBUILD_FACTOR * _live + REBUILD_SLACK) _rebuild();
}
} }
