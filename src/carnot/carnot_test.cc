/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <google/protobuf/text_format.h>

#include <algorithm>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/carnot.h"
#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/exec/grpc_router.h"
#include "src/carnot/exec/local_grpc_result_server.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/funcs/funcs.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/base/statuspb/status.pb.h"
#include "src/common/testing/testing.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {

using exec::CarnotTestUtils;
using planner::compiler::Compiler;
using ::testing::UnorderedElementsAre;

class CarnotTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    table_store_ = std::make_shared<table_store::TableStore>();
    result_server_ = std::make_unique<exec::LocalGRPCResultSinkServer>();

    auto func_registry = std::make_unique<px::carnot::udf::Registry>("default_registry");
    funcs::RegisterFuncsOrDie(func_registry.get());
    auto clients_config = std::make_unique<Carnot::ClientsConfig>(Carnot::ClientsConfig{
        [this](const std::string& address, const std::string&) {
          return result_server_.get()->StubGenerator(address);
        },
        [](grpc::ClientContext*) {},
    });
    auto server_config = std::make_unique<Carnot::ServerConfig>();
    server_config->grpc_server_creds = grpc::InsecureServerCredentials();
    server_config->grpc_server_port = 0;
    router_ = &server_config->grpc_router;

    carnot_ = px::carnot::Carnot::Create(sole::uuid4(), std::move(func_registry), table_store_,
                                         std::move(clients_config), std::move(server_config))
                  .ConsumeValueOrDie();
    auto table = CarnotTestUtils::TestTable();
    table_store_->AddTable("test_table", table);
    big_table_ = CarnotTestUtils::BigTestTable();
    table_store_->AddTable("big_test_table", big_table_);
    empty_table_ = table_store::Table::Create(
        "empty_table",
        table_store::schema::Relation({types::UINT128, types::INT64}, {"upid", "cycles"}));
    table_store_->AddTable("empty_table", empty_table_);
    table_store_->AddTable("duration_table", CarnotTestUtils::TestDuration64Table());

    process_stats_table_ = CarnotTestUtils::ProcessStatsTable();
    table_store_->AddTable("process_stats", process_stats_table_);
    http_events_table_ = CarnotTestUtils::HTTPEventsTable();
    table_store_->AddTable("http_events", http_events_table_);
  }

  exec::GRPCRouter* router_;
  std::shared_ptr<table_store::TableStore> table_store_;
  std::shared_ptr<table_store::Table> big_table_;
  std::shared_ptr<table_store::Table> empty_table_;
  std::shared_ptr<table_store::Table> process_stats_table_;
  std::shared_ptr<table_store::Table> http_events_table_;
  std::unique_ptr<Carnot> carnot_;
  std::unique_ptr<exec::LocalGRPCResultSinkServer> result_server_;
};

TEST_F(CarnotTest, basic) {
  std::vector<types::Float64Value> col1_in1 = {0.5, 1.2, 5.3};
  std::vector<types::Float64Value> col1_in2 = {0.1, 5.1};
  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<types::Int64Value> col2_in2 = {5, 6};

  auto query = R"pxl(
import px
df = px.DataFrame(table='test_table', select=['col1', 'col2'])
px.display(df, 'test_output')
)pxl";
  // No time column, doesn't use a time parameter.
  auto query_id = sole::uuid4();
  auto s = carnot_->ExecuteQuery(query, query_id, 0);
  ASSERT_OK(s);

  auto exec_stats = result_server_->exec_stats().ConsumeValueOrDie();
  EXPECT_EQ(5, exec_stats.execution_stats().records_processed());
  EXPECT_EQ(5 * sizeof(double) + 5 * sizeof(int64_t),
            exec_stats.execution_stats().bytes_processed());
  EXPECT_LT(0, exec_stats.execution_stats().timing().execution_time_ns());

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("test_output"));
  auto output_batches = result_server_->query_results("test_output");
  EXPECT_EQ(3, output_batches.size());
  EXPECT_EQ(output_batches[0].num_rows(), 0);

  auto rb1 = output_batches[1];
  EXPECT_TRUE(rb1.ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1.ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto rb2 = output_batches[2];
  EXPECT_TRUE(rb2.ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2.ColumnAt(1)->Equals(types::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, register_metadata) {
  auto callback_calls = 0;
  carnot_->RegisterAgentMetadataCallback(
      [&callback_calls]() -> std::shared_ptr<const md::AgentMetadataState> {
        callback_calls++;
        return nullptr;
      });

  auto query = R"pxl(
import px
df = px.DataFrame(table='test_table', select=['col1', 'col2'])
px.display(df, 'test_output')
)pxl";
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));
  // Check that the function was registered correctly and that it is called once during query
  // execution.
  EXPECT_EQ(1, callback_calls);
}

TEST_F(CarnotTest, assign_literal_to_column) {
  auto query = R"pxl(
import px
df = px.DataFrame(table='test_table')
df = df.agg(count=('col1', px.mean))
df.col2 = 1
px.display(df[['col2']]))pxl";
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("output"));
  auto output_batches = result_server_->query_results("output");
  EXPECT_EQ(2, output_batches.size());

  std::vector<types::Int64Value> expected1 = {1};
  EXPECT_TRUE(output_batches[1].ColumnAt(0)->Equals(
      types::ToArrow(expected1, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, map_test) {
  std::vector<types::Float64Value> col1_in1 = {1.5, 3.2, 8.3};
  std::vector<types::Float64Value> col1_in2 = {5.1, 11.1};

  auto query = R"pxl(
import px
queryDF = px.DataFrame(table='test_table', select=['col1', 'col2'])
queryDF.res = px.add(queryDF.col1, queryDF['col2'])
df = queryDF[['res']]
px.display(df, 'test_output'))pxl";

  // No time column, doesn't use a time parameter.
  auto uuid = sole::uuid4();
  auto s = carnot_->ExecuteQuery(query, uuid, 0);
  ASSERT_OK(s);

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("test_output"));
  auto output_batches = result_server_->query_results("test_output");
  EXPECT_EQ(3, output_batches.size());

  EXPECT_TRUE(output_batches[1].ColumnAt(0)->Equals(
      types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(output_batches[2].ColumnAt(0)->Equals(
      types::ToArrow(col1_in2, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, range_test_multiple_rbs) {
  auto query = R"pxl(
import px
df = px.DataFrame(table='big_test_table', select=['time_','col2', 'col3'], start_time=2, end_time=12)
px.display(df, 'range_output'))pxl";
  // now() not called, doesn't matter what now is.
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("range_output"));
  auto output_batches = result_server_->query_results("range_output");
  EXPECT_EQ(4, output_batches.size());
  EXPECT_EQ(3, output_batches[0].num_columns());

  auto rb1 = output_batches[1];

  std::vector<types::Time64NSValue> col0_out1;
  std::vector<types::Float64Value> col1_out1;
  std::vector<types::Int64Value> col2_out1;
  table_store::Table::Cursor cursor(big_table_.get());
  auto batch = cursor.GetNextRowBatch({0}).ConsumeValueOrDie();
  for (int64_t i = 0; i < batch->ColumnAt(0)->length(); i++) {
    if (CarnotTestUtils::big_test_col1[i].val >= 2 && CarnotTestUtils::big_test_col1[i].val < 12) {
      col0_out1.emplace_back(CarnotTestUtils::big_test_col1[i].val);
      col1_out1.emplace_back(CarnotTestUtils::big_test_col2[i].val);
      col2_out1.emplace_back(CarnotTestUtils::big_test_col3[i].val);
    }
  }

  EXPECT_TRUE(rb1.ColumnAt(0)->Equals(types::ToArrow(col0_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1.ColumnAt(1)->Equals(types::ToArrow(col1_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1.ColumnAt(2)->Equals(types::ToArrow(col2_out1, arrow::default_memory_pool())));

  auto rb2 = output_batches[2];

  std::vector<types::Time64NSValue> col0_out2;
  std::vector<types::Float64Value> col1_out2;
  std::vector<types::Int64Value> col2_out2;
  auto next_batch = cursor.GetNextRowBatch({0}).ConsumeValueOrDie();
  for (int64_t i = batch->ColumnAt(0)->length();
       i < batch->ColumnAt(0)->length() + next_batch->ColumnAt(0)->length(); i++) {
    if (CarnotTestUtils::big_test_col1[i].val >= 2 && CarnotTestUtils::big_test_col1[i].val < 12) {
      col0_out2.emplace_back(CarnotTestUtils::big_test_col1[i].val);
      col1_out2.emplace_back(CarnotTestUtils::big_test_col2[i].val);
      col2_out2.emplace_back(CarnotTestUtils::big_test_col3[i].val);
    }
  }

  EXPECT_TRUE(rb2.ColumnAt(0)->Equals(types::ToArrow(col0_out2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2.ColumnAt(1)->Equals(types::ToArrow(col1_out2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2.ColumnAt(2)->Equals(types::ToArrow(col2_out2, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, range_test_single_rb) {
  std::string query = R"pxl(
import px
queryDF = px.DataFrame(table='big_test_table', select=['time_', 'col2',
'col3'], start_time=$0, end_time=$1)
px.display(queryDF, 'range_output'))pxl";
  int64_t start_time = 9;
  int64_t stop_time = 12;
  query = absl::Substitute(query, start_time, stop_time);
  // now() not called, doesn't matter what now is.
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("range_output"));
  auto output_batches = result_server_->query_results("range_output");
  EXPECT_EQ(2, output_batches.size());
  EXPECT_EQ(3, output_batches[0].num_columns());

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

  auto rb1 = output_batches[1];
  EXPECT_TRUE(rb1.ColumnAt(0)->Equals(types::ToArrow(col0_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1.ColumnAt(1)->Equals(types::ToArrow(col1_out1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1.ColumnAt(2)->Equals(types::ToArrow(col2_out1, arrow::default_memory_pool())));
}

TEST_F(CarnotTest, empty_range_test) {
  // Tests that a table that has no rows that fall within the query's range, doesn't write any
  // rowbatches to the output table.
  std::string query = R"pxl(
import px
queryDF = px.DataFrame(table='big_test_table', select=['time_', 'col2'], start_time=$0, end_time=$1)
px.display(queryDF, 'range_output'))pxl";
  auto time_col = CarnotTestUtils::big_test_col1;
  auto max_time = std::max_element(time_col.begin(), time_col.end());

  int64_t start_time = max_time->val + 1;
  int64_t stop_time = start_time + 10000;
  query = absl::Substitute(query, start_time, stop_time);
  // now() not called, doesn't matter what now is.
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("range_output"));
  auto output_batches = result_server_->query_results("range_output");
  EXPECT_EQ(2, output_batches.size());
  EXPECT_EQ(0, output_batches[0].num_rows());
  EXPECT_EQ(0, output_batches[1].num_rows());
}

class CarnotRangeTest
    : public CarnotTest,
      public ::testing::WithParamInterface<std::tuple<types::Int64Value, size_t, size_t, bool>> {
 protected:
  void SetUp() {
    CarnotTest::SetUp();

    auto max_time = CarnotTestUtils::big_test_col1[CarnotTestUtils::big_test_col1.size() - 1];
    now_time_ = max_time.val + 1;
  }
  size_t num_batches;
  size_t num_rows;
  int64_t now_time_;
};

TEST_F(CarnotTest, group_by_none_agg_test) {
  auto query = R"pxl(
import px
queryDF = px.DataFrame(table='big_test_table', select=['time_', 'col2', 'col3'])
aggDF = queryDF.agg(
  mean=('col2', px.mean),
  count=('col3', px.count),
  min=('col2', px.min),
  max=('col3', px.max),
  sum=('col3', px.sum),
  sum2=('col3', px.sum),
)
px.display(aggDF, 'test_output'))pxl";
  // now() not called, doesn't matter what now is.
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("test_output"));
  auto output_batches = result_server_->query_results("test_output");
  EXPECT_EQ(2, output_batches.size());
  EXPECT_EQ(6, output_batches[0].num_columns());
  // First batch is a zero row batch.
  EXPECT_EQ(output_batches[0].num_rows(), 0);

  auto rb1 = output_batches[1];

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

  EXPECT_TRUE(rb1.ColumnAt(0)->Equals(
      types::ToArrow(std::vector<types::Float64Value>({types::Float64Value(col2_expected_mean)}),
                     arrow::default_memory_pool())));

  EXPECT_TRUE(rb1.ColumnAt(1)->Equals(
      types::ToArrow(std::vector<types::Int64Value>({types::Int64Value(col3_expected_count)}),
                     arrow::default_memory_pool())));

  EXPECT_TRUE(rb1.ColumnAt(2)->Equals(
      types::ToArrow(std::vector<types::Float64Value>({types::Float64Value(col2_expected_min)}),
                     arrow::default_memory_pool())));

  EXPECT_TRUE(rb1.ColumnAt(3)->Equals(
      types::ToArrow(std::vector<types::Int64Value>({types::Int64Value(col3_expected_max)}),
                     arrow::default_memory_pool())));

  EXPECT_TRUE(rb1.ColumnAt(4)->Equals(
      types::ToArrow(std::vector<types::Int64Value>({types::Int64Value(col3_expected_sum)}),
                     arrow::default_memory_pool())));

  // Contents of column 4 and column 5 are the same.
  EXPECT_TRUE(rb1.ColumnAt(5)->Equals(
      types::ToArrow(std::vector<types::Int64Value>({types::Int64Value(col3_expected_sum)}),
                     arrow::default_memory_pool())));
}

TEST_F(CarnotTest, group_by_test) {
  auto query = R"pxl(
import px
queryDF = px.DataFrame(table='big_test_table', select=['time_', 'col3', 'num_groups', 'string_groups'])
aggDF = queryDF.groupby(['num_groups', 'string_groups']).agg(sum=('col3', px.sum))
px.display(aggDF, 'test_output'))pxl";
  // now() not called, doesn't matter what now is.
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("test_output"));
  auto output_batches = result_server_->query_results("test_output");
  EXPECT_EQ(2, output_batches.size());
  EXPECT_EQ(3, output_batches[0].num_columns());

  // First batch is a zero row batch.
  EXPECT_EQ(output_batches[0].num_rows(), 0);

  auto rb1 = output_batches[1];

  struct Key {
    int64_t num_group;
    std::string string_group;

    bool operator<(const Key& other) const {
      return num_group < other.num_group ||
             (num_group == other.num_group && string_group < other.string_group);
    }
    bool operator==(const Key& other) const {
      return (num_group == other.num_group) && string_group == other.string_group;
    }
  };

  std::map<Key, int64_t> expected = {
      {Key{1, "sum"}, 6},  {Key{1, "mean"}, 7},  {Key{3, "sum"}, 24},
      {Key{2, "sum"}, 60}, {Key{2, "mean"}, 69},
  };

  std::map<Key, int64_t> actual;
  for (int i = 0; i < rb1.num_rows(); ++i) {
    auto output_col_num_grp = rb1.ColumnAt(0);
    auto output_col_str_grp = rb1.ColumnAt(1);
    auto output_col_agg = rb1.ColumnAt(2);
    auto casted_num_grp = static_cast<arrow::Int64Array*>(output_col_num_grp.get());
    auto casted_str_grp = static_cast<arrow::StringArray*>(output_col_str_grp.get());

    auto casted_agg = static_cast<arrow::Int64Array*>(output_col_agg.get());
    auto key = Key{casted_num_grp->Value(i), casted_str_grp->GetString(i)};

    actual[key] = casted_agg->Value(i);
  }
  EXPECT_EQ(expected, actual);
}

TEST_F(CarnotTest, string_filter) {
  std::string query = R"pxl(
import px
queryDF = px.DataFrame(table='big_test_table', select=['time_', 'col2', 'col3', 'num_groups', 'string_groups'])
mapDF = queryDF[queryDF['string_groups'] == 'sum']
px.display(mapDF, 'test_output'))pxl";

  auto comparison_column = CarnotTestUtils::big_test_strings;
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("test_output"));
  auto output_batches = result_server_->query_results("test_output");
  EXPECT_EQ(4, output_batches.size());
  EXPECT_EQ(5, output_batches[0].num_columns());
  // First batch is a zero row batch.
  auto rb = output_batches[0];
  EXPECT_EQ(rb.ColumnAt(0)->length(), 0);
  EXPECT_EQ(rb.ColumnAt(1)->length(), 0);

  // Iterate through the batches.
  for (size_t i = 0; i < CarnotTestUtils::split_idx.size(); i++) {
    // Iterate through the column.
    const auto& cur_split = CarnotTestUtils::split_idx[i];
    int64_t left = cur_split.first;
    int64_t right = cur_split.second;
    std::vector<types::Time64NSValue> time_out;
    std::vector<types::Float64Value> col2_out;
    std::vector<types::Int64Value> col3_out;
    std::vector<types::Int64Value> groups_out;
    std::vector<types::StringValue> strings_out;
    for (int64_t j = left; j < right; j++) {
      if (comparison_column[j] == "sum") {
        time_out.push_back(CarnotTestUtils::big_test_col1[j]);
        col2_out.push_back(CarnotTestUtils::big_test_col2[j]);
        col3_out.push_back(CarnotTestUtils::big_test_col3[j]);
        groups_out.push_back(CarnotTestUtils::big_test_groups[j]);
        strings_out.push_back(CarnotTestUtils::big_test_strings[j]);
      }
    }
    auto rb = output_batches[i + 1];
    EXPECT_TRUE(rb.ColumnAt(0)->Equals(types::ToArrow(time_out, arrow::default_memory_pool())));
    EXPECT_TRUE(rb.ColumnAt(1)->Equals(types::ToArrow(col2_out, arrow::default_memory_pool())));
    EXPECT_TRUE(rb.ColumnAt(2)->Equals(types::ToArrow(col3_out, arrow::default_memory_pool())));
    EXPECT_TRUE(rb.ColumnAt(3)->Equals(types::ToArrow(groups_out, arrow::default_memory_pool())));
    EXPECT_TRUE(rb.ColumnAt(4)->Equals(types::ToArrow(strings_out, arrow::default_memory_pool())));
  }
}
class CarnotLimitTest : public CarnotTest,
                        public ::testing::WithParamInterface<std::tuple<int64_t, int64_t>> {
 protected:
  void SetUp() { CarnotTest::SetUp(); }
};

TEST_P(CarnotLimitTest, limit) {
  std::string query = R"pxl(
import px
queryDF = px.DataFrame(table='big_test_table', select=['time_', 'col2'])
mapDF = queryDF.head(n=$0)
px.display(mapDF, 'test_output'))pxl";
  int64_t num_rows;
  int64_t expected_num_batches;
  std::tie(expected_num_batches, num_rows) = GetParam();
  query = absl::Substitute(query, num_rows);
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("test_output"));
  auto output_batches = result_server_->query_results("test_output");
  EXPECT_EQ(expected_num_batches + 1, output_batches.size());
  EXPECT_EQ(2, output_batches[0].num_columns());

  // First batch is a zero row batch.
  EXPECT_EQ(output_batches[0].num_rows(), 0);

  // Iterate through the batches.
  for (int64_t i = 0; i < expected_num_batches; i++) {
    // Iterate through the column.
    const auto& cur_split = CarnotTestUtils::split_idx[i];
    int64_t left = cur_split.first;
    int64_t right = cur_split.second;
    std::vector<types::Time64NSValue> time_out;
    std::vector<types::Float64Value> col2_out;
    for (int64_t j = left; j < right; j++) {
      if (j >= num_rows) {
        break;
      }
      time_out.push_back(CarnotTestUtils::big_test_col1[j]);
      col2_out.push_back(CarnotTestUtils::big_test_col2[j]);
    }
    auto rb = output_batches[i + 1];
    EXPECT_TRUE(rb.ColumnAt(0)->Equals(types::ToArrow(time_out, arrow::default_memory_pool())));
    EXPECT_TRUE(rb.ColumnAt(1)->Equals(types::ToArrow(col2_out, arrow::default_memory_pool())));
  }
}

// {expected_num_batches, num_rows}
INSTANTIATE_TEST_SUITE_P(CarnotLimitTestSuite, CarnotLimitTest,
                         ::testing::ValuesIn(std::vector<std::tuple<int64_t, int64_t>>{
                             {1, 2}, {2, 4}, {3, 7}}));

TEST_F(CarnotTest, multiple_display_calls) {
  std::string query = R"pxl(
import px
queryDF = px.DataFrame(table='big_test_table', select=['time_', 'col3', 'num_groups', 'string_groups'])
mapDF = queryDF[['col3', 'num_groups']]
mapDF['lt'] = mapDF['col3'] < $0
mapDF['gt'] = mapDF['num_groups'] > $1
df = mapDF[['lt', 'gt']]
px.display(df, 'test_output')
x = queryDF[queryDF['num_groups'] > $2]
px.display(x, 'filtered_output'))pxl";
  // Values to test on.
  int64_t col3_lt_val = 12;
  int64_t num_groups_gt_val = 1;
  int64_t groups_val = 1;
  query = absl::Substitute(query, col3_lt_val, num_groups_gt_val, groups_val);
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));

  // test the original output
  VLOG(1) << "test the original output";

  EXPECT_THAT(result_server_->output_tables(),
              UnorderedElementsAre("test_output", "filtered_output"));
  auto output_batches = result_server_->query_results("test_output");
  EXPECT_EQ(4, output_batches.size());
  EXPECT_EQ(2, output_batches[0].num_columns());

  // First batch is a zero row batch.
  EXPECT_EQ(output_batches[0].num_rows(), 0);
  auto rb1 = output_batches[1];

  auto col3 = CarnotTestUtils::big_test_col3;
  auto col_num_groups = CarnotTestUtils::big_test_groups;
  std::vector<types::BoolValue> lt_exp;
  std::vector<types::BoolValue> gt_exp;

  for (int64_t i = 0; i < rb1.num_rows(); i++) {
    lt_exp.emplace_back(col3[i] < col3_lt_val);
    gt_exp.emplace_back(col_num_groups[i] > num_groups_gt_val);
  }
  EXPECT_TRUE(rb1.ColumnAt(0)->Equals(types::ToArrow(lt_exp, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1.ColumnAt(1)->Equals(types::ToArrow(gt_exp, arrow::default_memory_pool())));

  // test the filtered_output
  output_batches = result_server_->query_results("filtered_output");
  EXPECT_EQ(4, output_batches.size());
  EXPECT_EQ(4, output_batches[0].num_columns());

  // iterate through the batches
  for (size_t i = 0; i < CarnotTestUtils::split_idx.size(); i++) {
    // iterate through the column
    const auto& cur_split = CarnotTestUtils::split_idx[i];
    int64_t left = cur_split.first;
    int64_t right = cur_split.second;
    std::vector<types::Time64NSValue> time_out;
    std::vector<types::StringValue> strings_out;
    std::vector<types::Int64Value> col3_out;
    std::vector<types::Int64Value> groups_out;
    for (int64_t j = left; j < right; j++) {
      if (CarnotTestUtils::big_test_groups[j].val > groups_val) {
        time_out.push_back(CarnotTestUtils::big_test_col1[j]);
        col3_out.push_back(CarnotTestUtils::big_test_col3[j]);
        groups_out.push_back(CarnotTestUtils::big_test_groups[j]);
        strings_out.push_back(CarnotTestUtils::big_test_strings[j]);
      }
    }
    auto rb = output_batches[i + 1];
    EXPECT_TRUE(rb.ColumnAt(0)->Equals(types::ToArrow(time_out, arrow::default_memory_pool())));
    EXPECT_TRUE(rb.ColumnAt(1)->Equals(types::ToArrow(col3_out, arrow::default_memory_pool())));
    EXPECT_TRUE(rb.ColumnAt(2)->Equals(types::ToArrow(groups_out, arrow::default_memory_pool())));
    EXPECT_TRUE(rb.ColumnAt(3)->Equals(types::ToArrow(strings_out, arrow::default_memory_pool())));
  }
}

// Test to see whether we can pass logical plan into Carnot instead of query.
TEST_F(CarnotTest, pass_logical_plan) {
  std::string query = R"pxl(
import px
queryDF = px.DataFrame(table='test_table', select=['col1', 'col2'])
queryDF['res'] = px.add(queryDF['col1'], queryDF['col2'])
df = queryDF[['res']]
px.display(df, '$0'))pxl";
  Compiler compiler;
  int64_t current_time = 0;
  std::string logical_plan_table_name = "logical_plan";
  std::string query_table_name = "query";

  std::unique_ptr<planner::RegistryInfo> registry_info =
      udfexporter::ExportUDFInfo().ConsumeValueOrDie();

  planner::CompilerState compiler_state(
      table_store_->GetRelationMap(), planner::SensitiveColumnMap{}, registry_info.get(),
      current_time,
      /* max_output_rows_per_table */ 0, "result_addr", "result_ssl_targetname",
      planner::RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});
  planpb::Plan plan =
      compiler.Compile(absl::Substitute(query, logical_plan_table_name), &compiler_state)
          .ConsumeValueOrDie();

  // Ensure that Query and LogicalPlan paths yield the same result.
  ASSERT_OK(carnot_->ExecutePlan(plan, sole::uuid4()));

  ASSERT_OK(carnot_->ExecuteQuery(absl::Substitute(query, query_table_name), sole::uuid4(),
                                  current_time));

  auto plan_table_batches = result_server_->query_results("logical_plan");
  auto query_table_batches = result_server_->query_results("query");
  EXPECT_EQ(plan_table_batches.size(), query_table_batches.size());
  EXPECT_EQ(plan_table_batches[0].num_columns(), query_table_batches[0].num_columns());
  EXPECT_EQ(1, plan_table_batches[0].num_columns());

  for (size_t i = 0; i < plan_table_batches.size(); ++i) {
    auto plan_rb = plan_table_batches[i];
    auto query_rb = query_table_batches[i];
    for (int64_t j = 0; j < plan_rb.num_columns(); ++j) {
      VLOG(2) << absl::Substitute("Batch $0; Column $1", i, j);
      EXPECT_TRUE(plan_rb.ColumnAt(j)->Equals(query_rb.ColumnAt(j)));
    }
  }
}

constexpr char kEmptySourcePlan[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 1
      sorted_children: 2
    }
    nodes {
      id: 2
      sorted_parents: 1
    }
  }
  nodes {
    id: 1
    op {
      op_type: EMPTY_SOURCE_OPERATOR
      empty_source_op {
        column_names: "cpu0"
        column_names: "cpu1"
        column_types: INT64
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "out_table"
          column_names: "cpu0"
          column_names: "cpu1"
          column_types: INT64
          column_types: FLOAT64
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
}
)proto";
TEST_F(CarnotTest, empty_source_test) {
  planpb::Plan plan;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kEmptySourcePlan, &plan));

  ASSERT_OK(carnot_->ExecutePlan(plan, sole::uuid4()));

  EXPECT_THAT(result_server_->output_tables(), UnorderedElementsAre("out_table"));
  auto output_batches = result_server_->query_results("out_table");
  ASSERT_EQ(2, output_batches.size());
  EXPECT_EQ(2, output_batches[0].num_columns());

  for (const auto& rb : output_batches) {
    EXPECT_TRUE(rb.ColumnAt(0)->Equals(
        types::ToArrow(std::vector<types::Int64Value>({}), arrow::default_memory_pool())));
    EXPECT_TRUE(rb.ColumnAt(1)->Equals(
        types::ToArrow(std::vector<types::Float64Value>({}), arrow::default_memory_pool())));
  }
}

const char kPxCluster[] = R"pxl(
import px

ns_per_ms = 1000 * 1000
ns_per_s = 1000 * ns_per_ms
# Window size to use on time_ column for bucketing.
window_ns = px.DurationNanos(10 * ns_per_s)
# Flag to filter out requests that come from an unresolvable IP.
filter_unresolved_inbound = True
# Flag to filter out health checks from the data.
filter_health_checks = True
# Flag to filter out ready checks from the data.
filter_ready_checks = True


def nodes_for_cluster(start_time: str):
    ''' Gets a list of nodes in the current cluster since `start_time`.
    Args:
    @start_time Start time of the data to examine.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df.node = df.ctx['node_name']
    df.pod = df.ctx['pod_name']
    agg = df.groupby(['node', 'pod']).agg()
    nodes = agg.groupby('node').agg(pod_count=('pod', px.count))
    process_stats = process_stats_by_entity(start_time, 'node')
    output = process_stats.merge(nodes, how='inner', left_on='node', right_on='node',
                                 suffixes=['', '_x'])
    return output[['node', 'cpu_usage', 'pod_count']]


def process_stats_by_entity(start_time: str, entity: str):
    ''' Gets the windowed process stats (CPU, memory, etc) per node or pod.
    Args:
    @start_time Starting time of the data to examine.
    @entity: Either pod or node_name.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df[entity] = df.ctx[entity]
    df.timestamp = px.bin(df.time_, window_ns)
    # First calculate CPU usage by process (UPID) in each k8s_object
    # over all windows.
    df = df.groupby([entity, 'upid', 'timestamp']).agg(
        rss=('rss_bytes', px.mean),
        vsize=('vsize_bytes', px.mean),
        # The fields below are counters, so we take the min and the max to subtract them.
        cpu_utime_ns_max=('cpu_utime_ns', px.max),
        cpu_utime_ns_min=('cpu_utime_ns', px.min),
        cpu_ktime_ns_max=('cpu_ktime_ns', px.max),
        cpu_ktime_ns_min=('cpu_ktime_ns', px.min),
        read_bytes_max=('read_bytes', px.max),
        read_bytes_min=('read_bytes', px.min),
        write_bytes_max=('write_bytes', px.max),
        write_bytes_min=('write_bytes', px.min),
        rchar_bytes_max=('rchar_bytes', px.max),
        rchar_bytes_min=('rchar_bytes', px.min),
        wchar_bytes_max=('wchar_bytes', px.max),
        wchar_bytes_min=('wchar_bytes', px.min),
    )
    # Next calculate cpu usage and memory stats per window.
    df.cpu_utime_ns = df.cpu_utime_ns_max - df.cpu_utime_ns_min
    df.cpu_ktime_ns = df.cpu_ktime_ns_max - df.cpu_ktime_ns_min
    df.read_bytes = df.read_bytes_max - df.read_bytes_min
    df.write_bytes = df.write_bytes_max - df.write_bytes_min
    df.rchar_bytes = df.rchar_bytes_max - df.rchar_bytes_min
    df.wchar_bytes = df.wchar_bytes_max - df.wchar_bytes_min
    # Sum by UPID.
    df = df.groupby([entity, 'timestamp']).agg(
        cpu_ktime_ns=('cpu_ktime_ns', px.sum),
        cpu_utime_ns=('cpu_utime_ns', px.sum),
        read_bytes=('read_bytes', px.sum),
        write_bytes=('write_bytes', px.sum),
        rchar_bytes=('rchar_bytes', px.sum),
        wchar_bytes=('wchar_bytes', px.sum),
        rss=('rss', px.sum),
        vsize=('vsize', px.sum),
    )
    df.actual_disk_read_throughput = df.read_bytes / window_ns
    df.actual_disk_write_throughput = df.write_bytes / window_ns
    df.total_disk_read_throughput = df.rchar_bytes / window_ns
    df.total_disk_write_throughput = df.wchar_bytes / window_ns
    # Now take the mean value over the various timestamps.
    df = df.groupby(entity).agg(
        cpu_ktime_ns=('cpu_ktime_ns', px.mean),
        cpu_utime_ns=('cpu_utime_ns', px.mean),
        actual_disk_read_throughput=('actual_disk_read_throughput', px.mean),
        actual_disk_write_throughput=('actual_disk_write_throughput', px.mean),
        total_disk_read_throughput=('total_disk_read_throughput', px.mean),
        total_disk_write_throughput=('total_disk_write_throughput', px.mean),
        avg_rss=('rss', px.mean),
        avg_vsize=('vsize', px.mean),
    )
    # Finally, calculate total (kernel + user time)  percentage used over window.
    df.cpu_usage = px.Percent((df.cpu_ktime_ns + df.cpu_utime_ns) / window_ns)
    return df.drop(['cpu_ktime_ns', 'cpu_utime_ns'])


def pods_for_cluster(start_time: str):
    ''' A list of pods in `namespace`.
    Args:
    @start_time: The timestamp of data to start at.
    @namespace: The name of the namespace to filter on.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df.pod = df.ctx['pod_name']
    df.node = df.ctx['node_name']
    df.container = df.ctx['container_name']
    df = df.groupby(['pod', 'node', 'container']).agg()
    df = df.groupby(['pod', 'node']).agg(container_count=('container', px.count))
    df.start_time = px.pod_name_to_start_time(df.pod)
    df.status = px.pod_name_to_status(df.pod)
    process_stats = process_stats_by_entity(start_time, 'pod')
    output = process_stats.merge(df, how='inner', left_on='pod', right_on='pod',
                                 suffixes=['', '_x'])
    return output[['pod', 'cpu_usage', 'total_disk_read_throughput',
                   'total_disk_write_throughput', 'container_count',
                   'node', 'start_time', 'status']]


def namespaces_for_cluster(start_time: str):
    ''' Gets a overview of namespaces in the current cluster since `start_time`.
    Args:
    @start_time Start time of the data to examine.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df.service = df.ctx['service_name']
    df.pod = df.ctx['pod_name']
    df.namespace = df.ctx['namespace']
    agg = df.groupby(['service', 'pod', 'namespace']).agg()
    pod_count = agg.groupby(['namespace', 'pod']).agg()
    pod_count = pod_count.groupby('namespace').agg(pod_count=('pod', px.count))
    svc_count = agg.groupby(['namespace', 'service']).agg()
    svc_count = svc_count.groupby('namespace').agg(service_count=('service', px.count))
    pod_and_svc_count = pod_count.merge(svc_count, how='inner',
                                        left_on='namespace', right_on='namespace',
                                        suffixes=['', '_x'])
    process_stats = process_stats_by_entity(start_time, 'namespace')
    output = process_stats.merge(pod_and_svc_count, how='inner', left_on='namespace',
                                 right_on='namespace', suffixes=['', '_y'])
    return output[['namespace', 'pod_count', 'service_count', 'avg_vsize', 'avg_rss']]


def services_for_cluster(start_time: str):
    ''' Get an overview of the services in the current cluster.
    Args:
    @start_time: The timestamp of data to start at.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df.service = df.ctx['service']
    df = df[df.service != '']
    df.pod = df.ctx['pod']
    df = df.groupby(['service', 'pod']).agg()
    df = df.groupby('service').agg(pod_count=('pod', px.count))
    service_let = inbound_service_let_summary(start_time)
    joined = df.merge(service_let, how='inner', left_on='service', right_on='service',
                      suffixes=['', '_x'])
    return joined.drop('service_x')


def inbound_service_let_summary(start_time: str):
    ''' Compute a summary of traffic by requesting service, for requests
        on services in the current cluster..
    Args:
    @start_time: The timestamp of data to start at.
    '''
    df = inbound_service_let_helper(start_time)
    df = df[df.remote_addr != '']
    df.responder = df.service
    per_ns_df = df.groupby(['timestamp', 'service']).agg(
        throughput_total=('latency', px.count),
        inbound_bytes_total=('req_size', px.sum),
        outbound_bytes_total=('resp_size', px.sum)
    )
    per_ns_df.request_throughput = per_ns_df.throughput_total / window_ns
    per_ns_df.inbound_throughput = per_ns_df.inbound_bytes_total / window_ns
    per_ns_df.outbound_throughput = per_ns_df.inbound_bytes_total / window_ns
    per_ns_df = per_ns_df.groupby('service').agg(
        request_throughput=('request_throughput', px.mean),
        inbound_throughput=('inbound_throughput', px.mean),
        outbound_throughput=('outbound_throughput', px.mean)
    )
    quantiles_df = df.groupby('service').agg(
        latency=('latency', px.quantiles)
        error_rate=('failure', px.mean),
    )
    quantiles_df.error_rate = px.Percent(quantiles_df.error_rate)
    joined = per_ns_df.merge(quantiles_df, left_on='service',
                             right_on='service', how='inner',
                             suffixes=['', '_x'])
    return joined[['service', 'latency', 'request_throughput', 'error_rate',
                   'inbound_throughput', 'outbound_throughput']]


def inbound_service_let_helper(start_time: str):
    ''' Compute the let as a timeseries for requests received or by services in `namespace`.
    Args:
    @start_time: The timestamp of data to start at.
    @namespace: The namespace to filter on.
    @groupby_cols: The columns to group on.
    '''
    df = px.DataFrame(table='http_events', start_time=start_time)
    df.service = df.ctx['service']
    df.pod = df.ctx['pod_name']
    df = df[df.service != '']
    df.latency = df.latency
    df.timestamp = px.bin(df.time_, window_ns)
    df.req_size = px.Bytes(px.length(df.req_body))
    df.resp_size = px.Bytes(px.length(df.resp_body))
    df.failure = df.resp_status >= 400
    filter_out_conds = ((df.req_path != '/health' or not filter_health_checks) and (
        df.req_path != '/readyz' or not filter_ready_checks)) and (
        df['remote_addr'] != '-' or not filter_unresolved_inbound)
    df = df[filter_out_conds]
    return df


def inbound_let_service_graph(start_time: str):
    ''' Compute a summary of traffic by requesting service, for requests on services
        in the current cluster. Similar to `inbound_let_summary` but also breaks down
        by pod in addition to service.
    Args:
    @start_time: The timestamp of data to start at.
    '''
    df = inbound_service_let_helper(start_time)
    df = df.groupby(['timestamp', 'service', 'remote_addr', 'pod']).agg(
        latency_quantiles=('latency', px.quantiles),
        error_rate=('failure', px.mean),
        throughput_total=('latency', px.count),
        inbound_bytes_total=('req_size', px.sum),
        outbound_bytes_total=('resp_size', px.sum)
    )
    df.latency_p50 = px.DurationNanos(px.floor(px.pluck_float64(df.latency_quantiles, 'p50')))
    df.latency_p90 = px.DurationNanos(px.floor(px.pluck_float64(df.latency_quantiles, 'p90')))
    df.latency_p99 = px.DurationNanos(px.floor(px.pluck_float64(df.latency_quantiles, 'p99')))
    df = df[df.remote_addr != '']
    df.responder_pod = df.pod
    df.requestor_pod_id = px.ip_to_pod_id(df.remote_addr)
    df.requestor_pod = px.pod_id_to_pod_name(df.requestor_pod_id)
    df.responder_service = df.service
    df.requestor_service = px.pod_id_to_service_name(df.requestor_pod_id)
    df.request_throughput = df.throughput_total / window_ns
    df.inbound_throughput = df.inbound_bytes_total / window_ns
    df.outbound_throughput = df.outbound_bytes_total / window_ns
    df.error_rate = px.Percent(df.error_rate)
    return df.groupby(['responder_pod', 'requestor_pod', 'responder_service',
                       'requestor_service']).agg(
        latency_p50=('latency_p50', px.mean),
        latency_p90=('latency_p90', px.mean),
        latency_p99=('latency_p99', px.mean),
        request_throughput=('request_throughput', px.mean),
        error_rate=('error_rate', px.mean),
        inbound_throughput=('inbound_throughput', px.mean),
        outbound_throughput=('outbound_throughput', px.mean),
        throughput_total=('throughput_total', px.sum)
    )

start_time = '-5m'
px.display(inbound_let_service_graph(start_time))
px.display(nodes_for_cluster(start_time))
px.display(namespaces_for_cluster(start_time))
px.display(services_for_cluster(start_time))
px.display(pods_for_cluster(start_time))
)pxl";

TEST_F(CarnotTest, multiple_queries) {
  auto exec_query_work = [&]() {
    // No time column, doesn't use a time parameter.
    ASSERT_OK(carnot_->ExecuteQuery(kPxCluster, sole::uuid4(), 0));
  };

  // TSAN should catch if running multiple queries at once mutate any shared state.
  // This caught a bug where schema was shared between queries causing weird non-deterministic bugs.
  auto num_simul = 5;
  std::vector<std::thread> threads;
  for (int i = 0; i < num_simul; ++i) {
    threads.emplace_back(exec_query_work);
  }

  for (auto& t : threads) {
    t.join();
  }
}

TEST_F(CarnotTest, init_args) {
  std::string query = R"pxl(
import px
df = px.DataFrame('big_test_table')
df.match = px.regex_match('pattern', df.string_groups)
px.display(df))pxl";
  ASSERT_OK(carnot_->ExecuteQuery(query, sole::uuid4(), 0));
}

constexpr char kErrorNodePlan[] = R"proto(
execution_status_destinations {
  grpc_address: "error_address"
  ssl_targetname: "result_ssltarget"
}
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 1
      sorted_children: 2
    }
    nodes {
      id: 2
      sorted_parents: 1
      sorted_children: 3
    }
    nodes {
      id: 3
      sorted_parents: 2
    }
  }
  nodes {
    id: 1
    op {
      op_type: EMPTY_SOURCE_OPERATOR
      empty_source_op {
        column_names: "cpu0"
        column_types: INT64
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
            name: "upid_to_service_name"
            args {
              column {
                node: 1
                index: 0
              }
            }
            args_data_types: UINT128
          }
        }
        column_names: "service"
      }
    }
  }
  nodes {
    id: 3
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "error_address"
        output_table {
          table_name: "out_table"
          column_names: "cpu0"
          column_types: INT64
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
}
)proto";
TEST_F(CarnotTest, result_server_receives_execution_errors_created_by_carnot) {
  planpb::Plan plan;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kErrorNodePlan, &plan));

  ASSERT_NOT_OK(carnot_->ExecutePlan(plan, sole::uuid4()));

  auto errors = result_server_->exec_errors();
  EXPECT_EQ(errors.size(), 1);
  EXPECT_THAT(errors[0].DebugString(),
              ::testing::MatchesRegex(".*No UDF matching upid_to_service_name.*"));
}

constexpr char kGRPCSourcePlan[] = R"proto(
execution_status_destinations {
  grpc_address: "result_addr"
  ssl_targetname: "result_ssltarget"
}
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 1
      sorted_children: 2
    }
    nodes {
      id: 2
      sorted_parents: 1
    }
  }
  nodes {
    id: 1
    op {
      op_type: GRPC_SOURCE_OPERATOR
      grpc_source_op {
        column_names: "cpu0"
        column_types: INT64
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "out_table"
          column_names: "cpu0"
          column_types: INT64
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
}
)proto";

struct TransferResultChunkTestCase {
  std::string name;
  std::string plan;
  // The number of results sent over to the local results server by Carnot that are not zero batch
  // results. Carnot maintains connections by sending over zero-batch results and the frequency is
  // dependent on the machine running the test. We maintain test-determinism by only counting a
  // result if it is not a row batch, has rows, or is marked eow/eos.
  int64_t num_non_zero_results;
  // the regex for the error. If the string is empty, the test does not consider this an error.
  // Will be tested agains the error from the plan and the exec_errors.
  std::string error_regex;
  // Each element of each element is the string repr of the proto message to send over the stream.
  std::vector<std::vector<std::string>> transfer_result_chunk_streams;
};

class TransferResultChunkTests : public CarnotTest,
                                 public ::testing::WithParamInterface<TransferResultChunkTestCase> {
};
TEST_P(TransferResultChunkTests, send_and_forward_messages) {
  grpc::ServerBuilder builder;
  builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials());
  builder.RegisterService(router_);
  auto grpc_server = builder.BuildAndStart();

  grpc::ChannelArguments args;
  auto input_stub = px::carnotpb::ResultSinkService::NewStub(grpc_server->InProcessChannel(args));

  auto tc = GetParam();
  for (const auto& requests : tc.transfer_result_chunk_streams) {
    grpc::ClientContext ctx;
    carnotpb::TransferResultChunkResponse response;
    auto writer = input_stub->TransferResultChunk(&ctx, &response);
    for (const auto& req_str : requests) {
      carnotpb::TransferResultChunkRequest req;
      ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(req_str, &req));
      EXPECT_TRUE(writer->Write(req));
    }
    writer->WritesDone();
    auto writer_s = writer->Finish();
    EXPECT_TRUE(writer_s.ok()) << writer_s.error_message();
  }

  planpb::Plan plan;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(tc.plan, &plan));

  if (tc.error_regex.empty()) {
    ASSERT_OK(carnot_->ExecutePlan(plan, sole::uuid{1, 1}));
    EXPECT_TRUE(result_server_->exec_errors().empty());
  } else {
    EXPECT_THAT(carnot_->ExecutePlan(plan, sole::uuid{1, 1}).status().msg(),
                ::testing::MatchesRegex(tc.error_regex));
    ASSERT_EQ(result_server_->exec_errors().size(), 1);
    EXPECT_THAT(result_server_->exec_errors()[0].DebugString(),
                ::testing::MatchesRegex(tc.error_regex));
  }

  size_t num_non_zero = result_server_->raw_query_results().size();
  for (const auto& result : result_server_->raw_query_results()) {
    if (!result.has_query_result() || !result.query_result().has_row_batch()) {
      continue;
    }
    const auto& rb = result.query_result().row_batch();
    if (rb.num_rows() == 0 && !rb.eos() && !rb.eow()) {
      --num_non_zero;
    }
  }

  EXPECT_EQ(num_non_zero, tc.num_non_zero_results);
};

INSTANTIATE_TEST_SUITE_P(TransferResultChunks, TransferResultChunkTests,
                         ::testing::Values(
                             TransferResultChunkTestCase{
                                 "end2end",
                                 kGRPCSourcePlan,
                                 /* num_non_zero_results */ 3,
                                 "",
                                 {
                                     {
                                         R"proto(
                                      address: "foo"
                                      query_id {
                                        high_bits: 1
                                        low_bits: 1
                                      }
                                      initiate_conn {}
                                      )proto",
                                         R"proto(
                                      address: "foo"
                                      query_id {
                                        high_bits: 1
                                        low_bits: 1
                                      }
                                      query_result {
                                        grpc_source_id: 1
                                        row_batch {
                                          cols {
                                            int64_data {
                                              data: 1
                                            }
                                          }
                                          num_rows: 1
                                          eow: true
                                          eos: true
                                        }
                                      })proto",
                                         R"proto(
                                      address: "foo"
                                      query_id {
                                        high_bits: 1
                                        low_bits: 1
                                      }
                                      execution_and_timing_info {
                                        execution_stats {
                                          timing {
                                            execution_time_ns: 1234
                                            compilation_time_ns: 123
                                          }
                                          bytes_processed: 1
                                          records_processed: 1
                                        }
                                      })proto",
                                     },
                                 },
                             },
                             TransferResultChunkTestCase{
                                 "timeout_does_not_throw_error",
                                 kGRPCSourcePlan,
                                 /* num_non_zero_results */ 4,
                                 "",
                                 {
                                     {
                                         R"proto(
                                      address: "foo"
                                      query_id {
                                        high_bits: 1
                                        low_bits: 1
                                      }
                                      initiate_conn {}
                                      )proto",
                                         R"proto(
                                      address: "foo"
                                      query_id {
                                        high_bits: 1
                                        low_bits: 1
                                      }
                                      query_result {
                                        grpc_source_id: 1
                                        row_batch {
                                          cols {
                                            int64_data {
                                              data: 1
                                            }
                                          }
                                          num_rows: 1
                                        }
                                      })proto",
                                     },
                                 },
                             },

                             TransferResultChunkTestCase{
                                 "receive_error_from_parent_agent",
                                 kGRPCSourcePlan,
                                 /* num_non_zero_results */ 3,
                                 ".*didnt process data.*",
                                 {
                                     {
                                         R"proto(
                                      address: "foo"
                                      query_id {
                                        high_bits: 1
                                        low_bits: 1
                                      }
                                      initiate_conn {}
                                      )proto",
                                         R"proto(
                                      address: "foo"
                                      query_id {
                                        high_bits: 1
                                        low_bits: 1
                                      }
                                      execution_error {
                                        err_code: INTERNAL
                                        msg: "didnt process data"
                                      })proto",
                                     },
                                 },
                             }),
                         [](const ::testing::TestParamInfo<TransferResultChunkTestCase>& info) {
                           return info.param.name;
                         });

}  // namespace carnot
}  // namespace px
