// test/test_variants.cpp
#include "bench/variant_factory.hpp"
#include "bench/diff_harness.hpp"
#include "bench/test_util.hpp"
#include <cstdlib>
using namespace rzq::bench;
static OpStream RandStream(unsigned seed, size_t dim, int n) {
  std::srand(seed); OpStream s;
  for (int i=0;i<n;++i){ Op op; op.type=(std::rand()%3==0)?OP_UPDATE:OP_CHECK;
    for(size_t d=0;d<dim;++d) op.vec.push_back(std::rand()%20); s.push_back(op);} return s;
}
int main() {
  size_t dim = 3;
  const char* names[] = {"linear","avlfast","kd","nd","range"};
  for (int b = 0; b < 5; ++b) {
    std::vector<VariantCfg> cfgs = AllVariants(names[b], dim);
    BCHECK(!cfgs.empty(), "non-empty variant list");
    for (size_t i = 0; i < cfgs.size(); ++i) {
      IFrontier* f = MakeVariant(names[b], dim, cfgs[i]);
      BCHECK(f != NULL, "factory built an advertised variant");
      std::string err;
      BCHECK(RunDiff(f, RandStream(10 + i, dim, 400), &err), err.c_str());
      delete f;
    }
  }
  // explicit kd arena + array store builds and matches
  IFrontier* k = MakeVariant("kd", dim, VariantCfg(STORE_ARRD, CMP_FAST, true));
  BCHECK(k != NULL, "kd arena variant"); std::string e;
  BCHECK(RunDiff(k, RandStream(99, dim, 400), &e), e.c_str()); delete k;
  // invalid combo returns NULL (arena + vector for kd)
  BCHECK(MakeVariant("kd", dim, VariantCfg(STORE_VEC, CMP_FAST, true)) == NULL, "arena+vec invalid");
  return 0;
}
