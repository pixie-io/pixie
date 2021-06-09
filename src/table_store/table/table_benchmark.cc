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
#include "src/shared/types/types.h"
#include "src/table_store/table/table.h"

namespace px::table_store {

static inline std::unique_ptr<Table> MakeTable(int64_t max_size) {
  schema::Relation rel(
      std::vector<types::DataType>({types::DataType::TIME64NS, types::DataType::FLOAT64}),
      std::vector<std::string>({"time_", "float"}));
  return std::make_unique<Table>(rel, max_size);
}

static inline std::unique_ptr<types::ColumnWrapperRecordBatch> MakeHotBatch(int64_t batch_size) {
  std::vector<types::Time64NSValue> col1_vals(batch_size, 0);
  std::vector<types::Float64Value> col2_vals(batch_size, 1.234);

  auto wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper1 = std::make_shared<types::Time64NSValueColumnWrapper>(batch_size);
  col_wrapper1->Clear();
  col_wrapper1->AppendFromVector(col1_vals);
  wrapper_batch->push_back(col_wrapper1);

  auto col_wrapper2 = std::make_shared<types::Float64ValueColumnWrapper>(batch_size);
  col_wrapper2->Clear();
  col_wrapper2->AppendFromVector(col2_vals);
  wrapper_batch->push_back(col_wrapper2);
  return wrapper_batch;
}

static inline schema::RowBatch MakeColdBatch(int64_t batch_size) {
  std::vector<types::Time64NSValue> col1_vals(batch_size, 0);
  std::vector<types::Float64Value> col2_vals(batch_size, 1.234);

  auto rb = schema::RowBatch(
      schema::RowDescriptor({types::DataType::TIME64NS, types::DataType::FLOAT64}), batch_size);
  PL_CHECK_OK(rb.AddColumn(types::ToArrow(col1_vals, arrow::default_memory_pool())));
  PL_CHECK_OK(rb.AddColumn(types::ToArrow(col2_vals, arrow::default_memory_pool())));
  return rb;
}

static inline void FillTableHot(Table* table, int64_t table_size, int64_t batch_length) {
  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  for (int64_t i = 0; i < (table_size / batch_size); ++i) {
    auto batch = MakeHotBatch(batch_length);
    PL_CHECK_OK(table->TransferRecordBatch(std::move(batch)));
  }
}

static inline void FillTableCold(Table* table, int64_t table_size, int64_t batch_length) {
  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  for (int64_t i = 0; i < (table_size / batch_size); ++i) {
    auto batch = MakeColdBatch(batch_length);
    PL_CHECK_OK(table->WriteRowBatch(std::move(batch)));
  }
}

static inline void ReadFullTable(Table* table) {
  for (int64_t i = 0; i < table->NumBatches(); ++i) {
    benchmark::DoNotOptimize(table->GetRowBatch(i, {0, 1}, arrow::default_memory_pool()));
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableReadAllHot(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size);
  FillTableHot(table.get(), table_size, batch_length);

  CHECK_EQ(table->GetTableStats().bytes, table_size);

  for (auto _ : state) {
    ReadFullTable(table.get());
    state.PauseTiming();
    table = MakeTable(table_size);
    FillTableHot(table.get(), table_size, batch_length);
    state.ResumeTiming();
  }

  state.SetBytesProcessed(state.iterations() * table_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableReadAllCold(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size);
  FillTableCold(table.get(), table_size, batch_length);
  CHECK_EQ(table->GetTableStats().bytes, table_size);

  for (auto _ : state) {
    ReadFullTable(table.get());
  }

  state.SetBytesProcessed(state.iterations() * table_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableReadLastBatchAllHot(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size);
  FillTableHot(table.get(), table_size, batch_length);

  CHECK_EQ(table->GetTableStats().bytes, table_size);

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        table->GetRowBatch(table->NumBatches() - 1, {0, 1}, arrow::default_memory_pool()));
    state.PauseTiming();
    table = MakeTable(table_size);
    FillTableHot(table.get(), table_size, batch_length);
    state.ResumeTiming();
  }

  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  state.SetBytesProcessed(state.iterations() * batch_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableReadLastBatchAllCold(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size);
  FillTableCold(table.get(), table_size, batch_length);
  CHECK_EQ(table->GetTableStats().bytes, table_size);

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        table->GetRowBatch(table->NumBatches() - 1, {0, 1}, arrow::default_memory_pool()));
  }

  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  state.SetBytesProcessed(state.iterations() * batch_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableWriteEmpty(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size);

  for (auto _ : state) {
    state.PauseTiming();
    auto batch = MakeHotBatch(batch_length);
    state.ResumeTiming();
    PL_CHECK_OK(table->TransferRecordBatch(std::move(batch)));
    state.PauseTiming();
    // Reset table each time to ensure no expiration is required on write.
    table = MakeTable(table_size);
    state.ResumeTiming();
  }

  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  state.SetBytesProcessed(state.iterations() * batch_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableWriteFull(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size);
  // Fill table first to make sure each write requires an expiration.
  FillTableHot(table.get(), table_size, batch_length);

  for (auto _ : state) {
    state.PauseTiming();
    auto batch = MakeHotBatch(batch_length);
    state.ResumeTiming();
    PL_CHECK_OK(table->TransferRecordBatch(std::move(batch)));
  }

  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  state.SetBytesProcessed(state.iterations() * batch_size);
}

BENCHMARK(BM_TableReadAllHot);
BENCHMARK(BM_TableReadAllCold);
BENCHMARK(BM_TableReadLastBatchAllHot);
BENCHMARK(BM_TableReadLastBatchAllCold);
BENCHMARK(BM_TableWriteEmpty);
BENCHMARK(BM_TableWriteFull);

}  // namespace px::table_store
