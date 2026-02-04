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

#include <absl/synchronization/barrier.h>
#include <absl/synchronization/notification.h>
#include <benchmark/benchmark.h>
#include <chrono>
#include <deque>
#include <numeric>
#include <random>
#include <thread>

#include "src/shared/types/types.h"
#include "src/table_store/table/table.h"

namespace px::table_store {

static inline std::unique_ptr<Table> MakeTable(int64_t max_size, int64_t compaction_size) {
  schema::Relation rel(
      std::vector<types::DataType>({types::DataType::TIME64NS, types::DataType::FLOAT64}),
      std::vector<std::string>({"time_", "float"}));
  return std::make_unique<Table>("test_table", rel, max_size, compaction_size);
}

static inline std::unique_ptr<types::ColumnWrapperRecordBatch> MakeHotBatch(int64_t batch_size,
                                                                            int64_t* time_counter) {
  std::vector<types::Time64NSValue> col1_vals(batch_size, 0);
  std::vector<types::Float64Value> col2_vals(batch_size, 1.234);

  for (size_t i = 0; i < col1_vals.size(); ++i) {
    col1_vals[i] = *time_counter;
    (*time_counter)++;
  }

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

static inline int64_t FillTableHot(Table* table, int64_t table_size, int64_t batch_length) {
  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  int64_t time_counter = 0;
  for (int64_t i = 0; i < (table_size / batch_size); ++i) {
    auto batch = MakeHotBatch(batch_length, &time_counter);
    PX_CHECK_OK(table->TransferRecordBatch(std::move(batch)));
  }
  return time_counter;
}

static inline int64_t FillTableCold(Table* table, int64_t table_size, int64_t batch_length) {
  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  int64_t time_counter = 0;
  for (int64_t i = 0; i < (table_size / batch_size); ++i) {
    auto batch = MakeHotBatch(batch_length, &time_counter);
    PX_CHECK_OK(table->TransferRecordBatch(std::move(batch)));
    // Run compaction every time to ensure that all batches get put into cold.
    PX_CHECK_OK(table->CompactHotToCold(arrow::default_memory_pool()));
  }
  return time_counter;
}

static inline void ReadFullTable(Table::Cursor* cursor) {
  while (!cursor->Done()) {
    benchmark::DoNotOptimize(cursor->GetNextRowBatch({0, 1}));
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableReadAllHot(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t compaction_size = 64 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size, compaction_size);
  FillTableHot(table.get(), table_size, batch_length);

  CHECK_EQ(table->GetTableStats().bytes, table_size);

  Table::Cursor cursor(table.get());

  for (auto _ : state) {
    ReadFullTable(&cursor);
    state.PauseTiming();
    table = MakeTable(table_size, compaction_size);
    FillTableHot(table.get(), table_size, batch_length);
    cursor = Table::Cursor(table.get());
    state.ResumeTiming();
  }

  state.SetBytesProcessed(state.iterations() * table_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableReadAllCold(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t compaction_size = 64 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size, compaction_size);
  FillTableCold(table.get(), table_size, batch_length);
  CHECK_EQ(table->GetTableStats().bytes, table_size);
  Table::Cursor cursor(table.get());

  for (auto _ : state) {
    ReadFullTable(&cursor);

    state.PauseTiming();
    cursor = Table::Cursor(table.get());
    state.ResumeTiming();
  }

  state.SetBytesProcessed(state.iterations() * table_size);
}

Table::Cursor GetLastBatchCursor(Table* table, int64_t last_time, int64_t batch_length,
                                 const std::vector<int64_t>& cols) {
  Table::Cursor cursor(table,
                       Table::Cursor::StartSpec{Table::Cursor::StartSpec::StartType::StartAtTime,
                                                last_time - 2 * batch_length},
                       Table::Cursor::StopSpec{});
  // Advance the cursor so that it points to the last batch and has BatchHints set.
  cursor.GetNextRowBatch(cols);
  return cursor;
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableReadLastBatchAllHot(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t compaction_size = 64 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size, compaction_size);
  auto last_time = FillTableHot(table.get(), table_size, batch_length);

  CHECK_EQ(table->GetTableStats().bytes, table_size);

  auto last_batch_cursor = GetLastBatchCursor(table.get(), last_time, batch_length, {0, 1});

  for (auto _ : state) {
    benchmark::DoNotOptimize(last_batch_cursor.GetNextRowBatch({0, 1}));
    state.PauseTiming();
    table = MakeTable(table_size, compaction_size);
    last_time = FillTableHot(table.get(), table_size, batch_length);
    last_batch_cursor = GetLastBatchCursor(table.get(), last_time, batch_length, {0, 1});
    state.ResumeTiming();
  }

  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  state.SetBytesProcessed(state.iterations() * batch_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableReadLastBatchAllCold(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t compaction_size = 64 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size, compaction_size);
  auto last_time = FillTableCold(table.get(), table_size, batch_length);
  CHECK_EQ(table->GetTableStats().bytes, table_size);

  auto last_batch_cursor = GetLastBatchCursor(table.get(), last_time, batch_length, {0, 1});

  for (auto _ : state) {
    benchmark::DoNotOptimize(last_batch_cursor.GetNextRowBatch({0, 1}));
    state.PauseTiming();
    last_batch_cursor = GetLastBatchCursor(table.get(), last_time, batch_length, {0, 1});
    state.ResumeTiming();
  }

  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  state.SetBytesProcessed(state.iterations() * batch_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableWriteEmpty(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t compaction_size = 64 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size, compaction_size);

  for (auto _ : state) {
    state.PauseTiming();
    int64_t time_counter = 0;
    auto batch = MakeHotBatch(batch_length, &time_counter);
    state.ResumeTiming();
    PX_CHECK_OK(table->TransferRecordBatch(std::move(batch)));
    state.PauseTiming();
    // Reset table each time to ensure no expiration is required on write.
    table = MakeTable(table_size, compaction_size);
    state.ResumeTiming();
  }

  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  state.SetBytesProcessed(state.iterations() * batch_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableWriteFull(benchmark::State& state) {
  int64_t table_size = 4 * 1024 * 1024;
  int64_t compaction_size = 64 * 1024;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size, compaction_size);
  // Fill table first to make sure each write requires an expiration.
  FillTableHot(table.get(), table_size, batch_length);

  for (auto _ : state) {
    state.PauseTiming();
    int64_t time_counter = 0;
    auto batch = MakeHotBatch(batch_length, &time_counter);
    state.ResumeTiming();
    PX_CHECK_OK(table->TransferRecordBatch(std::move(batch)));
  }

  int64_t batch_size = batch_length * sizeof(int64_t) + batch_length * sizeof(double);
  state.SetBytesProcessed(state.iterations() * batch_size);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableCompaction(benchmark::State& state) {
  int64_t compaction_size = 64 * 1024;
  int64_t table_size = Table::kMaxBatchesPerCompactionCall * compaction_size;
  int64_t batch_length = 256;
  auto table = MakeTable(table_size, compaction_size);
  // Fill table first to make sure each compaction hits kMaxBatchesPerCompaction.
  // This should be the slowest possible compaction. Since none of the batches will have an
  // arrow_cache.
  FillTableHot(table.get(), table_size, batch_length);

  for (auto _ : state) {
    PX_CHECK_OK(table->CompactHotToCold(arrow::default_memory_pool()));
    state.PauseTiming();
    FillTableHot(table.get(), table_size, batch_length);
    state.ResumeTiming();
  }

  state.SetBytesProcessed(state.iterations() * compaction_size *
                          Table::kMaxBatchesPerCompactionCall);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TableThreaded(benchmark::State& state) {
  schema::Relation rel({types::DataType::TIME64NS}, {"time_"});
  schema::RowDescriptor rd({types::DataType::TIME64NS});
  std::shared_ptr<Table> table_ptr =
      std::make_shared<Table>("test_table", rel, 16 * 1024 * 1024, 5 * 1024);

  int64_t batch_size = 1024;
  int64_t num_batches = 16 * 1024;

  auto done = std::make_shared<absl::Notification>();
  auto result_lock = std::make_shared<absl::base_internal::SpinLock>();
  auto read_results = std::make_shared<std::deque<double>>();
  auto write_results = std::make_shared<std::deque<double>>();
  int num_read_threads = 6;
  int num_write_threads = 2;
  auto barrier = std::make_shared<absl::Barrier>(num_read_threads + num_write_threads);
  auto reader_barrier = std::make_shared<absl::Barrier>(num_read_threads);

  std::thread compaction_thread([table_ptr, done]() {
    while (!done->WaitForNotificationWithTimeout(absl::Milliseconds(50))) {
      PX_CHECK_OK(table_ptr->CompactHotToCold(arrow::default_memory_pool()));
    }
    // Do one last compaction after writer thread has finished writing.
    PX_CHECK_OK(table_ptr->CompactHotToCold(arrow::default_memory_pool()));
  });

  auto writer_work = [&]() {
    barrier->Block();
    while (!done->WaitForNotificationWithTimeout(absl::Milliseconds(1))) {
      std::vector<types::Time64NSValue> time_col(batch_size, 1234);
      auto wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
      auto col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(batch_size);
      col_wrapper->Clear();
      col_wrapper->AppendFromVector(time_col);
      wrapper_batch->push_back(col_wrapper);
      auto start = std::chrono::high_resolution_clock::now();
      PX_CHECK_OK(table_ptr->TransferRecordBatch(std::move(wrapper_batch)));
      auto end = std::chrono::high_resolution_clock::now();
      auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
      {
        absl::base_internal::SpinLockHolder lock(result_lock.get());
        write_results->push_back(elapsed_seconds.count());
      }
    }
  };

  auto reader_work = [&](int thread_index) {
    barrier->Block();

    int64_t batch_counter = 0;
    while (batch_counter < (num_batches / num_read_threads)) {
      Table::Cursor cursor(table_ptr.get());
      auto start = std::chrono::high_resolution_clock::now();
      auto batch_or_s = cursor.GetNextRowBatch({0});
      auto end = std::chrono::high_resolution_clock::now();
      if (!batch_or_s.ok()) {
        continue;
      }

      auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
      {
        absl::base_internal::SpinLockHolder lock(result_lock.get());
        read_results->push_back(elapsed_seconds.count());
      }
      batch_counter++;
    }
    reader_barrier->Block();
    if (thread_index == 0) {
      done->Notify();
    }
  };

  std::vector<std::thread> reader_threads;
  for (int i = 0; i < num_read_threads; ++i) {
    reader_threads.emplace_back(reader_work, i);
  }

  std::vector<std::thread> writer_threads;
  for (int i = 0; i < num_write_threads; ++i) {
    writer_threads.emplace_back(writer_work);
  }

  for (auto _ : state) {
  }

  compaction_thread.join();
  for (int i = 0; i < num_write_threads; ++i) {
    writer_threads[i].join();
  }
  for (int i = 0; i < num_read_threads; ++i) {
    reader_threads[i].join();
  }

  auto read_average_time =
      std::accumulate(read_results->begin(), read_results->end(), 0.0) / read_results->size();
  auto write_average_time =
      std::accumulate(write_results->begin(), write_results->end(), 0.0) / write_results->size();

  state.counters["Read"] = benchmark::Counter(read_average_time);
  state.counters["Write"] = benchmark::Counter(write_average_time);
}

BENCHMARK(BM_TableReadAllHot);
BENCHMARK(BM_TableReadAllCold);
BENCHMARK(BM_TableReadLastBatchAllHot)->Iterations(1000);
BENCHMARK(BM_TableReadLastBatchAllCold)->Iterations(1000);
BENCHMARK(BM_TableWriteEmpty);
BENCHMARK(BM_TableWriteFull);
BENCHMARK(BM_TableCompaction);
BENCHMARK(BM_TableThreaded)->UseManualTime()->Iterations(1);

}  // namespace px::table_store
