// test/test_node_arena.cpp
#include "bench/node_arena.hpp"
#include "bench/test_util.hpp"
using namespace rzq::bench;
struct N { int v; N* l; N* r; };
int main() {
  NodeArena<N> a;
  N* x = a.alloc(); x->v = 5;
  N* y = a.alloc(); y->v = 7;
  BCHECK(x != y, "distinct nodes");
  BCHECK(x->v == 5 && y->v == 7, "writable");
  size_t before = a.allocated();
  BCHECK(before == 2, "count");
  a.reset();                 // frees all at once
  BCHECK(a.allocated() == 0, "reset clears");
  N* z = a.alloc();          // reuse after reset
  BCHECK(z != NULL, "alloc after reset");
  a.reset();                 // clear z's allocation before the fill/reset cycles

  for (int cycle = 0; cycle < 1000; ++cycle) {
    for (int k = 0; k < 5000; ++k) { N* p = a.alloc(); p->v = k; }
    BCHECK(a.allocated() == 5000, "count after fill");
    a.reset();
    BCHECK(a.allocated() == 0, "count after reset");
  }
  return 0;
}
