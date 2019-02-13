#include <arrow/array.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/plan/compiler_state.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/schema.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/carnot/proto/test_proto.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/common/object_pool.h"

namespace pl {
namespace carnot {
namespace exec {

using google::protobuf::TextFormat;

class AddUDF : public udf::ScalarUDF {
 public:
  udf::Float64Value Exec(udf::FunctionContext*, udf::Int64Value v1, udf::Float64Value v2) {
    return v1.val + v2.val;
  }
};

class MultiplyUDF : public udf::ScalarUDF {
 public:
  udf::Float64Value Exec(udf::FunctionContext*, udf::Float64Value v1, udf::Int64Value v2) {
    return v1.val * v2.val;
  }
};

class ExecGraphTest : public ::testing::Test {
 protected:
  void SetUp() override {
    carnotpb::PlanFragment pf_pb;
    ASSERT_TRUE(
        TextFormat::MergeFromString(carnotpb::testutils::kPlanFragmentWithFourNodes, &pf_pb));
    ASSERT_TRUE(plan_fragment_->Init(pf_pb).ok());

    auto udf_registry = std::make_shared<udf::ScalarUDFRegistry>("test_registry");
    auto uda_registry = std::make_shared<udf::UDARegistry>("test_registry");
    auto table_store = std::make_shared<TableStore>();

    exec_state_ = std::make_shared<ExecState>(udf_registry, uda_registry, table_store);
  }
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  std::shared_ptr<ExecState> exec_state_ = nullptr;
};

TEST_F(ExecGraphTest, basic) {
  ExecutionGraph e;
  auto c = std::make_shared<plan::CompilerState>(std::make_shared<udf::ScalarUDFRegistry>("test"),
                                                 std::make_shared<udf::UDARegistry>("testUDA"));

  auto schema = std::make_shared<plan::Schema>();
  plan::Relation relation(std::vector<udf::UDFDataType>({udf::UDFDataType::INT64}),
                          std::vector<std::string>({"test"}));
  schema->AddRelation(1, relation);

  auto s = e.Init(schema, c, exec_state_, plan_fragment_);

  // Check that the structure of the exec graph is correct.
  auto sources = e.sources();
  EXPECT_EQ(1, sources.size());
  auto root = e.node(sources[0]).ConsumeValueOrDie();
  EXPECT_TRUE(root->IsSource());
  auto root_children = root->children();
  EXPECT_EQ(2, root_children.size());
  EXPECT_TRUE(root_children[0]->IsProcessing());
  EXPECT_TRUE(root_children[1]->IsProcessing());
  EXPECT_EQ(1, root_children[0]->children().size());
  EXPECT_EQ(1, root_children[1]->children().size());
  EXPECT_TRUE(root_children[0]->children()[0]->IsSink());
  EXPECT_EQ(root_children[1]->children()[0], root_children[0]->children()[0]);
  EXPECT_EQ(0, root_children[1]->children()[0]->children().size());
}

TEST_F(ExecGraphTest, execute) {
  ExecutionGraph e;

  carnotpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(carnotpb::testutils::kLinearPlanFragment, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_TRUE(plan_fragment_->Init(pf_pb).ok());

  auto udf_registry = std::make_shared<udf::ScalarUDFRegistry>("testUDF");
  EXPECT_OK(udf_registry->Register<AddUDF>("add"));
  EXPECT_OK(udf_registry->Register<MultiplyUDF>("multiply"));
  auto uda_registry = std::make_shared<udf::UDARegistry>("testUDA");

  auto c = std::make_shared<plan::CompilerState>(udf_registry, uda_registry);

  auto schema = std::make_shared<plan::Schema>();
  schema->AddRelation(1, plan::Relation(std::vector<udf::UDFDataType>({udf::UDFDataType::INT64,
                                                                       udf::UDFDataType::BOOLEAN,
                                                                       udf::UDFDataType::FLOAT64}),
                                        std::vector<std::string>({"a", "b", "c"})));

  auto descriptor = std::vector<udf::UDFDataType>(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  auto table = std::make_shared<Table>(rd);

  auto col1 = std::make_shared<Column>(udf::UDFDataType::INT64);
  std::vector<udf::Int64Value> col1_in1 = {1, 2, 3};
  std::vector<udf::Int64Value> col1_in2 = {4, 5};
  EXPECT_OK(col1->AddChunk(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(col1->AddChunk(udf::ToArrow(col1_in2, arrow::default_memory_pool())));

  auto col2 = std::make_shared<Column>(udf::UDFDataType::BOOLEAN);
  std::vector<udf::BoolValue> col2_in1 = {true, false, true};
  std::vector<udf::BoolValue> col2_in2 = {false, false};
  EXPECT_OK(col2->AddChunk(udf::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(col2->AddChunk(udf::ToArrow(col2_in2, arrow::default_memory_pool())));

  auto col3 = std::make_shared<Column>(udf::UDFDataType::FLOAT64);
  std::vector<udf::Float64Value> col3_in1 = {1.4, 6.2, 10.2};
  std::vector<udf::Float64Value> col3_in2 = {3.4, 1.2};
  EXPECT_OK(col3->AddChunk(udf::ToArrow(col3_in1, arrow::default_memory_pool())));
  EXPECT_OK(col3->AddChunk(udf::ToArrow(col3_in2, arrow::default_memory_pool())));

  EXPECT_OK(table->AddColumn(col1));
  EXPECT_OK(table->AddColumn(col2));
  EXPECT_OK(table->AddColumn(col3));

  auto table_store = std::make_shared<TableStore>();
  table_store->AddTable("numbers", table);

  auto exec_state_ = std::make_shared<ExecState>(udf_registry, uda_registry, table_store);

  auto s = e.Init(schema, c, exec_state_, plan_fragment_);

  EXPECT_OK(e.Execute());

  auto output_table = exec_state_->table_store()->GetTable("output");
  std::vector<udf::Float64Value> out_in1 = {4.8, 16.4, 26.4};
  std::vector<udf::Float64Value> out_in2 = {14.8, 12.4};
  EXPECT_EQ(2, output_table->numBatches());
  EXPECT_TRUE(output_table->GetRowBatch(0, std::vector<int64_t>({0}))
                  .ConsumeValueOrDie()
                  ->ColumnAt(0)
                  ->Equals(udf::ToArrow(out_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(output_table->GetRowBatch(1, std::vector<int64_t>({0}))
                  .ConsumeValueOrDie()
                  ->ColumnAt(0)
                  ->Equals(udf::ToArrow(out_in2, arrow::default_memory_pool())));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
