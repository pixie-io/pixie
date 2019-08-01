#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <sole.hpp>
#include <vector>

#include "src/common/base/base.h"

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/map_node.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/shared/types/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using testing::_;
using types::Int64Value;
using udf::FunctionContext;

// TOOD(zasgar): refactor these into test udfs.
class AddUDF : public udf::ScalarUDF {
 public:
  types::Int64Value Exec(FunctionContext*, types::Int64Value v1, types::Int64Value v2) {
    return v1.val + v2.val;
  }
};

class MapNodeTest : public ::testing::Test {
 public:
  MapNodeTest() {
    auto op_proto = planpb::testutils::CreateTestMapAddTwoCols();
    plan_node_ = plan::MapOperator::FromProto(op_proto, 1);

    udf_registry_ = std::make_unique<udf::ScalarUDFRegistry>("test_registry");
    uda_registry_ = std::make_unique<udf::UDARegistry>("test_registry");
    EXPECT_OK(udf_registry_->Register<AddUDF>("add"));
    auto table_store = std::make_shared<TableStore>();
    auto row_batch_queue = std::make_shared<RowBatchQueue>();

    exec_state_ = std::make_unique<ExecState>(udf_registry_.get(), uda_registry_.get(), table_store,
                                              row_batch_queue, sole::uuid4());
    EXPECT_OK(exec_state_->AddScalarUDF(
        0, "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::INT64})));
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::UDARegistry> uda_registry_;
  std::unique_ptr<udf::ScalarUDFRegistry> udf_registry_;
};

TEST_F(MapNodeTest, basic) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<MapNode, plan::MapOperator>(*plan_node_, output_rd, {},
                                                                 exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, false, false)
                          .AddColumn<types::Int64Value>({2, 5, 9, 13})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 3, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3})
                       .AddColumn<types::Int64Value>({1, 4, 6})
                       .get(),
                   0)
      .ExpectRowBatch(
          RowBatchBuilder(output_rd, 3, true, true).AddColumn<types::Int64Value>({2, 6, 9}).get())
      .Close();
}

TEST_F(MapNodeTest, child_fail) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<MapNode, plan::MapOperator>(*plan_node_, output_rd, {},
                                                                 exec_state_.get());
  tester.ConsumeNextShouldFail(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                                   .AddColumn<types::Int64Value>({1, 2, 3, 4})
                                   .AddColumn<types::Int64Value>({1, 3, 6, 9})
                                   .get(),
                               0, error::InvalidArgument("args"));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
