#include <gtest/gtest.h>

#include <algorithm>
#include <unordered_map>

#include "src/carnot/exec/blocking_agg_node.h"
#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/proto/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/shared/types/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace exec {

using testing::_;
using types::Int64Value;
using udf::FunctionContext;

// Test UDA, takes the min of two arguments and then sums them.
// TODO(zasgar): move these all to a common file.
class MinSumUDA : public udf::UDA {
 public:
  void Update(udf::FunctionContext*, types::Int64Value arg1, types::Int64Value arg2) {
    sum_ = sum_.val + std::min(arg1.val, arg2.val);
  }
  void Merge(udf::FunctionContext*, const MinSumUDA& other) { sum_ = sum_.val + other.sum_.val; }
  types::Int64Value Finalize(udf::FunctionContext*) { return sum_; }

 protected:
  types::Int64Value sum_ = 0;
};

const char* kBlockingNoGroupAgg = R"(
op_type: BLOCKING_AGGREGATE_OPERATOR
blocking_agg_op {
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  value_names: "value1"
})";

const char* kBlockingSingleGroupAgg = R"(
op_type: BLOCKING_AGGREGATE_OPERATOR
blocking_agg_op {
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  groups {
     node: 0
     index: 0
  }
  group_names: "g1"
  value_names: "value1"
})";

const char* kBlockingMultipleGroupAgg = R"(
op_type: BLOCKING_AGGREGATE_OPERATOR
blocking_agg_op {
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 2
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  groups {
     node: 0
     index: 0
  }
  groups {
     node: 0
     index: 1
  }
  group_names: "g1"
  group_names: "g2"
  value_names: "value1"
})";

std::unique_ptr<ExecState> MakeTestExecState(udf::ScalarUDFRegistry* udf_registry,
                                             udf::UDARegistry* uda_registry) {
  auto table_store = std::make_shared<TableStore>();
  return std::make_unique<ExecState>(udf_registry, uda_registry, table_store);
}

std::unique_ptr<plan::Operator> PlanNodeFromPbtxt(const std::string& pbtxt) {
  carnotpb::Operator op_pb;
  EXPECT_TRUE(google::protobuf::TextFormat::MergeFromString(pbtxt, &op_pb));
  return plan::BlockingAggregateOperator::FromProto(op_pb, 1);
}

class BlockingAggNodeTest : public ::testing::Test {
 public:
  BlockingAggNodeTest() {
    udf_registry_ = std::make_unique<udf::ScalarUDFRegistry>("test");
    uda_registry_ = std::make_unique<udf::UDARegistry>("test_uda");
    EXPECT_TRUE(uda_registry_->Register<MinSumUDA>("minsum").ok());

    exec_state_ = MakeTestExecState(udf_registry_.get(), uda_registry_.get());
  }

 protected:
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::ScalarUDFRegistry> udf_registry_;
  std::unique_ptr<udf::UDARegistry> uda_registry_;
};

TEST_F(BlockingAggNodeTest, no_groups) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingNoGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<BlockingAggNode, plan::BlockingAggregateOperator>(
      *plan_node.get(), output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({2, 5, 6, 8})
                       .get(),
                   false)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get())
      .ExpectRowBatch(
          RowBatchBuilder(output_rd, 1, true).AddColumn<types::Int64Value>({Int64Value(23)}).get(),
          false)
      .Close();
}

TEST_F(BlockingAggNodeTest, single_group) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingSingleGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<BlockingAggNode, plan::BlockingAggregateOperator>(
      *plan_node.get(), output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, false)
                       .AddColumn<types::Int64Value>({1, 1, 2, 2})
                       .AddColumn<types::Int64Value>({2, 3, 3, 1})
                       .get(),
                   false)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({2, 3, 3, 4, 1, 5})
                          .get(),
                      false)
      .Close();
}

TEST_F(BlockingAggNodeTest, multiple_groups) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingMultipleGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<BlockingAggNode, plan::BlockingAggregateOperator>(
      *plan_node.get(), output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, false)
                       .AddColumn<types::Int64Value>({1, 5, 1, 2})
                       .AddColumn<types::Int64Value>({2, 1, 3, 1})
                       .AddColumn<types::Int64Value>({2, 5, 3, 1})
                       .get(),
                   false)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true)
                       .AddColumn<types::Int64Value>({5, 1, 3, 3})
                       .AddColumn<types::Int64Value>({1, 2, 3, 3})
                       .AddColumn<types::Int64Value>({1, 3, 3, 8})
                       .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, true)
                          .AddColumn<types::Int64Value>({1, 1, 2, 5, 3})
                          .AddColumn<types::Int64Value>({2, 3, 1, 1, 3})
                          .AddColumn<types::Int64Value>({4, 3, 1, 2, 6})
                          .get(),
                      false)
      .Close();
}

TEST_F(BlockingAggNodeTest, multiple_groups_with_string) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingMultipleGroupAgg);
  RowDescriptor input_rd({types::DataType::STRING, types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd(
      {types::DataType::STRING, types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<BlockingAggNode, plan::BlockingAggregateOperator>(
      *plan_node.get(), output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, false)
                       .AddColumn<types::StringValue>({"abc", "def", "abc", "fgh"})
                       .AddColumn<types::Int64Value>({2, 1, 3, 1})
                       .AddColumn<types::Int64Value>({2, 5, 3, 1})
                       .get(),
                   false)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true)
                       .AddColumn<types::StringValue>({"ijk", "abc", "abc", "def"})
                       .AddColumn<types::Int64Value>({1, 2, 3, 3})
                       .AddColumn<types::Int64Value>({1, 3, 3, 8})
                       .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, true)
                          .AddColumn<types::StringValue>({"abc", "def", "abc", "fgh", "ijk", "def"})
                          .AddColumn<types::Int64Value>({2, 1, 3, 1, 1, 3})
                          .AddColumn<types::Int64Value>({4, 1, 6, 1, 1, 3})
                          .get(),
                      false)
      .Close();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
