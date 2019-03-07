#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

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
  auto res = s.ConsumeValueOrDie();
  EXPECT_EQ(5, res.rows_processed);
  EXPECT_EQ(5 * sizeof(double) + 5 * sizeof(int64_t), res.bytes_processed);
  EXPECT_GT(res.compile_time_ns, 0);
  EXPECT_GT(res.exec_time_ns, 0);

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

TEST_F(CarnotTest, map_op_udf_add) {
  auto add_query = absl::StrJoin({"queryDF = From(table='test_table', select=['col1', 'col2'])",
                                  "mapOutput = queryDF.Map(fn=lambda r : {'sum' : r.col1 + "
                                  "r.col2}).Result(name='test_output')"},
                                 "\n");
  EXPECT_OK(carnot_.ExecuteQuery(add_query));
}

TEST_F(CarnotTest, map_op_udf_mult) {
  auto mult_query = absl::StrJoin({"queryDF = From(table='test_table', select=['col1', 'col2'])",
                                   "mapOutput = queryDF.Map(fn=lambda r : {'mult' : r.col1 * "
                                   "r.col2}).Result(name='test_output')"},
                                  "\n");
  EXPECT_OK(carnot_.ExecuteQuery(mult_query));
}

TEST_F(CarnotTest, map_op_udf_sub) {
  auto sub_query = absl::StrJoin({"queryDF = From(table='test_table', select=['col1', 'col2'])",
                                  "mapOutput = queryDF.Map(fn=lambda r : {'sub' : r.col1 - "
                                  "r.col2}).Result(name='test_output')"},
                                 "\n");
  EXPECT_OK(carnot_.ExecuteQuery(sub_query));
}

TEST_F(CarnotTest, map_op_udf_div) {
  auto div_query = absl::StrJoin({"queryDF = From(table='test_table', select=['col1', 'col2'])",
                                  "mapOutput = queryDF.Map(fn=lambda r : {'div' : r.col1 / "
                                  "r.col2}).Result(name='test_output')"},
                                 "\n");
  EXPECT_OK(carnot_.ExecuteQuery(div_query));
}

TEST_F(CarnotTest, order_test) {
  auto table = CarnotTestUtils::BigTestTable();

  auto table_store = carnot_.table_store();
  table_store->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col2', 'col3']).Map(fn=lambda "
          "r : {'res' : "
          "pl.add(r.col3,r.col2), 'a': 1, 'b': 2}).Result(name='test_output')",
      },
      "\n");
  auto s = carnot_.ExecuteQuery(query);
  ASSERT_TRUE(s.ok());

  auto output_table = table_store->GetTable("test_output");
  EXPECT_EQ(3, output_table->NumBatches());
  EXPECT_EQ(3, output_table->NumColumns());

  std::vector<udf::Float64Value> col1_out1 = {6.5, 3.2, 17.3};
  std::vector<udf::Float64Value> col1_out2 = {5.1, 65.1};
  std::vector<udf::Float64Value> col1_out3 = {61.2, 12.1, 20.3};
  std::vector<udf::Int64Value> col2_out1 = {1, 1, 1};
  std::vector<udf::Int64Value> col3_out1 = {2, 2, 2};

  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();

  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(udf::ToArrow(col1_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(udf::ToArrow(col2_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(2)->Equals(udf::ToArrow(col3_out1, arrow::default_memory_pool())));

  auto rb2 = output_table->GetRowBatch(1, std::vector<int64_t>({0}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(udf::ToArrow(col1_out2, arrow::default_memory_pool())));

  auto rb3 = output_table->GetRowBatch(2, std::vector<int64_t>({0}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb3->ColumnAt(0)->Equals(udf::ToArrow(col1_out3, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, empty_range_test) {
  // Tests that a table that has no rows that fall within the query's range returns an empty
  // rowbatch.
  auto table = CarnotTestUtils::BigTestTable();

  auto table_store = carnot_.table_store();
  table_store->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col2', "
          "'col3']).Range(time='-2m').Result(name='rng_output')",
      },
      "\n");
  auto s = carnot_.ExecuteQuery(query);
  std::cout << s.msg();
  ASSERT_TRUE(s.ok());

  auto output_table = table_store->GetTable("rng_output");
  EXPECT_EQ(0, output_table->NumBatches());
  EXPECT_EQ(3, output_table->NumColumns());
}

TEST_F(CarnotTest, group_by_all_agg_test) {
  auto table = CarnotTestUtils::BigTestTable();

  auto table_store = carnot_.table_store();
  table_store->AddTable("big_test_table", table);
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
  auto s = carnot_.ExecuteQuery(query);
  auto output_table = table_store->GetTable("test_output");
  EXPECT_EQ(1, output_table->NumBatches());
  EXPECT_EQ(5, output_table->NumColumns());

  auto rb1 =
      output_table
          ->GetRowBatch(0, std::vector<int64_t>({0, 1, 2, 3, 4}), arrow::default_memory_pool())
          .ConsumeValueOrDie();

  auto test_col2 = CarnotTestUtils::big_test_col2;
  auto test_col3 = CarnotTestUtils::big_test_col3;

  auto int64_sum_lambda = [](udf::Int64Value a, udf::Int64Value b) { return a.val + b.val; };
  auto float64_sum_lambda = [](udf::Float64Value a, udf::Float64Value b) { return a.val + b.val; };
  udf::Float64Value col2_expected_sum =
      std::accumulate(test_col2.begin(), test_col2.end(), 0.0, float64_sum_lambda);
  udf::Float64Value col2_expected_mean = col2_expected_sum.val / test_col2.size();

  udf::Int64Value col3_expected_count = test_col3.size();
  udf::Float64Value col2_expected_min = *std::min_element(test_col2.begin(), test_col2.end());
  udf::Int64Value col3_expected_max = *std::max_element(test_col3.begin(), test_col3.end());

  udf::Int64Value col3_expected_sum =
      std::accumulate(CarnotTestUtils::big_test_col3.begin(), CarnotTestUtils::big_test_col3.end(),
                      0, int64_sum_lambda);

  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(
      udf::ToArrow(std::vector<udf::Float64Value>({udf::Float64Value(col2_expected_mean)}),
                   arrow::default_memory_pool())));

  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(
      udf::ToArrow(std::vector<udf::Int64Value>({udf::Int64Value(col3_expected_count)}),
                   arrow::default_memory_pool())));

  EXPECT_TRUE(rb1->ColumnAt(2)->Equals(
      udf::ToArrow(std::vector<udf::Float64Value>({udf::Float64Value(col2_expected_min)}),
                   arrow::default_memory_pool())));

  EXPECT_TRUE(rb1->ColumnAt(3)->Equals(
      udf::ToArrow(std::vector<udf::Int64Value>({udf::Int64Value(col3_expected_max)}),
                   arrow::default_memory_pool())));

  EXPECT_TRUE(rb1->ColumnAt(4)->Equals(
      udf::ToArrow(std::vector<udf::Int64Value>({udf::Int64Value(col3_expected_sum)}),
                   arrow::default_memory_pool())));
}

TEST_F(CarnotTest, DISABLED_group_by_col_agg_test) {
  auto table = CarnotTestUtils::BigTestTable();

  auto table_store = carnot_.table_store();
  table_store->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col3', 'num_groups'])",
          "aggDF = queryDF.Agg(by=lambda r : r.num_groups, fn=lambda r : {'sum' : pl.sum(r.col3)})",
          "aggDF.Result(name='test_output')",
      },
      "\n");
  auto s = carnot_.ExecuteQuery(query);
  VLOG(1) << s.ToString();
  ASSERT_TRUE(s.ok());

  auto output_table = table_store->GetTable("test_output");
  EXPECT_EQ(1, output_table->NumBatches());
  EXPECT_EQ(2, output_table->NumColumns());
  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();

  std::vector<udf::Int64Value> expected_groups = {1, 2, 3};
  std::vector<udf::Int64Value> expected_sum = {13, 129, 24};
  std::unordered_map<int64_t, int64_t> expected = {{1, 13}, {2, 129}, {3, 24}};
  std::unordered_map<int64_t, int64_t> actual;

  for (int i = 0; i < rb1->num_rows(); ++i) {
    auto output_col_grp = rb1->ColumnAt(0);
    auto output_col_agg = rb1->ColumnAt(1);
    auto casted_grp = reinterpret_cast<arrow::Int64Array *>(output_col_grp.get());
    auto casted_agg = reinterpret_cast<arrow::Int64Array *>(output_col_agg.get());

    actual[casted_grp->Value(i)] = casted_agg->Value(i);
  }
  EXPECT_EQ(expected, actual);
}

// TEST_F(CarnotTest, multiple_group_by_test) {
TEST_F(CarnotTest, DISABLED_multiple_group_by_test) {
  auto table = CarnotTestUtils::BigTestTable();

  auto table_store = carnot_.table_store();
  table_store->AddTable("big_test_table", table);
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='big_test_table', select=['time_', 'col3', 'num_groups', "
          "'string_groups'])",
          "aggDF = queryDF.Agg(by=lambda r : [r.num_groups, r.string_groups], fn=lambda r : {'sum' "
          ": pl.sum(r.col3)})",
          "aggDF.Result(name='test_output')",
      },
      "\n");
  auto s = carnot_.ExecuteQuery(query);
  VLOG(1) << s.ToString();
  ASSERT_TRUE(s.ok());

  auto output_table = table_store->GetTable("test_output");
  EXPECT_EQ(1, output_table->NumBatches());
  EXPECT_EQ(3, output_table->NumColumns());
  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
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
    auto casted_num_grp = reinterpret_cast<arrow::Int64Array *>(output_col_num_grp.get());
    auto casted_str_grp = reinterpret_cast<arrow::StringArray *>(output_col_str_grp.get());

    auto casted_agg = reinterpret_cast<arrow::Int64Array *>(output_col_agg.get());
    auto key = Key{casted_num_grp->Value(i), casted_str_grp->GetString(i)};

    actual[key] = casted_agg->Value(i);
  }
  EXPECT_EQ(expected, actual);
}

}  // namespace carnot
}  // namespace pl
