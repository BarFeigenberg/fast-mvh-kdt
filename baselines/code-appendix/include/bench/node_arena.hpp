// include/bench/node_arena.hpp
#ifndef RZQ_BENCH_NODE_ARENA_H_
#define RZQ_BENCH_NODE_ARENA_H_
#include <vector>
#include <cstddef>
namespace rzq { namespace bench {
// Bump allocator: hands out T* from fixed-size blocks of raw storage; reset()
// reclaims all at once (no per-node free) and reuses existing blocks. Pointers
// are stable until reset() or destruction. T must be a POD-like node (fields
// assigned by the caller after alloc()); no constructor/destructor is run.
template<typename T>
class NodeArena {
public:
  NodeArena(size_t block = 4096)
    : _block(block), _blockIdx(0), _used(0), _count(0) {}
  ~NodeArena() {
    for (size_t i = 0; i < _blocks.size(); ++i) ::operator delete(_blocks[i]);
  }
  T* alloc() {
    if (_used == _block) { ++_blockIdx; _used = 0; }   // current block exhausted
    if (_blockIdx == _blocks.size()) {                 // need a fresh block
      _blocks.push_back((T*) ::operator new(_block * sizeof(T)));
    }
    T* p = _blocks[_blockIdx] + _used;
    ++_used; ++_count;
    return p;
  }
  void reset() { _blockIdx = 0; _used = 0; _count = 0; } // keep blocks, rewind
  size_t allocated() const { return _count; }
private:
  std::vector<T*> _blocks;
  size_t _block, _blockIdx, _used, _count;
};
} }
#endif
