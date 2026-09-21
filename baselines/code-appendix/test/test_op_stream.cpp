// test/test_op_stream.cpp
#include "bench/op_stream.hpp"
#include "bench/test_util.hpp"
#include <cstdio>
using namespace rzq::bench;
int main() {
  OpStream s;
  s.push_back({OP_UPDATE, {1.0, 2.0}});
  s.push_back({OP_CHECK,  {3.0, 4.0}});
  const char* path = "build/_tmp_test.stream";
  BCHECK(WriteStream(path, s, 2), "write failed");
  OpStream r; size_t dim = 0;
  BCHECK(ReadStream(path, &r, &dim), "read failed");
  BCHECK(dim == 2, "dim roundtrip");
  BCHECK(r.size() == 2, "size roundtrip");
  BCHECK(r[0].type == OP_UPDATE && r[0].vec[1] == 2.0, "op0 roundtrip");
  BCHECK(r[1].type == OP_CHECK && r[1].vec[0] == 3.0, "op1 roundtrip");
  std::remove(path);
  return 0;
}
