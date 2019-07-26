#include <benchmark/benchmark.h>
#include <google/protobuf/text_format.h>

#include <algorithm>
#include <random>
#include <vector>

#include "src/carnot/carnot.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/common/benchmark/benchmark.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::Column;
using table_store::Table;
using table_store::schema::RowDescriptor;

const char* kGroupByNoneQuery =
    R"(
queryDF = From(table='test_table', select=['col0', 'col1']))"
    R"(.Agg(fn=lambda r: { 'sum': pl.sum(r.col1)}).Result(name='$0'))";

const char* kGroupByOneQuery =
    R"(
queryDF = From(table='test_table', select=['col0', 'col1']))"
    R"(.Agg(by=lambda r: [r.col0], fn=lambda r: { 'sum': pl.sum(r.col1)}).Result(name='$0'))";

const char* kGroupByTwoQuery =
    R"(
queryDF = From(table='test_table', select=['col0', 'col1', 'col2']))"
    R"(.Agg(by=lambda r: [r.col0, r.col1], fn=lambda r: { 'sum': pl.sum(r.col2)}).Result(name='$0'))";

std::shared_ptr<arrow::Array> GenerateInt64Batch(bmutils::DistributionType dist_type,
                                                 int64_t size) {
  if (dist_type == bmutils::DistributionType::kUniform) {
    auto data = bmutils::CreateLargeData<types::Int64Value>(size);
    return types::ToArrow(data, arrow::default_memory_pool());
  }
  auto data = bmutils::GetIntsFromExponential<types::Int64Value>(size, 1);
  return types::ToArrow(data, arrow::default_memory_pool());
}

std::shared_ptr<arrow::Array> GenerateStringBatch(int size,
                                                  const bmutils::DistributionParams* dist_vars,
                                                  const bmutils::DistributionParams* len_vars) {
  auto data = bmutils::CreateLargeStringData(size, dist_vars, len_vars);
  return types::ToArrow<types::StringValue>(data.ConsumeValueOrDie(), arrow::default_memory_pool());
}

StatusOr<std::shared_ptr<Table>> CreateTable(
    std::vector<types::DataType> types, std::vector<bmutils::DistributionType> distribution_types,
    int64_t rb_size, int64_t num_batches, const bmutils::DistributionParams* dist_vars,
    const bmutils::DistributionParams* len_vars) {
  RowDescriptor rd(types);
  std::vector<std::string> col_names;
  for (size_t col_idx = 0; col_idx < types.size(); col_idx++) {
    col_names.push_back(absl::StrFormat("col%d", col_idx));
  }

  auto table = std::make_shared<Table>(table_store::schema::Relation(types, col_names));

  for (size_t col_idx = 0; col_idx < types.size(); col_idx++) {
    auto col = table->GetColumn(col_idx);
    for (int batch_idx = 0; batch_idx < num_batches; batch_idx++) {
      std::shared_ptr<arrow::Array> batch;
      if (types.at(col_idx) == types::DataType::INT64) {
        batch = GenerateInt64Batch(distribution_types.at(col_idx), rb_size);
      } else if (types.at(col_idx) == types::DataType::STRING) {
        batch = GenerateStringBatch(rb_size, dist_vars, len_vars);
      } else {
        return error::InvalidArgument("We only support int and str types.");
      }
      auto s = col->AddBatch(batch);
      PL_UNUSED(s);
    }
  }

  return table;
}

std::unique_ptr<Carnot> SetUpCarnot(std::shared_ptr<table_store::TableStore> table_store) {
  std::shared_ptr<exec::RowBatchQueue> row_batch_queue;
  auto carnot_or_s = Carnot::Create(table_store, row_batch_queue);
  if (!carnot_or_s.ok()) {
    LOG(FATAL) << "Failed to initialize Carnot.";
  }
  return carnot_or_s.ConsumeValueOrDie();
}

// NOLINTNEXTLINE : runtime/references.
void BM_Query(benchmark::State& state, std::vector<types::DataType> types,
              std::vector<bmutils::DistributionType> distribution_types, const std::string& query,
              int64_t num_batches, const bmutils::DistributionParams* dist_vars,
              const bmutils::DistributionParams* len_vars) {
  auto table_store = std::make_shared<table_store::TableStore>();
  auto carnot = SetUpCarnot(table_store);
  auto table =
      CreateTable(types, distribution_types, state.range(0), num_batches, dist_vars, len_vars)
          .ConsumeValueOrDie();
  table_store->AddTable("test_table", table);

  int64_t bytes_processed = 0;
  int i = 0;
  for (auto _ : state) {
    auto queryWithTableName = absl::Substitute(query, "results_" + std::to_string(i));
    auto res = carnot->ExecuteQuery(queryWithTableName, CurrentTimeNS()).ConsumeValueOrDie();
    bytes_processed += res.bytes_processed;
    ++i;
  }

  state.SetBytesProcessed(int64_t(bytes_processed));
}

// NOLINTNEXTLINE : runtime/references.
void BM_Query_String(benchmark::State& state, std::vector<types::DataType> types,
                     std::vector<bmutils::DistributionType> distribution_types,
                     const std::string& query, int64_t num_batches,
                     const bmutils::DistributionParams* dist_vars,
                     const bmutils::DistributionParams* len_vars) {
  BM_Query(state, types, distribution_types, query, num_batches, dist_vars, len_vars);
}

// NOLINTNEXTLINE : runtime/references.
void BM_Query_Int(benchmark::State& state, std::vector<types::DataType> types,
                  std::vector<bmutils::DistributionType> distribution_types,
                  const std::string& query, int64_t num_batches) {
  const bmutils::DistributionParams* default_params = nullptr;
  BM_Query(state, types, distribution_types, query, num_batches, default_params, default_params);
}

const std::unique_ptr<const bmutils::DistributionParams> sample_selection_params =
    std::make_unique<const bmutils::ZipfianParams>(2, 2, 999);
const std::unique_ptr<const bmutils::DistributionParams> sample_length_params =
    std::make_unique<const bmutils::UniformParams>(0, 255);

BENCHMARK_CAPTURE(BM_Query_String, eval_group_by_one_uniform_string,
                  {types::DataType::STRING, types::DataType::INT64},
                  {bmutils::DistributionType::kZipfian, bmutils::DistributionType::kUniform},
                  kGroupByOneQuery, 20, sample_selection_params.get(), sample_length_params.get())
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

// Group By String Tests
BENCHMARK_CAPTURE(BM_Query_String, eval_group_by_none_string,
                  {types::DataType::STRING, types::DataType::INT64},
                  {bmutils::DistributionType::kZipfian, bmutils::DistributionType::kUniform},
                  kGroupByNoneQuery, 20, sample_selection_params.get(), sample_length_params.get())
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query_String, eval_group_by_one_uniform_string,
                  {types::DataType::STRING, types::DataType::INT64},
                  {bmutils::DistributionType::kZipfian, bmutils::DistributionType::kUniform},
                  kGroupByOneQuery, 20, sample_selection_params.get(), sample_length_params.get())
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query_String, eval_group_by_one_uniform_string,
                  {types::DataType::STRING, types::DataType::STRING, types::DataType::INT64},
                  {bmutils::DistributionType::kZipfian, bmutils::DistributionType::kUniform,
                   bmutils::DistributionType::kUniform},
                  kGroupByTwoQuery, 20, sample_selection_params.get(), sample_length_params.get())
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

// Group By Int Tests
BENCHMARK_CAPTURE(BM_Query_Int, eval_group_by_none,
                  {types::DataType::INT64, types::DataType::INT64},
                  {bmutils::DistributionType::kUniform, bmutils::DistributionType::kUniform},
                  kGroupByNoneQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query_Int, eval_group_by_one_uniform_int,
                  {types::DataType::INT64, types::DataType::INT64},
                  {bmutils::DistributionType::kUniform, bmutils::DistributionType::kUniform},
                  kGroupByOneQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query_Int, eval_group_by_two_uniform_ints,
                  {types::DataType::INT64, types::DataType::INT64, types::DataType::INT64},
                  {bmutils::DistributionType::kUniform, bmutils::DistributionType::kUniform,
                   bmutils::DistributionType::kUniform},
                  kGroupByTwoQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query_Int, eval_group_by_one_exponential_int,
                  {types::DataType::INT64, types::DataType::INT64},
                  {bmutils::DistributionType::kExponential, bmutils::DistributionType::kUniform},
                  kGroupByOneQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

}  // namespace exec
}  // namespace carnot
}  // namespace pl
