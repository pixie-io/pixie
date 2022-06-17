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

#include <benchmark/benchmark.h>
#include <google/protobuf/text_format.h>

#include <algorithm>
#include <random>
#include <vector>

#include <sole.hpp>

#include "src/carnot/carnot.h"
#include "src/carnot/exec/local_grpc_result_server.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/funcs/funcs.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/common/benchmark/benchmark.h"
#include "src/datagen/datagen.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/table_store/test_utils.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::Table;
using table_store::schema::RowDescriptor;

constexpr char kGroupByNoneQuery[] = R"pxl(
import px
df = px.DataFrame(table='test_table', select=['col0', 'col1'])
df = df.agg(sum=('col1', px.sum))
px.display(df, '$0')
)pxl";

constexpr char kGroupByOneQuery[] = R"pxl(
import px
df = px.DataFrame(table='test_table', select=['col0', 'col1'])
df = df.groupby('col0').agg(sum=('col1', px.sum))
px.display(df, '$0')
)pxl";

constexpr char kGroupByTwoQuery[] = R"pxl(
import px
df = px.DataFrame(table='test_table', select=['col0', 'col1', 'col2'])
df = df.groupby(['col0', 'col1']).agg(sum=('col2', px.sum))
px.display(df, '$0')
)pxl";

std::unique_ptr<Carnot> SetUpCarnot(std::shared_ptr<table_store::TableStore> table_store,
                                    LocalGRPCResultSinkServer* server) {
  auto func_registry = std::make_unique<px::carnot::udf::Registry>("default_registry");
  funcs::RegisterFuncsOrDie(func_registry.get());
  auto clients_config = std::make_unique<Carnot::ClientsConfig>(Carnot::ClientsConfig{
      [server](const std::string& address, const std::string&) {
        return server->StubGenerator(address);
      },
      [](grpc::ClientContext*) {},
  });
  auto server_config = std::make_unique<Carnot::ServerConfig>();
  server_config->grpc_server_port = 0;

  return px::carnot::Carnot::Create(sole::uuid4(), std::move(func_registry), table_store,
                                    std::move(clients_config), std::move(server_config))
      .ConsumeValueOrDie();
}

// NOLINTNEXTLINE : runtime/references.
void BM_Query(benchmark::State& state, std::vector<types::DataType> types,
              std::vector<datagen::DistributionType> distribution_types, const std::string& query,
              int64_t num_batches, const datagen::DistributionParams* dist_vars,
              const datagen::DistributionParams* len_vars) {
  auto table_store = std::make_shared<table_store::TableStore>();
  auto server = LocalGRPCResultSinkServer();

  auto carnot = SetUpCarnot(table_store, &server);
  auto table = table_store::CreateTable(types, distribution_types, state.range(0), num_batches,
                                        dist_vars, len_vars)
                   .ConsumeValueOrDie();
  table_store->AddTable("test_table", table);

  int64_t bytes_processed = 0;
  int i = 0;
  for (auto _ : state) {
    auto queryWithTableName = absl::Substitute(query, "results_" + std::to_string(i));
    auto res = carnot->ExecuteQuery(queryWithTableName, sole::uuid4(), CurrentTimeNS());
    if (!res.ok()) {
      LOG(FATAL) << "Aggregate benchmark query did not execute successfully.";
    }
    bytes_processed += server.exec_stats().ConsumeValueOrDie().execution_stats().bytes_processed();
    server.ResetQueryResults();
    ++i;
  }

  state.SetBytesProcessed(int64_t(bytes_processed));
}

// NOLINTNEXTLINE : runtime/references.
void BM_Query_String(benchmark::State& state, std::vector<types::DataType> types,
                     std::vector<datagen::DistributionType> distribution_types,
                     const std::string& query, int64_t num_batches,
                     const datagen::DistributionParams* dist_vars,
                     const datagen::DistributionParams* len_vars) {
  BM_Query(state, types, distribution_types, query, num_batches, dist_vars, len_vars);
}

// NOLINTNEXTLINE : runtime/references.
void BM_Query_Int(benchmark::State& state, std::vector<types::DataType> types,
                  std::vector<datagen::DistributionType> distribution_types,
                  const std::string& query, int64_t num_batches) {
  const datagen::DistributionParams* default_params = nullptr;
  BM_Query(state, types, distribution_types, query, num_batches, default_params, default_params);
}

const std::unique_ptr<const datagen::DistributionParams> sample_selection_params =
    std::make_unique<const datagen::ZipfianParams>(2, 2, 999);
const std::unique_ptr<const datagen::DistributionParams> sample_length_params =
    std::make_unique<const datagen::UniformParams>(0, 255);

// TODO(philkuz) delete if this is a duplicate of L117.
BENCHMARK_CAPTURE(BM_Query_String, eval_group_by_one_uniform_string,
                  {types::DataType::STRING, types::DataType::INT64},
                  {datagen::DistributionType::kZipfian, datagen::DistributionType::kUniform},
                  kGroupByOneQuery, 20, sample_selection_params.get(), sample_length_params.get())
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

// Group By String Tests
BENCHMARK_CAPTURE(BM_Query_String, eval_group_by_none_string,
                  {types::DataType::STRING, types::DataType::INT64},
                  {datagen::DistributionType::kZipfian, datagen::DistributionType::kUniform},
                  kGroupByNoneQuery, 20, sample_selection_params.get(), sample_length_params.get())
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query_String, eval_group_by_one_uniform_string,
                  {types::DataType::STRING, types::DataType::INT64},
                  {datagen::DistributionType::kZipfian, datagen::DistributionType::kUniform},
                  kGroupByOneQuery, 20, sample_selection_params.get(), sample_length_params.get())
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

// TODO(philkuz) fails in table.cc because batch size > table size.
// BENCHMARK_CAPTURE(BM_Query_String, eval_group_by_one_uniform_string_two_data_cols,
//                   {types::DataType::STRING, types::DataType::STRING, types::DataType::INT64},
//                   {datagen::DistributionType::kZipfian, datagen::DistributionType::kUniform,
//                    datagen::DistributionType::kUniform},
//                   kGroupByTwoQuery, 20, sample_selection_params.get(),
//                   sample_length_params.get())
//     ->RangeMultiplier(2)
//     ->Range(1, 1 << 16);

// Group By Int Tests
BENCHMARK_CAPTURE(BM_Query_Int, eval_group_by_none,
                  {types::DataType::INT64, types::DataType::INT64},
                  {datagen::DistributionType::kUniform, datagen::DistributionType::kUniform},
                  kGroupByNoneQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query_Int, eval_group_by_one_uniform_int,
                  {types::DataType::INT64, types::DataType::INT64},
                  {datagen::DistributionType::kUniform, datagen::DistributionType::kUniform},
                  kGroupByOneQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query_Int, eval_group_by_two_uniform_ints,
                  {types::DataType::INT64, types::DataType::INT64, types::DataType::INT64},
                  {datagen::DistributionType::kUniform, datagen::DistributionType::kUniform,
                   datagen::DistributionType::kUniform},
                  kGroupByTwoQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query_Int, eval_group_by_one_exponential_int,
                  {types::DataType::INT64, types::DataType::INT64},
                  {datagen::DistributionType::kExponential, datagen::DistributionType::kUniform},
                  kGroupByOneQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

}  // namespace exec
}  // namespace carnot
}  // namespace px
