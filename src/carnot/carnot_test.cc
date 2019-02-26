#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <pypa/parser/parser.hh>
#include <unordered_map>
#include <vector>

#include "src/carnot/carnot.h"
#include "src/carnot/exec/row_descriptor.h"
#include "src/carnot/exec/table.h"

namespace pl {
namespace carnot {

using testing::_;

class CarnotTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    EXPECT_OK(carnot_.Init());
    auto descriptor =
        std::vector<udf::UDFDataType>({types::DataType::FLOAT64, types::DataType::INT64});
    exec::RowDescriptor rd = exec::RowDescriptor(descriptor);

    auto table = std::make_shared<exec::Table>(rd);

    auto col1 = std::make_shared<exec::Column>(udf::UDFDataType::FLOAT64, "col1");
    std::vector<udf::Float64Value> col1_in1 = {0.5, 1.2, 5.3};
    std::vector<udf::Float64Value> col1_in2 = {0.1, 5.1};
    EXPECT_OK(col1->AddBatch(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
    EXPECT_OK(col1->AddBatch(udf::ToArrow(col1_in2, arrow::default_memory_pool())));

    auto col2 = std::make_shared<exec::Column>(udf::UDFDataType::INT64, "col2");
    std::vector<udf::Int64Value> col2_in1 = {1, 2, 3};
    std::vector<udf::Int64Value> col2_in2 = {5, 6};
    EXPECT_OK(col2->AddBatch(udf::ToArrow(col2_in1, arrow::default_memory_pool())));
    EXPECT_OK(col2->AddBatch(udf::ToArrow(col2_in2, arrow::default_memory_pool())));

    EXPECT_OK(table->AddColumn(col1));
    EXPECT_OK(table->AddColumn(col2));

    auto table_store = carnot_.table_store();
    table_store->AddTable("test_table", table);
  }

  Carnot carnot_;
};

TEST_F(CarnotTest, basic) {
  std::vector<udf::Float64Value> col1_in1 = {0.5, 1.2, 5.3};
  std::vector<udf::Float64Value> col1_in2 = {0.1, 5.1};
  std::vector<udf::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<udf::Int64Value> col2_in2 = {5, 6};

  auto query = absl::StrJoin(
      {
          "queryDF = From(table='test_table', select=['col1', 'col2']).Result(name='test_output')",
      },
      "\n");
  auto s = carnot_.ExecuteQuery(query);
  ASSERT_TRUE(s.ok());

  auto table_store = carnot_.table_store();
  auto output_table = table_store->GetTable("test_output");
  EXPECT_EQ(2, output_table->NumBatches());

  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(udf::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto rb2 =
      output_table->GetRowBatch(1, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(udf::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(udf::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, map_test) {
  std::vector<udf::Float64Value> col1_in1 = {1.5, 3.2, 8.3};
  std::vector<udf::Float64Value> col1_in2 = {5.1, 11.1};

  auto query = absl::StrJoin(
      {
          "queryDF = From(table='test_table', select=['col1', 'col2']).Map(fn=lambda r : {'res' : "
          "pl.add(r.col1,r.col2)}).Result(name='test_output')",
      },
      "\n");

  auto s = carnot_.ExecuteQuery(query);
  ASSERT_TRUE(s.ok());

  auto table_store = carnot_.table_store();
  auto output_table = table_store->GetTable("test_output");
  EXPECT_EQ(2, output_table->NumBatches());
  //
  auto rb1 = output_table->GetRowBatch(0, std::vector<int64_t>({0}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(udf::ToArrow(col1_in1, arrow::default_memory_pool())));

  auto rb2 = output_table->GetRowBatch(1, std::vector<int64_t>({0}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(udf::ToArrow(col1_in2, arrow::default_memory_pool())));
}

}  // namespace carnot
}  // namespace pl
