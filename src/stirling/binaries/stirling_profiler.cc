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

#include <csignal>
#include <iostream>
#include <thread>

#include "src/common/base/base.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/core/pub_sub_manager.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/perf_profiler/stack_traces_table.h"
#include "src/stirling/stirling.h"

using ::px::ProcessStatsMonitor;

using ::px::Status;
using ::px::StatusOr;

using ::px::stirling::IndexPublication;
using ::px::stirling::PerfProfileConnector;
using ::px::stirling::SourceRegistry;
using ::px::stirling::Stirling;
using ::px::stirling::stirlingpb::InfoClass;
using ::px::stirling::stirlingpb::Publish;

using ::px::md::UPID;
using ::px::types::ColumnWrapperRecordBatch;
using ::px::types::TabletID;

struct Args {
  uint32_t pid = 0;
};

// Put this in global space, so we can kill it in the signal handler.
Stirling* g_stirling = nullptr;
ProcessStatsMonitor* g_process_stats_monitor = nullptr;
absl::flat_hash_map<uint64_t, InfoClass> g_table_info_map;
std::atomic<bool> g_data_received = false;
Args g_args;

Status ParseArgs(int argc, char** argv) {
  if (argc != 2) {
    return ::px::error::Internal("Usage: ./stirling_profiler <pid>");
  }

  std::string_view pid_str(argv[1]);

  bool success = absl::SimpleAtoi(pid_str, &g_args.pid);
  if (!success) {
    return ::px::error::Internal("PID is not a valid number: $0", pid_str);
  }

  return Status::OK();
}

Status StirlingWrapperCallback(uint64_t table_id, TabletID /* tablet_id */,
                               std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  // Find the table info from the publications.
  auto iter = g_table_info_map.find(table_id);
  CHECK(iter != g_table_info_map.end());
  const InfoClass& table_info = iter->second;
  CHECK_EQ(table_info.schema().name(), "stack_traces.beta");

  auto& upid_col = (*record_batch)[px::stirling::kStackTraceUPIDIdx];
  auto& stack_trace_str_col = (*record_batch)[px::stirling::kStackTraceStackTraceStrIdx];
  auto& count_col = (*record_batch)[px::stirling::kStackTraceCountIdx];

  std::string out;
  for (size_t i = 0; i < stack_trace_str_col->Size(); ++i) {
    UPID upid(upid_col->Get<px::types::UInt128Value>(i).val);

    if (g_args.pid == upid.pid()) {
      std::cout << stack_trace_str_col->Get<px::types::StringValue>(i);
      std::cout << " ";
      std::cout << count_col->Get<px::types::Int64Value>(i).val;
      std::cout << "\n";
    }
  }

  g_data_received = true;

  return Status::OK();
}

void SignalHandler(int signum) {
  std::cerr << "\n\nStopping, might take a few seconds ..." << std::endl;
  // Important to call Stop(), because it releases BPF resources,
  // which would otherwise leak.
  if (g_stirling != nullptr) {
    g_stirling->Stop();
  }
  if (g_process_stats_monitor != nullptr) {
    g_process_stats_monitor->PrintCPUTime();
  }
  exit(signum);
}

int main(int argc, char** argv) {
  // Register signal handlers to clean-up on exit.
  signal(SIGINT, SignalHandler);
  signal(SIGQUIT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  signal(SIGHUP, SignalHandler);

  px::EnvironmentGuard env_guard(&argc, argv);

  PL_EXIT_IF_ERROR(ParseArgs(argc, argv));

  // Make Stirling.
  auto registry = std::make_unique<SourceRegistry>();
  registry->RegisterOrDie<PerfProfileConnector>();
  std::unique_ptr<Stirling> stirling = Stirling::Create(std::move(registry));
  g_stirling = stirling.get();
  stirling->RegisterDataPushCallback(StirlingWrapperCallback);

  // Enable use of USR1/USR2 for controlling debug.
  stirling->RegisterUserDebugSignalHandlers();

  // Get a publish proto message and subscribe to sources.
  Publish publication;
  stirling->GetPublishProto(&publication);
  IndexPublication(publication, &g_table_info_map);

  // Start measuring process stats after init.
  ProcessStatsMonitor process_stats_monitor;
  g_process_stats_monitor = &process_stats_monitor;

  // Run Stirling.
  std::thread run_thread = std::thread(&Stirling::Run, stirling.get());

  // Run for the specified amount of time, then terminate.
  for (int i = 0; i < 100; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (g_data_received) {
      break;
    }
  }
  stirling->Stop();

  // Wait for the thread to return.
  run_thread.join();

  return 0;
}
