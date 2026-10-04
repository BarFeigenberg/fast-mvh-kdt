// include/bench/op_stream.hpp
#ifndef RZQ_BENCH_OP_STREAM_H_
#define RZQ_BENCH_OP_STREAM_H_
#include <vector>
#include <string>
#include "bench/ifrontier.hpp"
namespace rzq { namespace bench {
enum OpType { OP_CHECK = 0, OP_UPDATE = 1 };
struct Op { OpType type; CostVec vec; };
typedef std::vector<Op> OpStream;
// Defined in a later task (Task 9):
bool WriteStream(const std::string& path, const OpStream& s, size_t dim);
bool ReadStream(const std::string& path, OpStream* out, size_t* dim);
} }
#endif
