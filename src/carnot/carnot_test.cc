#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <pypa/parser/parser.hh>
#include <unordered_map>
#include <vector>

#include "src/carnot/carnot.h"
#include "src/carnot/exec/row_descriptor.h"
#include "src/carnot/exec/table.h"
#include "src/carnot/exec/test_utils.h"

namespace pl {
namespace carnot {

using exec::CarnotTestUtils;
using testing::_;

class CarnotTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    EXPECT_OK(carnot_.Init());

    auto table = CarnotTestUtils::TestTable();

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
// Test whether the compiler will handle issues nicely
TEST_F(CarnotTest, bad_syntax) {
  // Missing paranethesis
  auto bad_syntax = "queryDF = From(.Result(name='test_output')";
  auto bad_syntax_status = carnot_.ExecuteQuery(bad_syntax);
  VLOG(1) << bad_syntax_status.ToString();
  EXPECT_FALSE(bad_syntax_status.ok());
}

TEST_F(CarnotTest, wrong_args) {
  // select -> sel (wrong arg for Form).
  auto wrong_arg_names =
      "queryDF = From(table='test_table', sel=['col2', 'col2']).Result(name='test_output')";
  auto wrong_arg_status = carnot_.ExecuteQuery(wrong_arg_names);
  VLOG(1) << wrong_arg_status.ToString();
  EXPECT_FALSE(wrong_arg_status.ok());
}

TEST_F(CarnotTest, wrong_columns) {
  // Adding extra column that doesn't exist in the schema.
  auto wrong_columns =
      "queryDF = From(table='test_table', select=['col1', 'col2', "
      "'bunk_column']).Result(name='test_output')";
  auto wrong_columns_status = carnot_.ExecuteQuery(wrong_columns);
  VLOG(1) << wrong_columns_status.ToString();
  EXPECT_FALSE(wrong_columns_status.ok());
}

TEST_F(CarnotTest, missing_result) {
  // Missing the result call at the end of the query.
  auto missing_result_call = "queryDF = From(table='test_table', select=['col1', 'col2'])";
  auto missing_result_status = carnot_.ExecuteQuery(missing_result_call);
  VLOG(1) << missing_result_status.ToString();
  EXPECT_FALSE(missing_result_status.ok());
}

// See whether executor is tolerant to receiving the wrong table name.
TEST_F(CarnotTest, wrong_table_name) {
  auto wrong_table_name =
      "queryDF = From(table='bunk_table', select=['col1', 'col2']).Result(name='test_output')";
  auto wrong_table_status = carnot_.ExecuteQuery(wrong_table_name);
  VLOG(1) << wrong_table_status.ToString();
  EXPECT_FALSE(wrong_table_status.ok());
}

// Select no columns which should be acceptable.
TEST_F(CarnotTest, no_columns) {
  auto no_columns_name = "queryDF = From(table='test_table', select=[]).Result(name='test_output')";
  auto no_columns_status = carnot_.ExecuteQuery(no_columns_name);
  VLOG(1) << no_columns_status.ToString();
  EXPECT_OK(no_columns_status);
}

TEST_F(CarnotTest, empty_query_test) {
  auto s = carnot_.ExecuteQuery("");
  EXPECT_FALSE(s.ok());
}

}  // namespace carnot
}  // namespace pl
