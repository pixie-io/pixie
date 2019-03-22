#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/carnot.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/schema/row_descriptor.h"
#include "src/carnot/schema/table.h"

namespace pl {
namespace carnot {

using exec::CarnotTestUtils;

class CarnotTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    carnot_ = Carnot::Create().ConsumeValueOrDie();
    auto table = CarnotTestUtils::TestTable();
    carnot_->AddTable("test_table", table);
  }

  std::unique_ptr<Carnot> carnot_;
};

TEST_F(CarnotTest, basic) {
  std::vector<types::Float64Value> col1_in1 = {0.5, 1.2, 5.3};
  std::vector<types::Float64Value> col1_in2 = {0.1, 5.1};
  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<types::Int64Value> col2_in2 = {5, 6};

  auto query = absl::StrJoin(
      {
          "queryDF = From(table='test_table', select=['col1', 'col2']).Result(name='test_output')",
      },
      "\n");
  // No time column, doesn't use a time parameter.
  auto s = carnot_->ExecuteQuery(query, 0);
  ASSERT_OK(s);
  auto res = s.ConsumeValueOrDie();
  EXPECT_EQ(5, res.rows_processed);
  EXPECT_EQ(5 * sizeof(double) + 5 * sizeof(int64_t), res.bytes_processed);
  EXPECT_GT(res.compile_time_ns, 0);
  EXPECT_GT(res.exec_time_ns, 0);

  auto output_table = carnot_->GetTable("test_output");
  EXPECT_EQ(2, output_table->NumBatches());

  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto rb2 =
      output_table->GetRowBatch(1, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(types::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, map_test) {
  std::vector<types::Float64Value> col1_in1 = {1.5, 3.2, 8.3};
  std::vector<types::Float64Value> col1_in2 = {5.1, 11.1};

  auto query = absl::StrJoin(
      {
          "queryDF = From(table='test_table', select=['col1', 'col2']).Map(fn=lambda r : {'res' : "
          "pl.add(r.col1,r.col2)}).Result(name='test_output')",
      },
      "\n");

  // No time column, doesn't use a time parameter.
  auto s = carnot_->ExecuteQuery(query, 0);
  ASSERT_OK(s);

  auto output_table = carnot_->GetTable("test_output");
  EXPECT_EQ(2, output_table->NumBatches());
  //
  auto rb1 = output_table->GetRowBatch(0, std::vector<int64_t>({0}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));

  auto rb2 = output_table->GetRowBatch(1, std::vector<int64_t>({0}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
}
// Test whether the compiler will handle issues nicely
TEST_F(CarnotTest, bad_syntax) {
  // Missing paranethesis
  auto bad_syntax = "queryDF = From(.Result(name='test_output')";
  // No time column, doesn't use a time parameter.
  auto bad_syntax_status = carnot_->ExecuteQuery(bad_syntax, 0);
  VLOG(1) << bad_syntax_status.ToString();
  EXPECT_FALSE(bad_syntax_status.ok());
}

TEST_F(CarnotTest, wrong_args) {
  // select -> sel (wrong arg for Form).
  auto wrong_arg_names =
      "queryDF = From(table='test_table', sel=['col2', 'col2']).Result(name='test_output')";
  // No time column, doesn't use a time parameter.
  auto wrong_arg_status = carnot_->ExecuteQuery(wrong_arg_names, 0);
  VLOG(1) << wrong_arg_status.ToString();
  EXPECT_FALSE(wrong_arg_status.ok());
}

TEST_F(CarnotTest, wrong_columns) {
  // Adding extra column that doesn't exist in the schema.
  auto wrong_columns =
      "queryDF = From(table='test_table', select=['col1', 'col2', "
      "'bunk_column']).Result(name='test_output')";
  // No time column, doesn't use a time parameter.
  auto wrong_columns_status = carnot_->ExecuteQuery(wrong_columns, 0);
  VLOG(1) << wrong_columns_status.ToString();
  EXPECT_FALSE(wrong_columns_status.ok());
}

TEST_F(CarnotTest, missing_result) {
  // Missing the result call at the end of the query.
  auto missing_result_call = "queryDF = From(table='test_table', select=['col1', 'col2'])";
  // No time column, doesn't use a time parameter.
  auto missing_result_status = carnot_->ExecuteQuery(missing_result_call, 0);
  VLOG(1) << missing_result_status.ToString();
  EXPECT_FALSE(missing_result_status.ok());
}

// See whether executor is tolerant to receiving the wrong table name.
TEST_F(CarnotTest, wrong_table_name) {
  auto wrong_table_name =
      "queryDF = From(table='bunk_table', select=['col1', 'col2']).Result(name='test_output')";
  // No time column, doesn't use a time parameter.
  auto wrong_table_status = carnot_->ExecuteQuery(wrong_table_name, 0);
  VLOG(1) << wrong_table_status.ToString();
  EXPECT_FALSE(wrong_table_status.ok());
}

// Select no columns which should be acceptable.
TEST_F(CarnotTest, no_columns) {
  auto no_columns_name = "queryDF = From(table='test_table', select=[]).Result(name='test_output')";
  // No time column, doesn't use a time parameter.
  auto no_columns_status = carnot_->ExecuteQuery(no_columns_name, 0);
  VLOG(1) << no_columns_status.ToString();
  EXPECT_OK(no_columns_status);
}

TEST_F(CarnotTest, empty_query_test) {
  // No time column, doesn't use a time parameter.
  auto s = carnot_->ExecuteQuery("", 0);
  EXPECT_FALSE(s.ok());
}

TEST_F(CarnotTest, map_op_udf_add) {
  auto add_query = absl::StrJoin({"queryDF = From(table='test_table', select=['col1', 'col2'])",
                                  "mapOutput = queryDF.Map(fn=lambda r : {'sum' : r.col1 + "
                                  "r.col2}).Result(name='test_output')"},
                                 "\n");
  // No time column, doesn't use a time parameter.
  EXPECT_OK(carnot_->ExecuteQuery(add_query, 0));
}

TEST_F(CarnotTest, map_op_udf_mult) {
  auto mult_query = absl::StrJoin({"queryDF = From(table='test_table', select=['col1', 'col2'])",
                                   "mapOutput = queryDF.Map(fn=lambda r : {'mult' : r.col1 * "
                                   "r.col2}).Result(name='test_output')"},
                                  "\n");
  // No time column, doesn't use a time parameter.
  EXPECT_OK(carnot_->ExecuteQuery(mult_query, 0));
}

TEST_F(CarnotTest, map_op_udf_sub) {
  auto sub_query = absl::StrJoin({"queryDF = From(table='test_table', select=['col1', 'col2'])",
                                  "mapOutput = queryDF.Map(fn=lambda r : {'sub' : r.col1 - "
                                  "r.col2}).Result(name='test_output')"},
                                 "\n");
  // No time column, doesn't use a time parameter.
  EXPECT_OK(carnot_->ExecuteQuery(sub_query, 0));
}

TEST_F(CarnotTest, map_op_udf_div) {
  auto div_query = absl::StrJoin({"queryDF = From(table='test_table', select=['col1', 'col2'])",
                                  "mapOutput = queryDF.Map(fn=lambda r : {'div' : r.col1 / "
                                  "r.col2}).Result(name='test_output')"},
                                 "\n");
  // No time column, doesn't use a time parameter.
  EXPECT_OK(carnot_->ExecuteQuery(div_query, 0));
}

TEST_F(CarnotTest, order_test) {
  auto table = CarnotTestUtils::BigTestTable();
  carnot_->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col2', 'col3']).Map(fn=lambda "
          "r : {'res' : "
          "pl.add(r.col3,r.col2), 'a': 1, 'b': 2}).Result(name='test_output')",
      },
      "\n");
  // Time Column unused, doesn't matter what value is.
  auto s = carnot_->ExecuteQuery(query, 0);
  ASSERT_OK(s);

  auto output_table = carnot_->GetTable("test_output");
  EXPECT_EQ(3, output_table->NumBatches());
  EXPECT_EQ(3, output_table->NumColumns());

  std::vector<types::Float64Value> col0_out1 = {6.5, 3.2, 17.3};
  std::vector<types::Float64Value> col0_out2 = {5.1, 65.1};
  std::vector<types::Float64Value> col1_out3 = {61.2, 12.1, 20.3};
  std::vector<types::Int64Value> col1_out1 = {1, 1, 1};
  std::vector<types::Int64Value> col2_out1 = {2, 2, 2};

  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();

  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col0_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col1_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(2)->Equals(types::ToArrow(col2_out1, arrow::default_memory_pool())));

  auto rb2 = output_table->GetRowBatch(1, std::vector<int64_t>({0}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(types::ToArrow(col0_out2, arrow::default_memory_pool())));

  auto rb3 = output_table->GetRowBatch(2, std::vector<int64_t>({0}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb3->ColumnAt(0)->Equals(types::ToArrow(col1_out3, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, range_test_multiple_rbs) {
  auto table = CarnotTestUtils::BigTestTable();

  carnot_->AddTable("big_test_table", table);
  int64_t start_time = 2;
  int64_t stop_time = 6;
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col2', "
          "'col3']).Range(start=$0, stop=$1).Result(name='range_output')",
      },
      "\n");
  query = absl::Substitute(query, start_time, stop_time);
  // now() not called, doesn't matter what now is.
  auto s = carnot_->ExecuteQuery(query, 0);
  VLOG(1) << s.ToString();
  ASSERT_OK(s);

  auto output_table = carnot_->GetTable("range_output");
  EXPECT_EQ(2, output_table->NumBatches());
  EXPECT_EQ(3, output_table->NumColumns());

  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();

  std::vector<types::Time64NSValue> col0_out1;
  std::vector<types::Float64Value> col1_out1;
  std::vector<types::Int64Value> col2_out1;
  for (int64_t i = 0; i < table->GetColumn(0)->batch(0)->length(); i++) {
    if (CarnotTestUtils::big_test_col1[i].val >= 2 && CarnotTestUtils::big_test_col1[i].val < 6) {
      col0_out1.emplace_back(CarnotTestUtils::big_test_col1[i].val);
      col1_out1.emplace_back(CarnotTestUtils::big_test_col2[i].val);
      col2_out1.emplace_back(CarnotTestUtils::big_test_col3[i].val);
    }
  }

  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col0_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col1_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(2)->Equals(types::ToArrow(col2_out1, arrow::default_memory_pool())));

  auto rb2 =
      output_table->GetRowBatch(1, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();

  std::vector<types::Time64NSValue> col0_out2;
  std::vector<types::Float64Value> col1_out2;
  std::vector<types::Int64Value> col2_out2;
  for (int64_t i = table->GetColumn(0)->batch(0)->length();
       i < table->GetColumn(0)->batch(0)->length() + table->GetColumn(0)->batch(1)->length(); i++) {
    if (CarnotTestUtils::big_test_col1[i].val >= start_time &&
        CarnotTestUtils::big_test_col1[i].val < stop_time) {
      col0_out2.emplace_back(CarnotTestUtils::big_test_col1[i].val);
      col1_out2.emplace_back(CarnotTestUtils::big_test_col2[i].val);
      col2_out2.emplace_back(CarnotTestUtils::big_test_col3[i].val);
    }
  }

  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(types::ToArrow(col0_out2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(types::ToArrow(col1_out2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(2)->Equals(types::ToArrow(col2_out2, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, range_test_single_rb) {
  auto table = CarnotTestUtils::BigTestTable();

  carnot_->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col2', "
          "'col3']).Range(start=$0, stop=$1).Result(name='range_output')",
      },
      "\n");
  int64_t start_time = 2;
  int64_t stop_time = 3;
  query = absl::Substitute(query, start_time, stop_time);
  // now() not called, doesn't matter what now is.
  auto s = carnot_->ExecuteQuery(query, 0);
  ASSERT_OK(s);

  auto output_table = carnot_->GetTable("range_output");
  EXPECT_EQ(1, output_table->NumBatches());
  EXPECT_EQ(3, output_table->NumColumns());

  std::vector<types::Time64NSValue> col0_out1;
  std::vector<types::Float64Value> col1_out1;
  std::vector<types::Int64Value> col2_out1;
  for (size_t i = 0; i < CarnotTestUtils::big_test_col1.size(); i++) {
    if (CarnotTestUtils::big_test_col1[i].val >= start_time &&
        CarnotTestUtils::big_test_col1[i].val < stop_time) {
      col0_out1.emplace_back(CarnotTestUtils::big_test_col1[i].val);
      col1_out1.emplace_back(CarnotTestUtils::big_test_col2[i].val);
      col2_out1.emplace_back(CarnotTestUtils::big_test_col3[i].val);
    }
  }

  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col0_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col1_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(2)->Equals(types::ToArrow(col2_out1, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, empty_range_test) {
  // Tests that a table that has no rows that fall within the query's range returns an empty
  // rowbatch.
  auto table = CarnotTestUtils::BigTestTable();

  carnot_->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col2', "
          "'col3']).Range(start=$0, stop=$1).Result(name='range_output')",
      },
      "\n");
  auto time_col = CarnotTestUtils::big_test_col1;
  auto max_time = std::max_element(time_col.begin(), time_col.end());

  int64_t start_time = max_time->val + 1;
  int64_t stop_time = start_time + 10000;
  query = absl::Substitute(query, start_time, stop_time);
  // now() not called, doesn't matter what now is.
  auto s = carnot_->ExecuteQuery(query, 0);
  VLOG(1) << s.ToString();
  ASSERT_OK(s);

  auto output_table = carnot_->GetTable("range_output");
  EXPECT_EQ(0, output_table->NumBatches());
  EXPECT_EQ(3, output_table->NumColumns());
}

class CarnotRangeTest
    : public CarnotTest,
      public ::testing::WithParamInterface<std::tuple<types::Int64Value, size_t, bool>> {
 protected:
  void SetUp() {
    CarnotTest::SetUp();
    bool start_at_now;
    types::Int64Value sub_time;
    std::tie(sub_time, num_batches, start_at_now) = GetParam();
    query = absl::StrJoin({"queryDF = From(table='big_test_table', select=['time_', 'col2'])",
                           "queryDF.Range(start=$0, stop=$1).Result(name='range_output')"},
                          "\n");
    if (start_at_now) {
      query = absl::Substitute(query, "plc.now()", sub_time.val);
    } else {
      query = absl::Substitute(query, sub_time.val, "plc.now()");
    }
    auto table = CarnotTestUtils::BigTestTable();
    carnot_->AddTable("big_test_table", table);

    auto max_time = CarnotTestUtils::big_test_col1[CarnotTestUtils::big_test_col1.size() - 1];
    now_time_ = max_time.val + 1;
  }
  size_t num_batches;
  std::string query;
  int64_t now_time_;
};

std::vector<std::tuple<types::Int64Value, size_t, bool>> range_test_vals = {
    {CarnotTestUtils::big_test_col1[CarnotTestUtils::big_test_col1.size() - 1] /*sub_time*/,
     0 /*num_batches*/, true /*start_at_now*/},
    {CarnotTestUtils::big_test_col1[CarnotTestUtils::split_idx[1].first].val /*sub_time*/,
     CarnotTestUtils::split_idx.size() - 1 /*num_batches*/, false /*start_at_now*/}};

TEST_P(CarnotRangeTest, range_now_keyword_test) {
  auto s = carnot_->ExecuteQuery(query, now_time_);
  ASSERT_OK(s);

  auto output_table = carnot_->GetTable("range_output");
  EXPECT_EQ(num_batches, output_table->NumBatches());
  EXPECT_EQ(2, output_table->NumColumns());
}

INSTANTIATE_TEST_CASE_P(CarnotRangeVariants, CarnotRangeTest, ::testing::ValuesIn(range_test_vals));

TEST_F(CarnotTest, group_by_all_agg_test) {
  auto table = CarnotTestUtils::BigTestTable();

  carnot_->AddTable("big_test_table", table);
  auto agg_dict =
      absl::StrJoin({"'mean' : pl.mean(r.col2)", "'count' : pl.count(r.col3)",
                     "'min' : pl.min(r.col2)", "'max' : pl.max(r.col3)", "'sum' : pl.sum(r.col3)"},
                    ",");
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col2', 'col3'])",
          "aggDF = queryDF.Agg(by=None, fn=lambda r : {$0})",
          "aggDF.Result(name='test_output')",
      },
      "\n");
  query = absl::Substitute(query, agg_dict);
  // now() not called, doesn't matter what now is.
  auto s = carnot_->ExecuteQuery(query, 0);
  auto output_table = carnot_->GetTable("test_output");
  EXPECT_EQ(1, output_table->NumBatches());
  EXPECT_EQ(5, output_table->NumColumns());

  auto rb1 =
      output_table
          ->GetRowBatch(0, std::vector<int64_t>({0, 1, 2, 3, 4}), arrow::default_memory_pool())
          .ConsumeValueOrDie();

  auto test_col2 = CarnotTestUtils::big_test_col2;
  auto test_col3 = CarnotTestUtils::big_test_col3;

  auto int64_sum_lambda = [](types::Int64Value a, types::Int64Value b) { return a.val + b.val; };
  auto float64_sum_lambda = [](types::Float64Value a, types::Float64Value b) {
    return a.val + b.val;
  };
  types::Float64Value col2_expected_sum =
      std::accumulate(test_col2.begin(), test_col2.end(), 0.0, float64_sum_lambda);
  types::Float64Value col2_expected_mean = col2_expected_sum.val / test_col2.size();

  types::Int64Value col3_expected_count = test_col3.size();
  types::Float64Value col2_expected_min = *std::min_element(test_col2.begin(), test_col2.end());
  types::Int64Value col3_expected_max = *std::max_element(test_col3.begin(), test_col3.end());

  types::Int64Value col3_expected_sum =
      std::accumulate(CarnotTestUtils::big_test_col3.begin(), CarnotTestUtils::big_test_col3.end(),
                      0, int64_sum_lambda);

  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(
      types::ToArrow(std::vector<types::Float64Value>({types::Float64Value(col2_expected_mean)}),
                     arrow::default_memory_pool())));

  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(
      types::ToArrow(std::vector<types::Int64Value>({types::Int64Value(col3_expected_count)}),
                     arrow::default_memory_pool())));

  EXPECT_TRUE(rb1->ColumnAt(2)->Equals(
      types::ToArrow(std::vector<types::Float64Value>({types::Float64Value(col2_expected_min)}),
                     arrow::default_memory_pool())));

  EXPECT_TRUE(rb1->ColumnAt(3)->Equals(
      types::ToArrow(std::vector<types::Int64Value>({types::Int64Value(col3_expected_max)}),
                     arrow::default_memory_pool())));

  EXPECT_TRUE(rb1->ColumnAt(4)->Equals(
      types::ToArrow(std::vector<types::Int64Value>({types::Int64Value(col3_expected_sum)}),
                     arrow::default_memory_pool())));
}

TEST_F(CarnotTest, group_by_col_agg_test) {
  auto table = CarnotTestUtils::BigTestTable();

  carnot_->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col3', 'num_groups'])",
          "aggDF = queryDF.Agg(by=lambda r : r.num_groups, fn=lambda r : {'sum' : pl.sum(r.col3)})",
          "aggDF.Result(name='test_output')",
      },
      "\n");
  // now() not called, doesn't matter what now is.
  auto s = carnot_->ExecuteQuery(query, 0);
  VLOG(1) << s.ToString();
  ASSERT_OK(s);

  auto output_table = carnot_->GetTable("test_output");
  EXPECT_EQ(1, output_table->NumBatches());
  EXPECT_EQ(2, output_table->NumColumns());
  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();

  std::vector<types::Int64Value> expected_groups = {1, 2, 3};
  std::vector<types::Int64Value> expected_sum = {13, 129, 24};
  std::unordered_map<int64_t, int64_t> expected = {{1, 13}, {2, 129}, {3, 24}};
  std::unordered_map<int64_t, int64_t> actual;

  for (int i = 0; i < rb1->num_rows(); ++i) {
    auto output_col_grp = rb1->ColumnAt(0);
    auto output_col_agg = rb1->ColumnAt(1);
    auto casted_grp = static_cast<arrow::Int64Array *>(output_col_grp.get());
    auto casted_agg = static_cast<arrow::Int64Array *>(output_col_agg.get());

    actual[casted_grp->Value(i)] = casted_agg->Value(i);
  }
  EXPECT_EQ(expected, actual);
}

TEST_F(CarnotTest, multiple_group_by_test) {
  auto table = CarnotTestUtils::BigTestTable();

  carnot_->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col3', 'num_groups', "
          "'string_groups'])",
          "aggDF = queryDF.Agg(by=lambda r : [r.num_groups, r.string_groups], fn=lambda r : {'sum' "
          ": pl.sum(r.col3)})",
          "aggDF.Result(name='test_output')",
      },
      "\n");
  // now() not called, doesn't matter what now is.
  auto s = carnot_->ExecuteQuery(query, 0);
  VLOG(1) << s.ToString();
  ASSERT_OK(s);

  auto output_table = carnot_->GetTable("test_output");
  EXPECT_EQ(1, output_table->NumBatches());
  EXPECT_EQ(3, output_table->NumColumns());
  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  struct Key {
    int64_t num_group;
    std::string string_group;

    bool operator<(const Key &other) const {
      return num_group < other.num_group ||
             (num_group == other.num_group && string_group < other.string_group);
    }
    bool operator==(const Key &other) const {
      return (num_group == other.num_group) && string_group == other.string_group;
    }
  };

  std::map<Key, int64_t> expected = {
      {Key{1, "sum"}, 6},  {Key{1, "mean"}, 7},  {Key{3, "sum"}, 24},
      {Key{2, "sum"}, 60}, {Key{2, "mean"}, 69},
  };
  std::map<Key, int64_t> actual;
  for (int i = 0; i < rb1->num_rows(); ++i) {
    auto output_col_num_grp = rb1->ColumnAt(0);
    auto output_col_str_grp = rb1->ColumnAt(1);
    auto output_col_agg = rb1->ColumnAt(2);
    auto casted_num_grp = static_cast<arrow::Int64Array *>(output_col_num_grp.get());
    auto casted_str_grp = static_cast<arrow::StringArray *>(output_col_str_grp.get());

    auto casted_agg = static_cast<arrow::Int64Array *>(output_col_agg.get());
    auto key = Key{casted_num_grp->Value(i), casted_str_grp->GetString(i)};

    actual[key] = casted_agg->Value(i);
  }
  EXPECT_EQ(expected, actual);
}

TEST_F(CarnotTest, comparison_tests) {
  auto table = CarnotTestUtils::BigTestTable();

  carnot_->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col3', 'num_groups', "
          "'string_groups'])",
          "aggDF = queryDF.Map(fn=lambda r : {'lt' : r.col3 < $0, 'gt' : r.num_groups > $1 })",
          "aggDF.Result(name='test_output')",
      },
      "\n");
  // Values to test on.
  int64_t col3_lt_val = 12;
  int64_t num_groups_gt_val = 1;
  query = absl::Substitute(query, col3_lt_val, num_groups_gt_val);
  // now() not called, doesn't matter what now is.
  auto s = carnot_->ExecuteQuery(query, 0);
  VLOG(1) << s.ToString();
  ASSERT_OK(s);
  auto output_table = carnot_->GetTable("test_output");
  EXPECT_EQ(3, output_table->NumBatches());
  EXPECT_EQ(2, output_table->NumColumns());
  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  auto col3 = CarnotTestUtils::big_test_col3;
  auto col_num_groups = CarnotTestUtils::big_test_groups;
  std::vector<types::BoolValue> lt_exp;
  std::vector<types::BoolValue> gt_exp;

  for (int64_t i = 0; i < rb1->num_rows(); i++) {
    lt_exp.emplace_back(col3[i] < col3_lt_val);
    gt_exp.emplace_back(col_num_groups[i] > num_groups_gt_val);
  }
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(lt_exp, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(gt_exp, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, comparison_to_agg_tests) {
  auto table = CarnotTestUtils::BigTestTable();

  carnot_->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col3', 'num_groups', "
          "'string_groups'])",
          "mapDF = queryDF.Map(fn=lambda r : {'is_large' : r.col3 > $0, 'num_groups' : "
          "r.num_groups})",
          "aggDF = mapDF.Agg(by=lambda r : r.is_large, fn=lambda r : {'count' : "
          "pl.count(r.num_groups)})",
          "aggDF.Result(name='test_output')",
      },
      "\n");
  // Value to test on.
  int64_t col3_gt_val = 30;
  query = absl::Substitute(query, col3_gt_val);
  // now() not called, doesn't matter what now is.
  auto s = carnot_->ExecuteQuery(query, 0);
  VLOG(1) << s.ToString();
  ASSERT_OK(s);
  auto output_table = carnot_->GetTable("test_output");
  EXPECT_EQ(1, output_table->NumBatches());
  EXPECT_EQ(2, output_table->NumColumns());
  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  auto col3 = CarnotTestUtils::big_test_col3;
  int64_t gt_count = 0;
  for (auto &i : col3) {
    if (i > col3_gt_val) {
      gt_count += 1;
    }
  }
  std::unordered_map<bool, int64_t> expected = {{true, gt_count}, {false, col3.size() - gt_count}};
  std::unordered_map<bool, int64_t> actual;

  for (int i = 0; i < rb1->num_rows(); ++i) {
    auto output_col_grp = rb1->ColumnAt(0);
    auto output_col_agg = rb1->ColumnAt(1);
    auto casted_grp = static_cast<arrow::BooleanArray *>(output_col_grp.get());
    auto casted_agg = static_cast<arrow::Int64Array *>(output_col_agg.get());

    actual[casted_grp->Value(i)] = casted_agg->Value(i);
  }
  EXPECT_EQ(expected, actual);
}

}  // namespace carnot
}  // namespace pl
