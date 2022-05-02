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
#include <tuple>
#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/carnot.h"
#include "src/carnot/exec/local_grpc_result_server.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/funcs/funcs.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/testing.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {

using exec::CarnotTestUtils;

class JoinTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    table_store_ = std::make_shared<table_store::TableStore>();
    result_server_ = std::make_unique<exec::LocalGRPCResultSinkServer>();
    auto func_registry = std::make_unique<px::carnot::udf::Registry>("default_registry");
    px::carnot::funcs::RegisterFuncsOrDie(func_registry.get());
    auto clients_config = std::make_unique<Carnot::ClientsConfig>(Carnot::ClientsConfig{
        [this](const std::string& address, const std::string&) {
          return result_server_.get()->StubGenerator(address);
        },
        [](grpc::ClientContext*) {},
    });
    auto server_config = std::make_unique<Carnot::ServerConfig>();
    server_config->grpc_server_creds = grpc::InsecureServerCredentials();
    server_config->grpc_server_port = 0;

    carnot_ = px::carnot::Carnot::Create(sole::uuid4(), std::move(func_registry), table_store_,
                                         std::move(clients_config), std::move(server_config))
                  .ConsumeValueOrDie();
    auto left_table = CarnotTestUtils::TestTable();
    table_store_->AddTable("left_table", left_table);
    auto right_table = CarnotTestUtils::TestTable();
    table_store_->AddTable("right_table", right_table);
  }

  std::shared_ptr<table_store::TableStore> table_store_;
  std::unique_ptr<exec::LocalGRPCResultSinkServer> result_server_;
  std::unique_ptr<Carnot> carnot_;
};

TEST_F(JoinTest, basic) {
  std::string queryString =
      "import px\n"
      "src1 = px.DataFrame(table='left_table', select=['col1', 'col2'])\n"
      "src2 = px.DataFrame(table='right_table', select=['col1', 'col2'])\n"
      "join = src1.merge(src2, how='inner', left_on=['col1', 'col2'], right_on=['col1', 'col2'], "
      "suffixes=['', '_x'])\n"
      "join['left_col1'] = join['col1']\n"
      "join['right_col2'] = join['col2']\n"
      "df = join[['left_col1', 'right_col2']]\n"
      "px.display(df, 'joined')";

  auto query = absl::StrJoin({queryString}, "\n");
  auto query_id = sole::uuid4();
  // No time column, doesn't use a time parameter.
  auto s = carnot_->ExecuteQuery(query, query_id, 0);
  ASSERT_OK(s);

  auto exec_stats = result_server_->exec_stats().ConsumeValueOrDie();
  EXPECT_EQ(10, exec_stats.execution_stats().records_processed());
  EXPECT_EQ(10 * sizeof(double) + 10 * sizeof(int64_t),
            exec_stats.execution_stats().bytes_processed());
  EXPECT_LT(0, exec_stats.execution_stats().timing().execution_time_ns());

  EXPECT_THAT(result_server_->output_tables(), ::testing::UnorderedElementsAre("joined"));
  auto output_batches = result_server_->query_results("joined");
  EXPECT_EQ(2, output_batches.size());

  auto rb1 = output_batches[1];

  std::vector<types::Float64Value> expected_col1 = {0.5, 1.2, 5.3, 0.1, 5.1};
  std::vector<types::Int64Value> expected_col2 = {1, 2, 3, 5, 6};
  EXPECT_TRUE(rb1.ColumnAt(0)->Equals(types::ToArrow(expected_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1.ColumnAt(1)->Equals(types::ToArrow(expected_col2, arrow::default_memory_pool())));
}

TEST_F(JoinTest, self_join) {
  std::string queryString =
      "import px\n"
      "src1 = px.DataFrame(table='left_table', select=['col1', 'col2'])\n"
      "join = src1.merge(src1, how='inner', left_on=['col1'], right_on=['col1'], "
      "suffixes=['', '_x'])\n"
      "join['left_col1'] = join['col1']\n"
      "join['right_col2'] = join['col2_x']\n"
      "output = join[['left_col1', 'right_col2']]\n"
      "px.display(output, 'joined')";

  auto query = absl::StrJoin({queryString}, "\n");
  auto query_id = sole::uuid4();
  // No time column, doesn't use a time parameter.
  auto s = carnot_->ExecuteQuery(query, query_id, 0);
  ASSERT_OK(s);

  auto exec_stats = result_server_->exec_stats().ConsumeValueOrDie();
  EXPECT_EQ(5, exec_stats.execution_stats().records_processed());
  EXPECT_EQ(5 * sizeof(double) + 5 * sizeof(int64_t),
            exec_stats.execution_stats().bytes_processed());
  EXPECT_LT(0, exec_stats.execution_stats().timing().execution_time_ns());

  EXPECT_THAT(result_server_->output_tables(), ::testing::UnorderedElementsAre("joined"));
  auto output_batches = result_server_->query_results("joined");
  EXPECT_EQ(2, output_batches.size());

  auto rb1 = output_batches[1];
  std::vector<types::Float64Value> expected_col1 = {0.5, 1.2, 5.3, 0.1, 5.1};
  std::vector<types::Int64Value> expected_col2 = {1, 2, 3, 5, 6};
  EXPECT_TRUE(rb1.ColumnAt(0)->Equals(types::ToArrow(expected_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1.ColumnAt(1)->Equals(types::ToArrow(expected_col2, arrow::default_memory_pool())));
}

}  // namespace carnot
}  // namespace px
