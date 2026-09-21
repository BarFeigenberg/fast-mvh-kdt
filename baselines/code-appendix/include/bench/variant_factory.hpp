// include/bench/variant_factory.hpp
#ifndef RZQ_BENCH_VARIANT_FACTORY_H_
#define RZQ_BENCH_VARIANT_FACTORY_H_
#include <string>
#include <vector>
#include "bench/ifrontier.hpp"
namespace rzq { namespace bench {
enum StoreId { STORE_VEC = 0, STORE_ARRD = 1, STORE_ARRI = 2 };
enum CmpId   { CMP_NAIVE = 0, CMP_FAST = 1, CMP_BRANCHLESS = 2, CMP_SIMD = 3 };
struct VariantCfg {
  StoreId store; CmpId cmp; bool arena;
  VariantCfg(StoreId s = STORE_VEC, CmpId c = CMP_NAIVE, bool a = false)
    : store(s), cmp(c), arena(a) {}
};
// Build a backend variant; returns NULL for invalid combos (e.g. SIMD+VEC, kd arena+VEC).
IFrontier* MakeVariant(const std::string& name, size_t dim, const VariantCfg& cfg);
// The set of variants to ablate for a backend.
std::vector<VariantCfg> AllVariants(const std::string& name, size_t dim);
StoreId ParseStore(const std::string&);
CmpId ParseCmp(const std::string&);
const char* StoreName(StoreId); const char* CmpName(CmpId);
} }
#endif
