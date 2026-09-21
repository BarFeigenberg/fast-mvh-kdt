// test/road_costs.hpp
// Road-network cost-loading helpers shared by the road drivers
// (run_emoa_road, road_capture). Extracted verbatim (DRY) from the original
// run_emoa_road.cpp so both drivers build identical graphs from identical seeds.
//
// Cost models:
//   - MakeCost:    reproducible LCG grid cost model (base + correlated noise).
//   - LoadTNTP:    TransportationNetworks _net files (real attrs or synth grid).
//   - LoadPACE:    PACE-2016 .gr topology, synthetic grid costs per arc.
//   - LoadRoad:    dispatch TNTP vs PACE by first content line.
//   - LoadPACEGeo: landmark-field GEOMETRIC costs on a PACE topology (rho blend).
//
// All helpers are inline and live in namespace rzq so multiple translation
// units may include this header without ODR violations.
#ifndef RZQ_TEST_ROAD_COSTS_H_
#define RZQ_TEST_ROAD_COSTS_H_
#include "graph.hpp"
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include <utility>

namespace rzq {

// Reproducible cost model identical to bench::GenGridGraph (LCG + base/correlated
// noise), so a synthetic-cost road instance differs from the grid experiment only
// in its (real, irregular) topology.
inline unsigned LcgNext(unsigned& st) { st = st * 1103515245u + 12345u; return (st >> 16) & 0x7fff; }
inline std::vector<double> MakeCost(int M, double corr, unsigned& st) {
  std::vector<double> c(M);
  double base = 1.0 + (LcgNext(st) % 20); c[0] = base;
  for (int k = 1; k < M; ++k) {
    double indep = (double)(LcgNext(st) % 20);
    double v = corr * base + (1.0 - corr) * indep;
    if (v < 1.0) v = 1.0;
    c[k] = (double)((long)v);
  }
  return c;
}

// ---- Landmark-field geometric cost model (see paper appendix) ----------------
// PACE topology carries no coordinates, but a road network's graph distance
// approximates real geometry (the basis of ALT routing). We pick L landmarks by
// farthest-point sampling, compute their BFS hop-distance fields, and build M
// smooth objective fields as controllably-decorrelated linear mixes of those
// fields. rho blends each field toward a shared distance-like field: rho=1 is
// perfectly correlated, rho=0 maximally decorrelated. Deterministic given seed.
static const double kGeoPi = 3.14159265358979323846;

inline void GeoBfs(const std::vector<std::vector<int> >& adj,
                   const std::vector<int>& srcs, std::vector<int>* dist) {
  std::fill(dist->begin(), dist->end(), -1);
  std::queue<int> q;
  for (size_t i = 0; i < srcs.size(); ++i) { (*dist)[srcs[i]] = 0; q.push(srcs[i]); }
  while (!q.empty()) {
    int u = q.front(); q.pop();
    for (size_t i = 0; i < adj[u].size(); ++i) {
      int w = adj[u][i];
      if ((*dist)[w] < 0) { (*dist)[w] = (*dist)[u] + 1; q.push(w); }
    }
  }
}
inline double GeoGauss(unsigned& s) { // Box-Muller from two LCG uniforms
  double u1 = ((LcgNext(s) % 10000) + 1) / 10001.0;
  double u2 = (LcgNext(s) % 10000) / 10000.0;
  return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kGeoPi * u2);
}

inline bool LoadPACEGeo(const std::string& path, int M, double rho, unsigned seed,
                        basic::SparseGraph* g, long* maxNode) {
  std::ifstream f(path.c_str());
  if (!f) { std::cerr << "cannot open " << path << "\n"; return false; }
  std::vector<std::pair<int,int> > edges; int N = 0; std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == 'c' || line[0] == 'p') continue;
    std::istringstream ss(line); int u, v;
    if (!(ss >> u >> v)) continue;
    edges.push_back(std::make_pair(u, v));
    if (u > N) N = u; if (v > N) N = v;
  }
  if (edges.empty()) { std::cerr << "no edges in " << path << "\n"; return false; }
  std::vector<std::vector<int> > adj(N + 1);
  for (size_t i = 0; i < edges.size(); ++i) {
    adj[edges[i].first].push_back(edges[i].second);
    adj[edges[i].second].push_back(edges[i].first);
  }
  const int L = std::max(8, 2 * M);
  unsigned st = seed;
  std::vector<int> lm; lm.push_back((int)(LcgNext(st) % N) + 1);
  std::vector<int> d(N + 1, -1);
  for (int i = 1; i < L; ++i) {               // farthest-point sampling
    GeoBfs(adj, lm, &d);
    int best = lm[0], bd = -1;
    for (int v = 1; v <= N; ++v) if (d[v] > bd) { bd = d[v]; best = v; }
    lm.push_back(best);
  }
  std::vector<std::vector<double> > fld(L, std::vector<double>(N + 1, 0.0));
  for (int i = 0; i < L; ++i) {               // normalized landmark fields
    std::vector<int> src(1, lm[i]); GeoBfs(adj, src, &d);
    int mx = 1; for (int v = 1; v <= N; ++v) if (d[v] > mx) mx = d[v];
    for (int v = 1; v <= N; ++v) fld[i][v] = d[v] >= 0 ? (double)d[v] / mx : 0.0;
  }
  // M decorrelated coefficient vectors (Gram-Schmidt of seeded Gaussians).
  std::vector<std::vector<double> > orth;
  for (int k = 0; k < M; ++k) {
    std::vector<double> a(L); for (int i = 0; i < L; ++i) a[i] = GeoGauss(st);
    for (size_t j = 0; j < orth.size(); ++j) {
      double dot = 0, nn = 0;
      for (int i = 0; i < L; ++i) { dot += a[i] * orth[j][i]; nn += orth[j][i] * orth[j][i]; }
      double c = nn > 0 ? dot / nn : 0;
      for (int i = 0; i < L; ++i) a[i] -= c * orth[j][i];
    }
    double nrm = 0; for (int i = 0; i < L; ++i) nrm += a[i] * a[i]; nrm = std::sqrt(nrm) + 1e-12;
    for (int i = 0; i < L; ++i) a[i] /= nrm; orth.push_back(a);
  }
  std::vector<double> ashared(L, 1.0 / std::sqrt((double)L));
  // Shared smooth "position" field phi0 (the rho->1, fully-correlated limit).
  std::vector<double> phi0(N + 1, 0.0);
  { double lo = 1e18, hi = -1e18;
    for (int v = 1; v <= N; ++v) { double s = 0; for (int i = 0; i < L; ++i) s += ashared[i] * fld[i][v];
      phi0[v] = s; if (s < lo) lo = s; if (s > hi) hi = s; }
    double rng = (hi - lo) > 1e-12 ? (hi - lo) : 1.0;
    for (int v = 1; v <= N; ++v) phi0[v] = (phi0[v] - lo) / rng; }
  // Per-objective multi-frequency oscillatory fields (Perlin-like over the landmark
  // distances): smooth attributes (terrain / congestion / land-use bands) that do not
  // merely track position, so objectives genuinely trade off on a near-planar map.
  // rho blends each toward phi0: rho=1 fully correlated, rho=0 maximally decorrelated.
  std::vector<std::vector<double> > phi(M, std::vector<double>(N + 1, 0.0));
  for (int k = 0; k < M; ++k) {
    std::vector<double> freq(L), phase(L);
    for (int i = 0; i < L; ++i) { freq[i] = 1.0 + (double)(LcgNext(st) % 6);
                                  phase[i] = 2.0 * kGeoPi * ((LcgNext(st) % 1000) / 1000.0); }
    double lo = 1e18, hi = -1e18;
    for (int v = 1; v <= N; ++v) {
      double s = 0;
      for (int i = 0; i < L; ++i) s += orth[k][i] * std::sin(2.0 * kGeoPi * freq[i] * fld[i][v] + phase[i]);
      phi[k][v] = s; if (s < lo) lo = s; if (s > hi) hi = s;
    }
    double rng = (hi - lo) > 1e-12 ? (hi - lo) : 1.0;
    for (int v = 1; v <= N; ++v) { double osc = (phi[k][v] - lo) / rng;
                                   phi[k][v] = rho * phi0[v] + (1.0 - rho) * osc; }
  }
  const double Wsc = 64.0;
  std::vector<std::vector<double> > smp; // per-edge cost sample for diagnostics
  for (size_t i = 0; i < edges.size(); ++i) {
    int u = edges[i].first, v = edges[i].second;
    std::vector<double> c(M);
    for (int k = 0; k < M; ++k) {
      double m = 0.5 * (phi[k][u] + phi[k][v]);
      c[k] = 1.0 + (double)((long)(Wsc * m + 0.5));
    }
    g->AddArc(u, v, c); g->AddArc(v, u, c);
    if (smp.size() < 5000) smp.push_back(c);
  }
  *maxNode = N;
  double mc = 0; int cnt = 0;                 // mean |pairwise Pearson corr|
  for (int a = 0; a < M; ++a) for (int b = a + 1; b < M; ++b) {
    double ma = 0, mb = 0;
    for (size_t i = 0; i < smp.size(); ++i) { ma += smp[i][a]; mb += smp[i][b]; }
    ma /= smp.size(); mb /= smp.size();
    double sab = 0, sa = 0, sb = 0;
    for (size_t i = 0; i < smp.size(); ++i) {
      sab += (smp[i][a] - ma) * (smp[i][b] - mb);
      sa  += (smp[i][a] - ma) * (smp[i][a] - ma);
      sb  += (smp[i][b] - mb) * (smp[i][b] - mb);
    }
    double r = (sa > 0 && sb > 0) ? sab / std::sqrt(sa * sb) : 0;
    mc += std::fabs(r); ++cnt;
  }
  std::cout << "[GEO] loaded " << path << " : " << g->NumVertex() << " vertices, "
            << edges.size() << " edges, M=" << M << " L=" << L << " rho=" << rho
            << " mean|pairwise-corr|=" << (cnt ? mc / cnt : 0.0) << "\n";
  return true;
}

// Parse a TNTP _net file into a directed SparseGraph with M costs. cost_mode
// "real" uses TNTP attributes (length/time/congestion/hops); "synth" assigns the
// grid cost model per arc (richer, decorrelated fronts on the real topology).
inline bool LoadTNTP(const std::string& path, int M, bool synth, double corr,
                     unsigned seed, basic::SparseGraph* g, long* maxNode) {
  std::ifstream f(path.c_str());
  if (!f) { std::cerr << "cannot open " << path << "\n"; return false; }
  std::string line; bool inData = false; long mx = 0; size_t links = 0; unsigned st = seed;
  while (std::getline(f, line)) {
    if (!inData) { if (line.find("<END OF METADATA>") != std::string::npos) inData = true; continue; }
    for (size_t i = 0; i < line.size(); ++i) if (line[i] == ';') line[i] = ' ';
    std::istringstream ss(line);
    std::vector<double> tok; double x;
    while (ss >> x) tok.push_back(x);
    if (tok.size() < 5) continue;        // header '~' line and blanks have <5 numbers
    if (line[0] == '~') continue;
    long u = (long)tok[0], v = (long)tok[1];
    std::vector<double> c;
    if (synth) {
      c = MakeCost(M, corr, st);
    } else {
      double cap = tok[2], length = tok[3], fft = tok[4];
      c.push_back(length);                                            // c0 distance
      if (M >= 2) c.push_back(std::round(fft * 1000.0));              // c1 time
      if (M >= 3) c.push_back(std::round(length * 1e5 / (cap > 1 ? cap : 1))); // c2 congestion
      if (M >= 4) c.push_back(1.0);                                  // c3 hops
      if (M >= 5) c.push_back(std::round(cap));                       // c4 capacity
    }
    g->AddArc(u, v, c);
    if (u > mx) mx = u; if (v > mx) mx = v; ++links;
  }
  *maxNode = mx;
  std::cout << "[ROAD] loaded " << path << " : " << g->NumVertex() << " vertices, "
            << links << " arcs, M=" << g->CostDim() << " cost_mode=" << (synth ? "synth" : "real") << "\n";
  return links > 0;
}

// Parse a PACE-2016 tree-decomposition .gr file (real road topology, no edge
// weights): "p tw N M" then undirected "u v" edges. Assigns the synthetic grid
// cost model per directed arc. Used for the DIMACS USA road graphs mirrored at
// github.com/ben-strasser/road-graphs-pace16 (e.g. NY ~264k nodes, FLA ~1M).
inline bool LoadPACE(const std::string& path, int M, double corr, unsigned seed,
                     basic::SparseGraph* g, long* maxNode) {
  std::ifstream f(path.c_str());
  if (!f) { std::cerr << "cannot open " << path << "\n"; return false; }
  std::string line; long mx = 0; size_t edges = 0; unsigned st = seed;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == 'c' || line[0] == 'p') continue;
    std::istringstream ss(line); long u, v;
    if (!(ss >> u >> v)) continue;
    g->AddArc(u, v, MakeCost(M, corr, st));
    g->AddArc(v, u, MakeCost(M, corr, st));
    if (u > mx) mx = u; if (v > mx) mx = v; ++edges;
  }
  *maxNode = mx;
  std::cout << "[ROAD] loaded " << path << " (PACE): " << g->NumVertex()
            << " vertices, " << edges << " edges, M=" << g->CostDim()
            << " corr=" << corr << "\n";
  return edges > 0;
}

// Dispatch by first content line: '<' => TNTP metadata, 'p' => PACE.
inline bool LoadRoad(const std::string& path, int M, bool synth, double corr,
                     unsigned seed, basic::SparseGraph* g, long* maxNode) {
  std::ifstream peek(path.c_str());
  std::string line;
  while (std::getline(peek, line)) { if (!line.empty() && line[0] != 'c') break; }
  if (!line.empty() && line[0] == 'p') return LoadPACE(path, M, corr, seed, g, maxNode);
  return LoadTNTP(path, M, synth, corr, seed, g, maxNode);
}

} // namespace rzq

#endif // RZQ_TEST_ROAD_COSTS_H_
