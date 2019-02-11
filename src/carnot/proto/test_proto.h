#pragma once

#include <glog/logging.h>
#include <google/protobuf/text_format.h>
#include <memory>

#include "absl/strings/substitute.h"
#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/common/status.h"

namespace pl {
namespace carnot {
namespace carnotpb {
namespace testutils {

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

}  // namespace testutils
}  // namespace carnotpb
}  // namespace carnot
}  // namespace pl
