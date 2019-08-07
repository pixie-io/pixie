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
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace planpb {
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
}
args_data_types: INT64
args_data_types: INT64
args_data_types: INT64
)";

/*
 * Template for an Operator.
 * $0: The type of Operator. See planpb::OperatorType.
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
windowed: false
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

const char* kWindowedAggOperator1 = R"(
windowed: true
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
    args_data_types: BOOLEAN
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

const char* kGrpcSourceOperator1 = R"(
source_id: "agent1_2"
)";

const char* kGrpcSinkOperator1 = R"(
address: "localhost:1234"
destination_id: "agent1_2"
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
    args_data_types: BOOLEAN
    args_data_types: BOOLEAN
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

// relation 1: [abc, time_]
// relation 2: [time_, abc]
// maps to output relation:
const char* kUnionOperatorOrdered = R"(
  rows_per_batch: 5
  column_names: "abc"
  column_names: "time_"
  column_mappings {
    has_time_column: true
    time_column_index: 1
    column_indexes: 0
    column_indexes: 1
  }
  column_mappings {
    has_time_column: true
    time_column_index: 0
    column_indexes: 1
    column_indexes: 0
  }
)";

const char* kUnionOperatorUnordered = R"(
  column_names: "abc"
  column_names: "xyz"
  column_mappings {
    has_time_column: false
    column_indexes: 0
    column_indexes: 1
  }
  column_mappings {
    has_time_column: false
    column_indexes: 1
    column_indexes: 0
  }
)";

const char* kUnionOperatorOutOfRange1 = R"(
  rows_per_batch: 3
  column_names: "abc"
  column_names: "time_"
  column_mappings {
    has_time_column: true
    time_column_index: 1
    column_indexes: 0
  }
  column_mappings {
    has_time_column: true
    time_column_index: 0
    column_indexes: 1
  }
)";

const char* kUnionOperatorOutOfRange2 = R"(
  column_names: "abc"
  column_names: "time_"
  column_mappings {
    has_time_column: true
    time_column_index: 1
    column_indexes: 0
    column_indexes: 1
    column_indexes: 2
  }
  column_mappings {
    has_time_column: true
    time_column_index: 0
    column_indexes: 1
    column_indexes: 2
    column_indexes: 3
  }
)";

const char* kUnionOperatorMismatched = R"(
  column_names: "abc"
  column_names: "xyz"
  column_mappings {
    has_time_column: false
    column_indexes: 0
    column_indexes: 1
  }
  column_mappings {
    has_time_column: true
    time_column_index: 1
    column_indexes: 1
    column_indexes: 2
  }
)";

const char* kJoinOperator1 = R"(
  type: INNER
  equality_conditions {
    left_column_index: 0
    right_column_index: 1
  }
  output_columns: {
    parent_index: 0
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 0
  }
  column_names: "abc"
  column_names: "time_"
  rows_per_batch: 10
)";

const char* kJoinOperatorNoTime1 = R"(
  type: INNER
  equality_conditions {
    left_column_index: 0
    right_column_index: 1
  }
  output_columns: {
    parent_index: 0
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 0
  }
  column_names: "abc"
  column_names: "def"
  rows_per_batch: 10
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

/**
 * Template for Map Operator.
 *   $0 : the expression
 */
const char* kFilterOperatorTmpl = R"(
expression {
  $0
}
columns {
  node: 0
  index: 0
}
columns {
  node: 0
  index: 1
}
columns {
  node: 0
  index: 2
}
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
  args_data_types: INT64
  args_data_types: INT64
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
  args_data_types: INT64
  args_data_types: INT64
})";

const char* kEq1ScalarFuncConstPbtxt = R"(
func {
  name: "eq"
  args {
    column {
      node: 0
      index: 0
    }
  }
  args {
    constant {
      data_type: INT64,
      int64_value: 1
    }
  }
  args_data_types: INT64
  args_data_types: INT64
})";

const char* kColValueScalarFuncConstPbtxt = R"(
column {
  node: 0
  index: 0
})";

const char* kStrEqAScalarFuncConstPbtxt = R"(
func {
  name: "eq"
  id: 1
  args {
    column {
      node: 0
      index: 0
    }
  }
  args {
    constant {
      data_type: STRING,
      string_value: "A"
    }
  }
  args_data_types: STRING
  args_data_types: STRING
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
      args_data_types: FLOAT64
      args_data_types: INT64
    }
  }
  args_data_types: FLOAT64
  args_data_types: FLOAT64
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
            args_data_types: INT64
            args_data_types: FLOAT64
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
            args_data_types: FLOAT64
            args_data_types: INT64
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

planpb::Operator CreateTestMap1PB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MAP_OPERATOR", "map_op", kMapOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestMapAddTwoCols() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MAP_OPERATOR", "map_op",
                                   absl::Substitute(kMapOperatorTmpl, kAddScalarFuncPbtxt));
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestFilterTwoCols() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "FILTER_OPERATOR", "filter_op",
                                   absl::Substitute(kFilterOperatorTmpl, kEq1ScalarFuncConstPbtxt));
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestFilterTwoColsString() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "FILTER_OPERATOR", "filter_op",
                       absl::Substitute(kFilterOperatorTmpl, kStrEqAScalarFuncConstPbtxt));
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestSource1PB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SOURCE_OPERATOR", "mem_source_op",
                                   kMemSourceOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestSourceRangePB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SOURCE_OPERATOR", "mem_source_op",
                                   kMemSourceOperatorRange);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestSourceEmptyRangePB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SOURCE_OPERATOR", "mem_source_op",
                                   kMemSourceOperatorEmptyRange);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestSourceAllRangePB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SOURCE_OPERATOR", "mem_source_op",
                                   kMemSourceOperatorAllRange);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestSink1PB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SINK_OPERATOR", "mem_sink_op",
                                   kMemSinkOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestSink2PB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "MEMORY_SINK_OPERATOR", "mem_sink_op",
                                   kMemSinkOperator2);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestGrpcSource1PB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "GRPC_SOURCE_OPERATOR", "grpc_source_op",
                                   kGrpcSourceOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestGrpcSink1PB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "GRPC_SINK_OPERATOR", "grpc_sink_op",
                                   kGrpcSinkOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestBlockingAgg1PB() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "AGGREGATE_OPERATOR", "agg_op", kBlockingAggOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestWindowedAgg1PB() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "AGGREGATE_OPERATOR", "agg_op", kWindowedAggOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestFilter1PB() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "FILTER_OPERATOR", "filter_op", kFilterOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestUnionOrderedPB() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "UNION_OPERATOR", "union_op", kUnionOperatorOrdered);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestUnionUnorderedPB() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "UNION_OPERATOR", "union_op", kUnionOperatorUnordered);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestUnionOutOfRange1() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "UNION_OPERATOR", "union_op", kUnionOperatorOutOfRange1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestUnionOutOfRange2() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "UNION_OPERATOR", "union_op", kUnionOperatorOutOfRange2);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestUnionMismatched() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "UNION_OPERATOR", "union_op", kUnionOperatorMismatched);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestLimit1PB() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "LIMIT_OPERATOR", "limit_op", kLimitOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestJoinWithTimePB() {
  planpb::Operator op;
  auto op_proto = absl::Substitute(kOperatorProtoTmpl, "JOIN_OPERATOR", "join_op", kJoinOperator1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::Operator CreateTestJoinNoTimePB() {
  planpb::Operator op;
  auto op_proto =
      absl::Substitute(kOperatorProtoTmpl, "JOIN_OPERATOR", "join_op", kJoinOperatorNoTime1);
  CHECK(google::protobuf::TextFormat::MergeFromString(op_proto, &op)) << "Failed to parse proto";
  return op;
}

planpb::ScalarFunc CreateTestFuncWithTwoColsPB() {
  planpb::ScalarFunc func;
  CHECK(google::protobuf::TextFormat::MergeFromString(kFuncWithTwoCols, &func))
      << "Failed to parse proto";
  return func;
}

planpb::ScalarExpression CreateTestScalarExpressionWithConstBooleanPB() {
  planpb::ScalarExpression exp;
  auto exp_proto = absl::Substitute(kScalarExpressionTmpl, "constant", kScalarBooleanValue);
  CHECK(google::protobuf::TextFormat::MergeFromString(exp_proto, &exp)) << "Failed to parse proto";
  return exp;
}

planpb::ScalarExpression CreateTestScalarExpressionWithConstInt64PB() {
  planpb::ScalarExpression exp;
  CHECK(google::protobuf::TextFormat::MergeFromString(kScalarInt64ValuePbtxt, &exp))
      << "Failed to parse proto";
  return exp;
}

planpb::ScalarExpression CreateTestScalarExpressionWithFunc1PB() {
  planpb::ScalarExpression exp;
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
bool CompareLogicalPlans(const planpb::Plan& expected_plan, const planpb::Plan& actual_plan,
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
}  // namespace planpb
}  // namespace carnot
}  // namespace pl
