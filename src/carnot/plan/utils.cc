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

std::string ToString(planpb::DataType dt) {
  switch (dt) {
    case planpb::BOOLEAN:
      return "bool";
    case planpb::INT64:
      return "int64";
    case planpb::FLOAT64:
      return "float64";
    case planpb::STRING:
      return "string";
    default:
      LOG(WARNING) << "Unknown datatype in ToStringFunction";
      return absl::StrFormat("(UnknownDatatype:%d)", static_cast<int>(dt));
  }
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
