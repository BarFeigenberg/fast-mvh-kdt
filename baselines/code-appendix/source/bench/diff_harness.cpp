// source/bench/diff_harness.cpp
#include "bench/diff_harness.hpp"
#include "bench/linear_frontier.hpp"
#include "vec_type.hpp"
#include <sstream>
namespace rzq { namespace bench {
bool RunDiff(IFrontier* backend, const OpStream& s, std::string* err) {
  LinearFrontier oracle;
  for (size_t i = 0; i < s.size(); ++i) {
    const Op& op = s[i];
    if (op.type == OP_CHECK) {
      bool b = backend->Check(op.vec);
      bool o = oracle.Check(op.vec);
      if (b != o) {
        std::ostringstream ss;
        ss << "Check mismatch at op " << i << ": backend=" << b << " oracle=" << o;
        *err = ss.str();
        return false;
      }
    } else {
      backend->Update(op.vec);
      oracle.Update(op.vec);
      if (backend->Size() != oracle.Size()) {
        std::ostringstream ss;
        ss << "Size mismatch after op " << i << ": backend=" << backend->Size()
           << " oracle=" << oracle.Size();
        *err = ss.str();
        return false;
      }
    }
  }
  return true;
}
} }
