#include <benchmark/benchmark.h>
#include <google/protobuf/text_format.h>

#include <algorithm>
#include <random>
#include <vector>

#include "src/carnot/carnot.h"
#include "src/carnot/exec/row_batch.h"
#include "src/carnot/exec/row_descriptor.h"
#include "src/carnot/exec/table.h"
#include "src/carnot/plan/relation.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/carnot/udf/udf_wrapper.h"
#include "src/common/common.h"
#include "src/common/time.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/utils/benchmark/utils.h"
PL_SUPPRESS_WARNINGS_START()
#include "src/vizier/proto/service.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace carnot {
namespace exec {

const char* kGroupByNoneQuery =
    R"(
queryDF = From(table='test_table', select=['col0', 'col1']))"
    R"(.Agg(by=None, fn=lambda r: { 'sum': pl.sum(r.col1)}).Result(name='$0'))";

const char* kGroupByOneQuery =
    R"(
queryDF = From(table='test_table', select=['col0', 'col1']))"
    R"(.Agg(by=lambda r: [r.col0], fn=lambda r: { 'sum': pl.sum(r.col1)}).Result(name='$0'))";

const char* kGroupByTwoQuery =
    R"(
queryDF = From(table='test_table', select=['col0', 'col1', 'col2']))"
    R"(.Agg(by=lambda r: [r.col0, r.col1], fn=lambda r: { 'sum': pl.sum(r.col2)}).Result(name='$0'))";

enum class DistributionType {
  uniform,
  exponential,
};

std::shared_ptr<arrow::Array> GenerateInt64Batch(DistributionType dist_type, int64_t size) {
  PL_UNUSED(dist_type);
  if (dist_type == DistributionType::uniform) {
    auto data = bmutils::CreateLargeData<types::Int64Value>(size);
    return types::ToArrow(data, arrow::default_memory_pool());
  }
  auto data = bmutils::GetIntsFromExponential<types::Int64Value>(size, 1);
  return types::ToArrow(data, arrow::default_memory_pool());
}

StatusOr<std::shared_ptr<Table>> CreateTable(std::vector<types::DataType> types,
                                             std::vector<DistributionType> distribution_types,
                                             int64_t rb_size, int64_t num_batches) {
  RowDescriptor rd = RowDescriptor(types);
  std::vector<std::string> col_names;
  for (size_t col_idx = 0; col_idx < types.size(); col_idx++) {
    col_names.push_back(absl::StrFormat("col%d", col_idx));
  }

  auto table = std::make_shared<Table>(plan::Relation(types, col_names));

  for (size_t col_idx = 0; col_idx < types.size(); col_idx++) {
    auto col = table->GetColumn(col_idx);
    for (int batch_idx = 0; batch_idx < num_batches; batch_idx++) {
      std::shared_ptr<arrow::Array> batch;
      // TODO(michelle): Handle string types.
      if (types.at(col_idx) == types::DataType::INT64) {
        batch = GenerateInt64Batch(distribution_types.at(col_idx), rb_size);
      } else {
        return error::InvalidArgument("We only support int types.");
      }
      auto s = col->AddBatch(batch);
      PL_UNUSED(s);
    }
  }

  return table;
}

std::shared_ptr<Carnot> SetUpCarnot() {
  auto carnot = std::make_shared<Carnot>();
  auto s = carnot->Init();
  PL_UNUSED(s);

  return carnot;
}

// NOLINTNEXTLINE : runtime/references.
void BM_Query(benchmark::State& state, std::vector<types::DataType> types,
              std::vector<DistributionType> distribution_types, const std::string& query,
              int64_t num_batches) {
  auto carnot = SetUpCarnot();
  auto table_store = carnot->table_store();
  auto table =
      CreateTable(types, distribution_types, state.range(0), num_batches).ConsumeValueOrDie();
  table_store->AddTable("test_table", table);

  int64_t bytes_processed = 0;
  int i = 0;
  for (auto _ : state) {
    auto queryWithTableName = absl::Substitute(query, "results_" + std::to_string(i));
    LOG(INFO) << queryWithTableName;
    auto res = carnot->ExecuteQuery(queryWithTableName, CurrentTimeNS()).ConsumeValueOrDie();
    bytes_processed += res.bytes_processed;
    ++i;
  }

  state.SetBytesProcessed(int64_t(bytes_processed));
}

BENCHMARK_CAPTURE(BM_Query, eval_group_by_none, {types::DataType::INT64, types::DataType::INT64},
                  {DistributionType::uniform, DistributionType::uniform}, kGroupByNoneQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query, eval_group_by_one_uniform_int,
                  {types::DataType::INT64, types::DataType::INT64},
                  {DistributionType::uniform, DistributionType::uniform}, kGroupByOneQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query, eval_group_by_two_uniform_ints,
                  {types::DataType::INT64, types::DataType::INT64, types::DataType::INT64},
                  {DistributionType::uniform, DistributionType::uniform, DistributionType::uniform},
                  kGroupByTwoQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_Query, eval_group_by_one_exponential_int,
                  {types::DataType::INT64, types::DataType::INT64},
                  {DistributionType::exponential, DistributionType::uniform}, kGroupByOneQuery, 20)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

}  // namespace exec
}  // namespace carnot
}  // namespace pl
