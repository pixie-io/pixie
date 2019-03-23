#include <string>

#include "absl/strings/str_format.h"
#include "src/carnot/plan/utils.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace plan {

std::string ToString(carnotpb::OperatorType op) {
  switch (op) {
    case carnotpb::MEMORY_SOURCE_OPERATOR:
      return "MemorySourceOperator";
    case carnotpb::MAP_OPERATOR:
      return "MapOperator";
    case carnotpb::BLOCKING_AGGREGATE_OPERATOR:
      return "BlockingAggregateOperator";
    case carnotpb::MEMORY_SINK_OPERATOR:
      return "MemorySinkOperator";
    default:
      LOG(WARNING) << "Unknown operator in ToString function";
      return absl::StrFormat("(UnknownOperator:%d)", static_cast<int>(op));
  }
}

// PL_CARNOT_UPDATE_FOR_NEW_TYPES
std::string ToString(types::DataType dt) {
  switch (dt) {
    case types::BOOLEAN:
      return "bool";
    case types::INT64:
      return "int64";
    case types::FLOAT64:
      return "float64";
    case types::STRING:
      return "string";
    case types::TIME64NS:
      return "time64ns";
    default:
      LOG(WARNING) << "Unknown datatype in ToStringFunction";
      return absl::StrFormat("(UnknownDatatype:%d)", static_cast<int>(dt));
  }
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
