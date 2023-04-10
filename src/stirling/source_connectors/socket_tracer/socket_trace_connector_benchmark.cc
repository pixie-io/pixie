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

#include <gflags/gflags.h>

#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_split.h>
#include <benchmark/benchmark.h>
#include <magic_enum.hpp>

#include "src/common/perf/memory_tracker.h"
#include "src/common/perf/tcmalloc.h"
#include "src/stirling/core/connector_context.h"
#include "src/stirling/core/data_tables.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/benchmark_data_gen/data_gen.h"
#include "src/stirling/source_connectors/socket_tracer/testing/benchmark_data_gen/generators.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_connector_friend.h"
#include "src/stirling/testing/common.h"

DEFINE_string(display, "allocpeak,polliters",
              "Comma separated list of DisplayStatCategory's to specify what statistics to "
              "display. The list is case-insensitive.");

using ::benchmark::Counter;
using ::px::MemoryStats;
using ::px::MemoryTracker;
using ::px::stirling::SocketTraceConnector;
using ::px::stirling::SocketTraceConnectorFriend;
using ::px::stirling::SystemWideStandaloneContext;
using ::px::stirling::testing::BenchmarkDataGenerationSpec;
using ::px::stirling::testing::CQLQueryReqRespGen;
using ::px::stirling::testing::GapPosGenerator;
using ::px::stirling::testing::GenerateBenchmarkData;
using ::px::stirling::testing::HTTP1SingleReqRespGen;
using ::px::stirling::testing::IterationGapPosGenerator;
using ::px::stirling::testing::MySQLExecuteReqRespGen;
using ::px::stirling::testing::NATSMSGGen;
using ::px::stirling::testing::NoGapsPosGenerator;
using ::px::stirling::testing::PostgresSelectReqRespGen;

namespace {

enum class DisplayStatCategory {
  // Not using typical k... enum style to avoid user having to write the `k` on the command line.
  AllocPeak,
  PollIters,
  NumEvents,
  InOut,
  Physical,
  MemStartEnd,
  EquivThroughput,
  Throughput,
};

void CountOutput(px::stirling::DataTables* tables, uint64_t* output_records,
                 uint64_t* output_bytes) {
  for (auto tbl : tables->tables()) {
    auto tagged_records = tbl->ConsumeRecords();
    for (auto tagged_record : tagged_records) {
      if (tagged_record.records.size() > 0) {
        *output_records += tagged_record.records[0]->Size();
      }
      for (auto column_wrapper : tagged_record.records) {
        *output_bytes += column_wrapper->Bytes();
      }
    }
  }
}

px::StatusOr<absl::flat_hash_set<DisplayStatCategory>> GetDisplayStatCategories() {
  if (FLAGS_display == "*") {
    auto all_names = magic_enum::enum_values<DisplayStatCategory>();
    return absl::flat_hash_set<DisplayStatCategory>(all_names.begin(), all_names.end());
  }
  absl::flat_hash_set<DisplayStatCategory> categories;
  auto case_insensitive_cmp = [](char a, char b) { return tolower(a) == tolower(b); };
  for (std::string_view category_str : absl::StrSplit(FLAGS_display, ",")) {
    auto cast_opt = magic_enum::enum_cast<DisplayStatCategory, decltype(case_insensitive_cmp)>(
        category_str, case_insensitive_cmp);
    if (!cast_opt.has_value()) {
      return px::error::InvalidArgument("Invalid DisplayStatCategory: ", category_str);
    }
    categories.insert(cast_opt.value());
  }
  return categories;
}

}  // namespace

// Benchmark that simulates events coming from BPF and getting pushed to the SocketTraceConnector.
// Only benchmarks the path from receiving events from BPF to transferring those events to
// DataTables, doesn't benchmark the pushing to table store part of the pipeline.

// NOLINTNEXTLINE: runtime/references.
static void BM_SocketTraceConnector(benchmark::State& state, BenchmarkDataGenerationSpec spec) {
  auto display_stat_categories = GetDisplayStatCategories().ConsumeValueOrDie();

  auto generated_data = GenerateBenchmarkData(spec);
  MemoryStats mem_stats;
  uint64_t total_output_bytes = 0;
  uint64_t total_output_records = 0;

  SystemWideStandaloneContext ctx;
  // Only measure memory on the first iteration. Memory stats shouldn't change too much between
  // benchmark iterations, instead the iterations are helpful to measure variations in CPU.
  bool is_first_iter = true;
  for (auto _ : state) {
    state.PauseTiming();
    // We need a sub scope here so that we can call MemoryTracker::ReleaseFreeMemory after all the
    // destructors are called.
    {
      auto source_connector = SocketTraceConnectorFriend::Create("socket_trace_connector");
      auto socket_trace_connector =
          static_cast<SocketTraceConnectorFriend*>(source_connector.get());

      px::stirling::DataTables tables(SocketTraceConnector::kTables);
      source_connector->set_data_tables({tables.tables()});
      // Send control events to start all the connections. Control events are out of the scope of
      // this benchmark so we handle them before starting the timer.
      for (size_t i = 0; i < generated_data.control_events.size(); ++i) {
        socket_trace_connector->HandleControlEvent(&generated_data.control_events[i],
                                                   sizeof(socket_control_event_t));
      }
      // Run TransferData to make sure all the ConnTracker's have done at least an iteration before
      // we run the benchmark.
      source_connector->TransferData(&ctx);

      MemoryTracker mem_tracker(is_first_iter);
      if (is_first_iter) {
        mem_tracker.Start();
      }
      state.ResumeTiming();

      // START timed part of benchmark.

      for (auto& iter_events : generated_data.per_iter_data_events) {
        for (auto& event : iter_events) {
          socket_trace_connector->HandleDataEvent(
              &event, sizeof(socket_data_event_t::attr) + event.attr.msg_size);
        }
        source_connector->TransferData(&ctx);
      }

      // END timed part of benchmark.

      state.PauseTiming();
      if (is_first_iter) {
        mem_stats = mem_tracker.End();
      }
      // Count the size of the records that would be pushed as a result of this TransferData call.
      CountOutput(&tables, &total_output_records, &total_output_bytes);
    }
    px::ReleaseFreeMemory();
    is_first_iter = false;
    state.ResumeTiming();
  }
  // Stat reporting
#define MEM_COUNTER(x) Counter(x, Counter::kDefaults, Counter::OneK::kIs1024)

  if (display_stat_categories.contains(DisplayStatCategory::PollIters)) {
    state.counters["PollIters"] = Counter(spec.num_poll_iterations);
  }
  if (display_stat_categories.contains(DisplayStatCategory::AllocPeak)) {
    state.counters["AllocPeak"] = MEM_COUNTER(mem_stats.max.allocated - mem_stats.start.allocated);
  }

  if (display_stat_categories.contains(DisplayStatCategory::Throughput)) {
    state.SetBytesProcessed(generated_data.data_size_bytes * state.iterations());
  }

  if (display_stat_categories.contains(DisplayStatCategory::MemStartEnd)) {
    if (display_stat_categories.contains(DisplayStatCategory::Physical)) {
      state.counters["PhysMemStart"] = MEM_COUNTER(mem_stats.start.physical);
      state.counters["PhysMemEnd"] = MEM_COUNTER(mem_stats.end.physical);
    }
    state.counters["AllocMemStart"] = MEM_COUNTER(mem_stats.start.allocated);
    state.counters["AllocMemEnd"] = MEM_COUNTER(mem_stats.end.allocated);
  }

  if (display_stat_categories.contains(DisplayStatCategory::EquivThroughput)) {
    double simulated_time = static_cast<double>(spec.num_poll_iterations *
                                                SocketTraceConnector::kSamplingPeriod.count()) /
                            1000.0;
    state.counters["EquivalentWorkloadThroughput"] =
        MEM_COUNTER(generated_data.data_size_bytes / simulated_time);
  }

  if (display_stat_categories.contains(DisplayStatCategory::InOut)) {
    state.counters["BytesInput"] = MEM_COUNTER(generated_data.data_size_bytes);
    state.counters["RecordsInput"] =
        Counter(spec.num_poll_iterations * spec.num_conns * spec.records_per_conn);

    state.counters["BytesOutput"] = MEM_COUNTER(total_output_bytes / state.iterations());
    state.counters["RecordsOutput"] = Counter(total_output_records / state.iterations());
  }

  if (display_stat_categories.contains(DisplayStatCategory::NumEvents)) {
    size_t num_events = 0;
    for (const auto& iter : generated_data.per_iter_data_events) {
      num_events += iter.size();
    }
    state.counters["NumEvents"] = Counter(num_events);
  }
#undef MEM_COUNTER
}

constexpr uint64_t kRecordSize = 128 * 1024;
BENCHMARK_CAPTURE(BM_SocketTraceConnector, http1_no_gaps,
                  BenchmarkDataGenerationSpec{
                      .num_conns = 10,
                      .num_poll_iterations = 1,
                      .records_per_conn = 16,
                      .protocol = kProtocolHTTP,
                      .role = kRoleServer,
                      .rec_gen_func =
                          []() { return std::make_unique<HTTP1SingleReqRespGen>(kRecordSize); },
                      .pos_gen_func = []() { return std::make_unique<NoGapsPosGenerator>(); },
                  })
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(BM_SocketTraceConnector, http1_encode_chunked_no_gaps,
                  BenchmarkDataGenerationSpec{
                      .num_conns = 10,
                      .num_poll_iterations = 1,
                      .records_per_conn = 16,
                      .protocol = kProtocolHTTP,
                      .role = kRoleServer,
                      .rec_gen_func =
                          []() {
                            return std::make_unique<HTTP1SingleReqRespGen>(kRecordSize, 3 * 1024);
                          },
                      .pos_gen_func = []() { return std::make_unique<NoGapsPosGenerator>(); },
                  })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(BM_SocketTraceConnector, mysql_no_gaps,
                  BenchmarkDataGenerationSpec{
                      .num_conns = 10,
                      .num_poll_iterations = 1,
                      .records_per_conn = 16,
                      .protocol = kProtocolMySQL,
                      .role = kRoleServer,
                      .rec_gen_func =
                          []() { return std::make_unique<MySQLExecuteReqRespGen>(kRecordSize); },
                      .pos_gen_func = []() { return std::make_unique<NoGapsPosGenerator>(); },
                  })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(BM_SocketTraceConnector, postgres_no_gaps,
                  BenchmarkDataGenerationSpec{
                      .num_conns = 10,
                      .num_poll_iterations = 1,
                      .records_per_conn = 16,
                      .protocol = kProtocolPGSQL,
                      .role = kRoleServer,
                      .rec_gen_func =
                          []() { return std::make_unique<PostgresSelectReqRespGen>(kRecordSize); },
                      .pos_gen_func = []() { return std::make_unique<NoGapsPosGenerator>(); },
                  })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(BM_SocketTraceConnector, cql_no_gaps,
                  BenchmarkDataGenerationSpec{
                      .num_conns = 10,
                      .num_poll_iterations = 1,
                      .records_per_conn = 16,
                      .protocol = kProtocolCQL,
                      .role = kRoleServer,
                      .rec_gen_func =
                          []() { return std::make_unique<CQLQueryReqRespGen>(kRecordSize); },
                      .pos_gen_func = []() { return std::make_unique<NoGapsPosGenerator>(); },
                  })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(BM_SocketTraceConnector, nats_no_gaps,
                  BenchmarkDataGenerationSpec{
                      .num_conns = 10,
                      .num_poll_iterations = 1,
                      .records_per_conn = 16,
                      .protocol = kProtocolNATS,
                      .role = kRoleServer,
                      .rec_gen_func = []() { return std::make_unique<NATSMSGGen>(kRecordSize); },
                      .pos_gen_func = []() { return std::make_unique<NoGapsPosGenerator>(); },
                  })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(
    BM_SocketTraceConnector, http1_inter_iter_gaps,
    BenchmarkDataGenerationSpec{
        .num_conns = 10,
        .num_poll_iterations = 5,
        .records_per_conn = 16,
        .protocol = kProtocolHTTP,
        .role = kRoleServer,
        .rec_gen_func =
            []() { return std::make_unique<HTTP1SingleReqRespGen>(/*body size*/ kRecordSize); },
        .pos_gen_func =
            []() {
              return std::make_unique<IterationGapPosGenerator>(/*gap size*/ 500 * 1024 * 1024);
            },
    })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(
    BM_SocketTraceConnector, mysql_inter_iter_gaps,
    BenchmarkDataGenerationSpec{
        .num_conns = 10,
        .num_poll_iterations = 5,
        .records_per_conn = 16,
        .protocol = kProtocolMySQL,
        .role = kRoleServer,
        .rec_gen_func = []() { return std::make_unique<MySQLExecuteReqRespGen>(kRecordSize); },
        .pos_gen_func =
            []() {
              return std::make_unique<IterationGapPosGenerator>(/*gap_size*/ 500 * 1024 * 1024);
            },
    })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(
    BM_SocketTraceConnector, postgres_inter_iter_gaps,
    BenchmarkDataGenerationSpec{
        .num_conns = 10,
        .num_poll_iterations = 5,
        .records_per_conn = 16,
        .protocol = kProtocolPGSQL,
        .role = kRoleServer,
        .rec_gen_func = []() { return std::make_unique<PostgresSelectReqRespGen>(kRecordSize); },
        .pos_gen_func =
            []() {
              return std::make_unique<IterationGapPosGenerator>(/*gap_size*/ 500 * 1024 * 1024);
            },
    })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(BM_SocketTraceConnector, nats_inter_iter_gaps,
                  BenchmarkDataGenerationSpec{
                      .num_conns = 10,
                      .num_poll_iterations = 5,
                      .records_per_conn = 16,
                      .protocol = kProtocolNATS,
                      .role = kRoleServer,
                      .rec_gen_func = []() { return std::make_unique<NATSMSGGen>(kRecordSize); },
                      .pos_gen_func =
                          []() {
                            return std::make_unique<IterationGapPosGenerator>(/*gap_size*/ 500 *
                                                                              1024 * 1024);
                          },
                  })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(
    BM_SocketTraceConnector, cql_inter_iter_gaps,
    BenchmarkDataGenerationSpec{
        .num_conns = 10,
        .num_poll_iterations = 5,
        .records_per_conn = 16,
        .protocol = kProtocolCQL,
        .role = kRoleServer,
        .rec_gen_func = []() { return std::make_unique<CQLQueryReqRespGen>(kRecordSize); },
        .pos_gen_func =
            []() {
              return std::make_unique<IterationGapPosGenerator>(/*gap_size*/ 500 * 1024 * 1024);
            },
    })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(BM_SocketTraceConnector, http1_intra_iter_gaps,
                  BenchmarkDataGenerationSpec{
                      .num_conns = 10,
                      .num_poll_iterations = 1,
                      .records_per_conn = 48,
                      .protocol = kProtocolHTTP,
                      .role = kRoleServer,
                      .rec_gen_func =
                          []() { return std::make_unique<HTTP1SingleReqRespGen>(kRecordSize); },
                      .pos_gen_func =
                          []() {
                            return std::make_unique<GapPosGenerator>(
                                /*continuous chunk size*/ 10 * kRecordSize,
                                /*gap size*/ 5 * 1024 * 1024);
                          },
                  })
    ->Unit(benchmark::kMillisecond);
