// test/test_capture.cpp
#include "graph_io.hpp"
#include "search_emoa.hpp"
#include "bench/op_stream.hpp"
#include "bench/test_util.hpp"
#include <cstdlib>
using namespace rzq;
int main() {
#ifdef _WIN32
  _putenv_s("EMOA_CAPTURE", "build/_cap.stream");
#else
  setenv("EMOA_CAPTURE", "build/_cap.stream", 1);
#endif
  std::vector<std::string> fn;
  fn.push_back("data/ex1-c1.gr");
  fn.push_back("data/ex1-c2.gr");
  fn.push_back("data/ex1-c3.gr");
  basic::SparseGraph g;
  BCHECK(basic::LoadSparseGraphDIMAC(fn, &g) >= 0, "load graph");
  search::EMOAResult res;
  search::RunEMOA(&g, 1, 5, 60.0, &res);
  bench::OpStream s; size_t dim = 0;
  BCHECK(bench::ReadStream("build/_cap.stream", &s, &dim), "capture file exists");
  BCHECK(dim == 2, "projected dim = M-1 = 2");
  BCHECK(s.size() > 0, "captured some ops");
  return 0;
}
