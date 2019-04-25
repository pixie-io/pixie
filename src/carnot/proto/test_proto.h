#pragma once

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/substitute.h"
#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace carnotpb {
namespace testutils {

using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
/**
 * This files provides canonical test protos that
 * other parts of the project can use to provide "fakes" for the
 * plan.
 *
 * Protos in this file are always valid as they are not expected to be used for
 * error case testing.
 */

const char* kColumnReferencePbtxt = R"(
column {
  node: 0,
  index: 0,
})";

const char* kScalarBooleanValue = R"(
  type: BOOLEAN,
  value {
    bool_value: false
  }
)";

const char* kScalarInt64ValuePbtxt = R"(
constant {
    data_type: INT64,
    int64_value: 1337
})";

/*
 * Template for a ScalarFunc.
 * $1: A ScalarExpression representing the args used during evaluation.
 */
const char* kScalarFuncFIITmpl = R"(
  name: "testUDF"
  args: constant {

  }
  args: $1
)";

const char* kFuncWithTwoCols = R"(
name: "foobar"
args {
  column {
    node: 0
    index: 1
  }
}
args {
  column {
    node: 1
    index: 1
  }
}
args {
  constant {
    data_type: INT64
    int64_value: 36
  }
})";

/*
 * Template for an Operator.
 * $0: The type of Operator. See carnotpb::OperatorType.
 * $1: The operator field.
 * $2: The operator contents.
 */
const char* kOperatorProtoTmpl = R"(
op_type: $0
$1 {
  $2
})";

const char* kMemSourceOperator1 = R"(
name: "cpu"
column_idxs: 1
column_types: FLOAT64
column_names: "usage"
)";

const char* kMemSourceOperatorRange = R"(
name: "cpu"
start_time: {
 value: 3
}
stop_time: {
 value: 6
}
column_idxs: 1
column_types: FLOAT64
column_names: "usage"
)";

const char* kMemSourceOperatorEmptyRange = R"(
name: "cpu"
start_time: {
 value: 10
}
stop_time: {
 value: 20
}
column_idxs: 1
column_types: FLOAT64
column_names: "usage"
)";

const char* kMemSourceOperatorAllRange = R"(
name: "cpu"
start_time: {
 value: 3
}
stop_time: {
 value: 20
}
column_idxs: 1
column_types: FLOAT64
column_names: "usage"
)";

const char* kBlockingAggOperator1 = R"(
values {
  name: "testUdf"
  args {
    constant {
      data_type: BOOLEAN,
      bool_value: false
    }
  }
}
groups {
  node: 1
  index: 1
}
group_names: "group1"
value_names: "value1"
)";

const char* kFilterOperator1 = R"(
expression {
  func {
    name: "testUdf"
    args {
      constant {
        data_type: BOOLEAN,
        bool_value: false
      }
    }
  }
}
columns {
  node: 1
  index: 0
}
columns {
  node: 1
  index: 1
}
)";

const char* kMemSinkOperator1 = R"(
name: "cpu_15s"
column_names: "winagg_cpu0"
column_types: FLOAT64
)";

const char* kMemSinkOperator2 = R"(
name: "cpu_15s"
column_names: "test_col1"
column_types: INT64
column_names: "test_col2"
column_types: BOOLEAN
)";

const char* kMapOperator1 = R"(
expressions {
  func {
    name: "testUdf"
    args {
      constant {
        data_type: BOOLEAN,
        bool_value: false
      }
    }
    args {
      column {
        node: 1
        index: 1
      }
    }
  }
}
column_names: "col1"
)";

const char* kLimitOperator1 = R"(
limit: 10
columns {
  node: 1
  index: 0
}
columns {
  node: 1
  index: 1
}
)";

/**
 * Template for Map Operator.
 *   $0 : the expressions
 */
const char* kMapOperatorTmpl = R"(
expressions {
  $0
}
column_names: "col1"
)";

/*
 * Template for a ScalarExpression.
 * $0: The type of ScalarExpression. (constant|func|column)
 * $1: The contents of the ScalarExpression. Either a ScalarValue, Column, or ScalarFunc.
 */
const char* kScalarExpressionTmpl = R"(
  $0 {
    $1
  }
)";

const char* kAddScalarFuncPbtxt = R"(
func {
  name: "add"
  args {
    column {
      node: 0
      index: 0
    }
  }
  args {
    column {
      node: 0
      index: 1
    }
  }
})";

const char* kAddScalarFuncConstPbtxt = R"(
func {
  name: "add"
  args {
    column {
      node: 0
      index: 0
    }
  }
  args {
    constant {
      data_type: INT64,
      int64_value: 1337
    }
  }
})";

const char* kAddScalarFuncNestedPbtxt = R"(
func {
  name: "add"
  args {
    column {
      node: 0
      index: 0
    }
  }
  args {
    func {
      name: "add"
      args {
        column {
          node: 0
          index: 1
        }
      }
      args {
        constant {
          data_type: INT64,
          int64_value: 1337
        }
      }
    }
  }
})";

const char* kPlanFragmentWithFourNodes = R"(
  id: 1,
  dag {
    nodes {
      id: 1
      sorted_deps: 2
      sorted_deps: 3
    }
    nodes {
      id: 2
      sorted_deps: 5
    }
    nodes {
      id: 3
      sorted_deps: 5
    }
    nodes {
      id: 5
    }
  }
  nodes {
    id: 1
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "mem_source"
        column_types: INT64
        column_names: "test"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          constant {
            data_type: INT64
            int64_value: 1
          }
        }
        column_names: "test"
      }
    }
  }
  nodes {
    id: 3
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          constant {
            data_type: INT64
            int64_value: 1
          }
        }
        column_names: "test2"
      }
    }
  }
  nodes {
    id: 5
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "mem_sink"
        column_types: INT64
        column_names: "test3"
      }
    }
  }
)";

const char* kLinearPlanFragment = R"(
  id: 1,
  dag {
    nodes {
      id: 1
      sorted_deps: 2
    }
    nodes {
      id: 2
      sorted_deps: 3
    }
    nodes {
      id: 3
      sorted_deps: 4
    }
    nodes {
      id: 4
    }
  }
  nodes {
    id: 1
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "numbers"
        column_idxs: 0
        column_types: INT64
        column_names: "a"
        column_idxs: 1
        column_types: BOOLEAN
        column_names: "b"
        column_idxs: 2
        column_types: FLOAT64
        column_names: "c"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          func {
            name: "add"
            id: 0
            args {
              column {
                node: 1
                index: 0
              }
            }
            args {
              column {
                node: 1
                index: 2
              }
            }
          }
        }
        column_names: "summed"
      }
    }
  }
  nodes {
    id: 3
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          func {
            id: 1
            name: "multiply"
            args {
              column {
                node: 2
                index: 0
              }
            }
            args {
              constant {
                data_type: INT64
                int64_value: 2
              }
            }
          }
        }
        column_names: "mult"
      }
    }
  }
  nodes {
    id: 4
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "output"
        column_types: FLOAT64
        column_names: "res"
      }
    }
  }
)";

const char* kPlanWithFiveNodes = R"(
  dag {
    nodes {
      id: 1
      sorted_deps: 2
      sorted_deps: 3
    }
    nodes {
      id: 2
      sorted_deps: 4
    }
    nodes {
      id: 3
      sorted_deps: 4
    }
    nodes {
      id: 4
      sorted_deps: 5
    }
    nodes {
      id: 5
    }
  }
  nodes {
    id: 1
  }
  nodes {
    id: 2
  }
  nodes {
    id: 3
  }
  nodes {
    id: 4
  }
  nodes {
    id: 5
  }
)";

carnotpb::Operator CreateTestMap1PB() {
  carnotpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MAP_OPERATOR", "map_op", kMapOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestMapAddTwoCols() {
  carnotpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MAP_OPERATOR", "map_op",
                                   absl::Substitute(kMapOperatorTmpl, kAddScalarFuncPbtxt));
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestSource1PB() {
  carnotpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SOURCE_OPERATOR", "mem_source_op",
                                   kMemSourceOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestSourceRangePB() {
  carnotpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SOURCE_OPERATOR", "mem_source_op",
                                   kMemSourceOperatorRange);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestSourceEmptyRangePB() {
  carnotpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SOURCE_OPERATOR", "mem_source_op",
                                   kMemSourceOperatorEmptyRange);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestSourceAllRangePB() {
  carnotpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SOURCE_OPERATOR", "mem_source_op",
                                   kMemSourceOperatorAllRange);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestSink1PB() {
  carnotpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SINK_OPERATOR", "mem_sink_op",
                                   kMemSinkOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestSink2PB() {
  carnotpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SINK_OPERATOR", "mem_sink_op",
                                   kMemSinkOperator2);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestBlockingAgg1PB() {
  carnotpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "BLOCKING_AGGREGATE_OPERATOR",
                                   "blocking_agg_op", kBlockingAggOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestFilter1PB() {
  carnotpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "FILTER_OPERATOR", "filter_op", kFilterOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::Operator CreateTestLimit1PB() {
  carnotpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "LIMIT_OPERATOR", "limit_op", kLimitOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

carnotpb::ScalarFunc CreateTestFuncWithTwoColsPB() {
  carnotpb::ScalarFunc func;
  CHECK(google::protobuf::TextFormat::MergeFromString(kFuncWithTwoCols, &func))
      << "Failed to parse proto";
  return func;
}

carnotpb::ScalarExpression CreateTestScalarExpressionWithConstBooleanPB() {
  carnotpb::ScalarExpression exp;
  auto exp_proto = absl::Substitute(kScalarExpressionTmpl, "constant", kScalarBooleanValue);
  CHECK(google::protobuf::TextFormat::MergeFromString(exp_proto, &exp)) << "Failed to parse proto";
  return exp;
}

carnotpb::ScalarExpression CreateTestScalarExpressionWithConstInt64PB() {
  carnotpb::ScalarExpression exp;
  CHECK(google::protobuf::TextFormat::MergeFromString(kScalarInt64ValuePbtxt, &exp))
      << "Failed to parse proto";
  return exp;
}

carnotpb::ScalarExpression CreateTestScalarExpressionWithFunc1PB() {
  carnotpb::ScalarExpression exp;
  auto exp_proto = absl::Substitute(kScalarExpressionTmpl, "func", kFuncWithTwoCols);
  CHECK(google::protobuf::TextFormat::MergeFromString(exp_proto, &exp)) << "Failed to parse proto";
  return exp;
}
const FieldDescriptor* GetFieldDescriptor(const google::protobuf::Message& message,
                                          const std::string& field_name) {
  std::vector<std::string> field_path = absl::StrSplit(field_name, ".");
  const Descriptor* descriptor = message.GetDescriptor();
  const FieldDescriptor* field = NULL;
  for (size_t i = 0; i < field_path.size(); i++) {
    field = descriptor->FindFieldByName(field_path[i]);
    descriptor = field->message_type();
  }
  return field;
}
bool CompareLogicalPlans(const carnotpb::Plan& expected_plan, const carnotpb::Plan& actual_plan,
                         bool ignore_ids) {
  google::protobuf::util::MessageDifferencer differ;
  if (ignore_ids) {
    differ.IgnoreField(GetFieldDescriptor(expected_plan, "dag"));
    differ.IgnoreField(GetFieldDescriptor(expected_plan, "nodes.dag"));
    differ.IgnoreField(GetFieldDescriptor(expected_plan, "nodes.id"));
    differ.IgnoreField(GetFieldDescriptor(expected_plan, "nodes.nodes.id"));
  }
  return differ.Compare(expected_plan, actual_plan);
}

}  // namespace testutils
}  // namespace carnotpb
}  // namespace carnot
}  // namespace pl
