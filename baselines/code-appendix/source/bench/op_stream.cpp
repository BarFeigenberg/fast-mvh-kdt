// source/bench/op_stream.cpp
#include "bench/op_stream.hpp"
#include <fstream>
#include <sstream>
namespace rzq { namespace bench {
bool WriteStream(const std::string& path, const OpStream& s, size_t dim) {
  std::ofstream f(path.c_str());
  if (!f) return false;
  f << "DIM " << dim << "\n";
  for (size_t i = 0; i < s.size(); ++i) {
    f << (s[i].type == OP_CHECK ? 'C' : 'U');
    for (size_t d = 0; d < s[i].vec.size(); ++d) f << ' ' << s[i].vec[d];
    f << "\n";
  }
  return true;
}
bool ReadStream(const std::string& path, OpStream* out, size_t* dim) {
  std::ifstream f(path.c_str());
  if (!f) return false;
  std::string line;
  if (!std::getline(f, line)) return false;
  { std::istringstream hs(line); std::string tag; hs >> tag >> *dim;
    if (tag != "DIM") return false; }
  out->clear();
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    std::istringstream ls(line);
    char c; ls >> c;
    Op op; op.type = (c == 'C') ? OP_CHECK : OP_UPDATE;
    double x; while (ls >> x) op.vec.push_back(x);
    out->push_back(op);
  }
  return true;
}
} }
