// source/bench/variant_factory.cpp
#include "bench/variant_factory.hpp"
#include "bench/linear_frontier_t.hpp"
#include "bench/kd_frontier_t.hpp"
#include "bench/kd_inc_frontier_t.hpp"
#include "bench/nd_frontier_t.hpp"
#include "bench/nd_inc_frontier_t.hpp"
#include "bench/range_frontier_t.hpp"
#include "bench/avlfast_frontier.hpp"
namespace rzq { namespace bench {

// ---------- linear ----------
template<template<int> class StoreD, typename Cmp>
static IFrontier* mkLinArr(size_t dim) {
  switch (dim) {
    case 1: return new LinearFrontierT<StoreD<1>, Cmp>();
    case 2: return new LinearFrontierT<StoreD<2>, Cmp>();
    case 3: return new LinearFrontierT<StoreD<3>, Cmp>();
    case 4: return new LinearFrontierT<StoreD<4>, Cmp>();
    case 5: return new LinearFrontierT<StoreD<5>, Cmp>();
    default: return NULL;
  }
}
static IFrontier* makeLinear(size_t dim, const VariantCfg& c) {
  if (c.store == STORE_VEC) {
    switch (c.cmp) {
      case CMP_NAIVE:      return new LinearFrontierT<VecDouble, CmpNaive>();
      case CMP_FAST:       return new LinearFrontierT<VecDouble, CmpFast>();
      case CMP_BRANCHLESS: return new LinearFrontierT<VecDouble, CmpBranchless>();
      default: return NULL; // SIMD invalid on VEC
    }
  }
  if (c.store == STORE_ARRD) {
    switch (c.cmp) {
      case CMP_NAIVE:      return mkLinArr<ArrDouble, CmpNaive>(dim);
      case CMP_FAST:       return mkLinArr<ArrDouble, CmpFast>(dim);
      case CMP_BRANCHLESS: return mkLinArr<ArrDouble, CmpBranchless>(dim);
#ifdef __AVX2__
      case CMP_SIMD:       return mkLinArr<ArrDouble, CmpSimd>(dim);
#endif
      default: return NULL;
    }
  }
  if (c.store == STORE_ARRI) {
    switch (c.cmp) {
      case CMP_NAIVE:      return mkLinArr<ArrInt, CmpNaive>(dim);
      case CMP_FAST:       return mkLinArr<ArrInt, CmpFast>(dim);
      case CMP_BRANCHLESS: return mkLinArr<ArrInt, CmpBranchless>(dim);
#ifdef __AVX2__
      case CMP_SIMD:       return mkLinArr<ArrInt, CmpSimd>(dim);
#endif
      default: return NULL;
    }
  }
  return NULL;
}

// ---------- kd (arena only with array stores) ----------
template<template<int> class StoreD, typename Cmp>
static IFrontier* mkKdArr(size_t dim, bool arena) {
  if (arena) {
    switch (dim) {
      case 1: return new KDFrontierT<StoreD<1>, Cmp, true>(1);
      case 2: return new KDFrontierT<StoreD<2>, Cmp, true>(2);
      case 3: return new KDFrontierT<StoreD<3>, Cmp, true>(3);
      case 4: return new KDFrontierT<StoreD<4>, Cmp, true>(4);
      case 5: return new KDFrontierT<StoreD<5>, Cmp, true>(5);
      default: return NULL;
    }
  }
  switch (dim) {
    case 1: return new KDFrontierT<StoreD<1>, Cmp, false>(1);
    case 2: return new KDFrontierT<StoreD<2>, Cmp, false>(2);
    case 3: return new KDFrontierT<StoreD<3>, Cmp, false>(3);
    case 4: return new KDFrontierT<StoreD<4>, Cmp, false>(4);
    case 5: return new KDFrontierT<StoreD<5>, Cmp, false>(5);
    default: return NULL;
  }
}
template<typename Cmp>
static IFrontier* mkKdVec(size_t dim) { return new KDFrontierT<VecDouble, Cmp, false>(dim); }
static IFrontier* makeKd(size_t dim, const VariantCfg& c) {
  if (c.store == STORE_VEC) {
    if (c.arena) return NULL; // arena+vector is invalid (would not compile); never instantiate
    switch (c.cmp) {
      case CMP_NAIVE:      return mkKdVec<CmpNaive>(dim);
      case CMP_FAST:       return mkKdVec<CmpFast>(dim);
      case CMP_BRANCHLESS: return mkKdVec<CmpBranchless>(dim);
      default: return NULL;
    }
  }
  if (c.store == STORE_ARRD) {
    switch (c.cmp) {
      case CMP_NAIVE:      return mkKdArr<ArrDouble, CmpNaive>(dim, c.arena);
      case CMP_FAST:       return mkKdArr<ArrDouble, CmpFast>(dim, c.arena);
      case CMP_BRANCHLESS: return mkKdArr<ArrDouble, CmpBranchless>(dim, c.arena);
#ifdef __AVX2__
      case CMP_SIMD:       return mkKdArr<ArrDouble, CmpSimd>(dim, c.arena);
#endif
      default: return NULL;
    }
  }
  if (c.store == STORE_ARRI) {
    switch (c.cmp) {
      case CMP_NAIVE:      return mkKdArr<ArrInt, CmpNaive>(dim, c.arena);
      case CMP_FAST:       return mkKdArr<ArrInt, CmpFast>(dim, c.arena);
      case CMP_BRANCHLESS: return mkKdArr<ArrInt, CmpBranchless>(dim, c.arena);
#ifdef __AVX2__
      case CMP_SIMD:       return mkKdArr<ArrInt, CmpSimd>(dim, c.arena);
#endif
      default: return NULL;
    }
  }
  return NULL;
}

// ---------- kdinc (incremental k-d; same store/arena rules as kd) ----------
template<template<int> class StoreD, typename Cmp>
static IFrontier* mkKdIncArr(size_t dim, bool arena) {
  if (arena) {
    switch (dim) {
      case 1: return new KDIncFrontierT<StoreD<1>, Cmp, true>(1);
      case 2: return new KDIncFrontierT<StoreD<2>, Cmp, true>(2);
      case 3: return new KDIncFrontierT<StoreD<3>, Cmp, true>(3);
      case 4: return new KDIncFrontierT<StoreD<4>, Cmp, true>(4);
      case 5: return new KDIncFrontierT<StoreD<5>, Cmp, true>(5);
      default: return NULL;
    }
  }
  switch (dim) {
    case 1: return new KDIncFrontierT<StoreD<1>, Cmp, false>(1);
    case 2: return new KDIncFrontierT<StoreD<2>, Cmp, false>(2);
    case 3: return new KDIncFrontierT<StoreD<3>, Cmp, false>(3);
    case 4: return new KDIncFrontierT<StoreD<4>, Cmp, false>(4);
    case 5: return new KDIncFrontierT<StoreD<5>, Cmp, false>(5);
    default: return NULL;
  }
}
template<typename Cmp>
static IFrontier* mkKdIncVec(size_t dim) { return new KDIncFrontierT<VecDouble, Cmp, false>(dim); }
static IFrontier* makeKdInc(size_t dim, const VariantCfg& c) {
  if (c.store == STORE_VEC) {
    if (c.arena) return NULL; // arena+vector is invalid (would not compile); never instantiate
    switch (c.cmp) {
      case CMP_NAIVE:      return mkKdIncVec<CmpNaive>(dim);
      case CMP_FAST:       return mkKdIncVec<CmpFast>(dim);
      case CMP_BRANCHLESS: return mkKdIncVec<CmpBranchless>(dim);
      default: return NULL;
    }
  }
  if (c.store == STORE_ARRD) {
    switch (c.cmp) {
      case CMP_NAIVE:      return mkKdIncArr<ArrDouble, CmpNaive>(dim, c.arena);
      case CMP_FAST:       return mkKdIncArr<ArrDouble, CmpFast>(dim, c.arena);
      case CMP_BRANCHLESS: return mkKdIncArr<ArrDouble, CmpBranchless>(dim, c.arena);
#ifdef __AVX2__
      case CMP_SIMD:       return mkKdIncArr<ArrDouble, CmpSimd>(dim, c.arena);
#endif
      default: return NULL;
    }
  }
  if (c.store == STORE_ARRI) {
    switch (c.cmp) {
      case CMP_NAIVE:      return mkKdIncArr<ArrInt, CmpNaive>(dim, c.arena);
      case CMP_FAST:       return mkKdIncArr<ArrInt, CmpFast>(dim, c.arena);
      case CMP_BRANCHLESS: return mkKdIncArr<ArrInt, CmpBranchless>(dim, c.arena);
#ifdef __AVX2__
      case CMP_SIMD:       return mkKdIncArr<ArrInt, CmpSimd>(dim, c.arena);
#endif
      default: return NULL;
    }
  }
  return NULL;
}

// ---------- nd / range (no arena; ctor takes dim) ----------
template<template<typename,typename> class Tree, template<int> class StoreD, typename Cmp>
static IFrontier* mkTreeArr(size_t dim) {
  switch (dim) {
    case 1: return new Tree<StoreD<1>, Cmp>(1);
    case 2: return new Tree<StoreD<2>, Cmp>(2);
    case 3: return new Tree<StoreD<3>, Cmp>(3);
    case 4: return new Tree<StoreD<4>, Cmp>(4);
    case 5: return new Tree<StoreD<5>, Cmp>(5);
    default: return NULL;
  }
}
template<template<typename,typename> class Tree>
static IFrontier* makeTree(size_t dim, const VariantCfg& c) {
  if (c.store == STORE_VEC) {
    switch (c.cmp) {
      case CMP_NAIVE:      return new Tree<VecDouble, CmpNaive>(dim);
      case CMP_FAST:       return new Tree<VecDouble, CmpFast>(dim);
      case CMP_BRANCHLESS: return new Tree<VecDouble, CmpBranchless>(dim);
      default: return NULL;
    }
  }
  if (c.store == STORE_ARRD) {
    switch (c.cmp) {
      case CMP_NAIVE:      return mkTreeArr<Tree, ArrDouble, CmpNaive>(dim);
      case CMP_FAST:       return mkTreeArr<Tree, ArrDouble, CmpFast>(dim);
      case CMP_BRANCHLESS: return mkTreeArr<Tree, ArrDouble, CmpBranchless>(dim);
#ifdef __AVX2__
      case CMP_SIMD:       return mkTreeArr<Tree, ArrDouble, CmpSimd>(dim);
#endif
      default: return NULL;
    }
  }
  if (c.store == STORE_ARRI) {
    switch (c.cmp) {
      case CMP_NAIVE:      return mkTreeArr<Tree, ArrInt, CmpNaive>(dim);
      case CMP_FAST:       return mkTreeArr<Tree, ArrInt, CmpFast>(dim);
      case CMP_BRANCHLESS: return mkTreeArr<Tree, ArrInt, CmpBranchless>(dim);
#ifdef __AVX2__
      case CMP_SIMD:       return mkTreeArr<Tree, ArrInt, CmpSimd>(dim);
#endif
      default: return NULL;
    }
  }
  return NULL;
}

// ---------- avlfast (compare only) ----------
static IFrontier* makeAvlfast(const VariantCfg& c) {
  if (c.store != STORE_VEC || c.arena) return NULL; // storage/arena do not apply
  switch (c.cmp) {
    case CMP_NAIVE:      return new AVLFastFrontier();
    case CMP_FAST:       return new AVLFastCmpFrontier<CmpFast>();
    case CMP_BRANCHLESS: return new AVLFastCmpFrontier<CmpBranchless>();
    default: return NULL;
  }
}

IFrontier* MakeVariant(const std::string& name, size_t dim, const VariantCfg& c) {
  if (name == "linear")  return makeLinear(dim, c);
  if (name == "kd")      return makeKd(dim, c);
  if (name == "kdinc")   return makeKdInc(dim, c);
  if (name == "nd")      return makeTree<NDTreeFrontierT>(dim, c);
  if (name == "ndinc")   return makeTree<NDIncFrontierT>(dim, c);
  if (name == "range")   return makeTree<RangeTreeFrontierT>(dim, c);
  if (name == "avlfast") return makeAvlfast(c);
  return NULL;
}

std::vector<VariantCfg> AllVariants(const std::string& name, size_t dim) {
  (void)dim;
  std::vector<VariantCfg> v;
  bool ownsStorage = (name == "linear" || name == "kd" || name == "kdinc" || name == "nd" || name == "ndinc" || name == "range");
  v.push_back(VariantCfg(STORE_VEC, CMP_NAIVE, false));      // baseline
  v.push_back(VariantCfg(STORE_VEC, CMP_FAST, false));       // O1
  v.push_back(VariantCfg(STORE_VEC, CMP_BRANCHLESS, false)); // O5
  if (ownsStorage) {
    v.push_back(VariantCfg(STORE_ARRD, CMP_FAST, false));    // O2
    v.push_back(VariantCfg(STORE_ARRI, CMP_FAST, false));    // O6
#ifdef __AVX2__
    v.push_back(VariantCfg(STORE_ARRD, CMP_SIMD, false));    // O7 double
    v.push_back(VariantCfg(STORE_ARRI, CMP_SIMD, false));    // O7 int
#endif
  }
  if (name == "kd" || name == "kdinc") { // O3 arena (array stores only)
    v.push_back(VariantCfg(STORE_ARRD, CMP_FAST, true));
    v.push_back(VariantCfg(STORE_ARRI, CMP_FAST, true));
  }
  return v;
}

StoreId ParseStore(const std::string& s){ if(s=="arrd")return STORE_ARRD; if(s=="arri")return STORE_ARRI; return STORE_VEC; }
CmpId ParseCmp(const std::string& s){ if(s=="fast")return CMP_FAST; if(s=="bl")return CMP_BRANCHLESS; if(s=="simd")return CMP_SIMD; return CMP_NAIVE; }
const char* StoreName(StoreId s){ return s==STORE_ARRD?"arrd":s==STORE_ARRI?"arri":"vec"; }
const char* CmpName(CmpId c){ return c==CMP_FAST?"fast":c==CMP_BRANCHLESS?"bl":c==CMP_SIMD?"simd":"naive"; }
} }
