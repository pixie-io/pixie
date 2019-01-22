#include <glog/logging.h>
#include <string>

#include "absl/strings/str_format.h"
#include "src/carnot/plan/utils.h"

namespace pl {
namespace carnot {
namespace plan {

std::string ToString(planpb::OperatorType op) {
  switch (op) {
    case planpb::MEMORY_SOURCE_OPERATOR:
      return "MemorySourceOperator";
    case planpb::MAP_OPERATOR:
      return "MapOperator";
    case planpb::BLOCKING_AGGREGATE_OPERATOR:
      return "BlockingAggregateOperator";
    case planpb::MEMORY_SINK_OPERATOR:
      return "MemorySinkOperator";
    default:
      LOG(WARNING) << "Unknown operator in ToString function";
      return absl::StrFormat("(UnknownOperator:%d)", static_cast<int>(op));
  }
}

std::string ToString(carnotpb::DataType dt) {
  switch (dt) {
    case carnotpb::BOOLEAN:
      return "bool";
    case carnotpb::INT64:
      return "int64";
    case carnotpb::FLOAT64:
      return "float64";
    case carnotpb::STRING:
      return "string";
    default:
      LOG(WARNING) << "Unknown datatype in ToStringFunction";
      return absl::StrFormat("(UnknownDatatype:%d)", static_cast<int>(dt));
  }
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
