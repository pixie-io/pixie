#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "src/common/error.h"

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/map_node.h"
#include "src/carnot/proto/test_proto.h"
#include "src/carnot/udf/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace exec {

using testing::_;
using udf::FunctionContext;
using udf::Int64Value;

// TOOD(zasgar): refactor these into test udfs.
class AddUDF : public udf::ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, Int64Value v1, Int64Value v2) { return v1.val + v2.val; }
};

class MapNodeTest : public ::testing::Test {
 public:
  MapNodeTest() {
    auto op_proto = carnotpb::testutils::CreateTestMapAddTwoCols();
    plan_node_ = plan::MapOperator::FromProto(op_proto, 1);

    auto udf_registry = std::make_shared<udf::ScalarUDFRegistry>("test_registry");
    auto uda_registry = std::make_shared<udf::UDARegistry>("test_registry");
    auto table_store = std::make_shared<TableStore>();

    EXPECT_TRUE(udf_registry->Register<AddUDF>("add").ok());
    exec_state_ = std::make_unique<ExecState>(udf_registry, uda_registry, table_store);
  }
  RowBatch CreateInputRowBatch(const std::vector<udf::Int64Value>& in1,
                               const std::vector<udf::Int64Value>& in2) {
    RowDescriptor rd({udf::UDFDataType::INT64, udf::UDFDataType::INT64});
    RowBatch rb(rd, in1.size());
    PL_CHECK_OK(rb.AddColumn(ToArrow(in1, arrow::default_memory_pool())));
    PL_CHECK_OK(rb.AddColumn(ToArrow(in2, arrow::default_memory_pool())));
    return rb;
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
};

TEST_F(MapNodeTest, basic) {
  RowDescriptor output_rd({udf::UDFDataType::INT64});
  MapNode mn;
  MockExecNode mock_child_;
  mn.AddChild(&mock_child_);
  PL_CHECK_OK(mn.Init(*plan_node_, output_rd, {}));
  PL_CHECK_OK(mn.Prepare(exec_state_.get()));
  PL_CHECK_OK(mn.Open(exec_state_.get()));
  auto rb1 = CreateInputRowBatch({1, 2, 3, 4}, {1, 3, 6, 9});

  auto check_result_batch1 = [&](ExecState* exec_state, const RowBatch& child_rb) {
    EXPECT_EQ(exec_state, exec_state_.get());
    EXPECT_EQ(child_rb.num_rows(), rb1.num_rows());
    EXPECT_EQ(child_rb.num_columns(), 1);
    EXPECT_EQ(child_rb.desc().type(0), udf::UDFDataType::INT64);
    auto output_col = child_rb.ColumnAt(0);
    auto casted = reinterpret_cast<arrow::Int64Array*>(output_col.get());
    EXPECT_EQ(2, casted->Value(0));
    EXPECT_EQ(5, casted->Value(1));
  };

  EXPECT_CALL(mock_child_, ConsumeNextImpl(_, _))
      .Times(1)
      .WillOnce(testing::DoAll(testing::Invoke(check_result_batch1), testing::Return(Status::OK())))
      .RetiresOnSaturation();
  PL_CHECK_OK(mn.ConsumeNext(exec_state_.get(), rb1));

  auto rb2 = CreateInputRowBatch({1, 2, 3}, {1, 4, 6});

  auto check_result_batch2 = [&](ExecState* exec_state, const RowBatch& child_rb) {
    EXPECT_EQ(exec_state, exec_state_.get());
    EXPECT_EQ(child_rb.num_rows(), rb2.num_rows());
    EXPECT_EQ(child_rb.num_columns(), 1);
    EXPECT_EQ(child_rb.desc().type(0), udf::UDFDataType::INT64);
    auto output_col = child_rb.ColumnAt(0);
    auto casted = reinterpret_cast<arrow::Int64Array*>(output_col.get());
    EXPECT_EQ(2, casted->Value(0));
    EXPECT_EQ(6, casted->Value(1));
  };

  EXPECT_CALL(mock_child_, ConsumeNextImpl(_, _))
      .Times(1)
      .WillOnce(
          testing::DoAll(testing::Invoke(check_result_batch2), testing::Return(Status::OK())));
  PL_CHECK_OK(mn.ConsumeNext(exec_state_.get(), rb2));
  PL_CHECK_OK(mn.Close(exec_state_.get()));
}

TEST_F(MapNodeTest, child_fail) {
  RowDescriptor output_rd({udf::UDFDataType::INT64});
  MapNode mn;
  MockExecNode mock_child_;
  mn.AddChild(&mock_child_);
  PL_CHECK_OK(mn.Init(*plan_node_, output_rd, {}));
  PL_CHECK_OK(mn.Prepare(exec_state_.get()));
  PL_CHECK_OK(mn.Open(exec_state_.get()));

  auto rb1 = CreateInputRowBatch({1, 2, 3, 4}, {1, 3, 6, 9});
  EXPECT_CALL(mock_child_, ConsumeNextImpl(_, _))
      .Times(1)
      .WillRepeatedly(testing::Return(error::InvalidArgument("args")));

  auto retval = mn.ConsumeNext(exec_state_.get(), rb1);
  EXPECT_FALSE(retval.ok());
  EXPECT_TRUE(error::IsInvalidArgument(retval));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
