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

#include <absl/base/internal/spinlock.h>
#include <google/protobuf/text_format.h>

#include "src/common/base/base.h"
#include "src/stirling/core/output.h"
#include "src/stirling/core/pub_sub_manager.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dynamic_tracer.h"
#include "src/stirling/stirling.h"

using ::px::ProcessStatsMonitor;

using ::px::Status;
using ::px::StatusOr;

using ::px::stirling::IndexPublication;
using ::px::stirling::SourceRegistry;
using ::px::stirling::Stirling;
using ::px::stirling::ToString;
using ::px::stirling::stirlingpb::InfoClass;
using ::px::stirling::stirlingpb::Publish;
using DynamicTracepointDeployment =
    ::px::stirling::dynamic_tracing::ir::logical::TracepointDeployment;

using ::px::types::ColumnWrapperRecordBatch;
using ::px::types::TabletID;

// Put this in global space, so we can kill it in the signal handler.
Stirling* g_stirling = nullptr;
ProcessStatsMonitor* g_process_stats_monitor = nullptr;
absl::flat_hash_map<uint64_t, InfoClass> g_table_info_map;
absl::base_internal::SpinLock g_callback_state_lock;

Status StirlingWrapperCallback(uint64_t table_id, TabletID /* tablet_id */,
                               std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  absl::base_internal::SpinLockHolder lock(&g_callback_state_lock);

  // Find the table info from the publications.
  auto iter = g_table_info_map.find(table_id);
  if (iter == g_table_info_map.end()) {
    return px::error::Internal("Encountered unknown table id $0", table_id);
  }
  const InfoClass& table_info = iter->second;

  std::cout << ToString(table_info.schema().name(), table_info.schema(), *record_batch);

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

enum class TracepointFormat { kUnknown, kIR, kPXL };

struct TraceProgram {
  std::string text;
  TracepointFormat format = TracepointFormat::kUnknown;
};

constexpr std::string_view kFunctionTraceTemplate = R"(
deployment_spec {
  path: "$0"
}
tracepoints {
  program {
    probes {
      name: "probe0"
      tracepoint {
        symbol: "$1"
        type: LOGICAL
      }
    }
  }
}
)";

StatusOr<TraceProgram> ParseArgs(int argc, char** argv) {
  if (argc != 3) {
    return ::px::error::Internal("Usage: ./stirling_dt <path to object file> <symbol>");
  }

  std::string_view path(argv[1]);
  std::string_view symbol(argv[2]);

  TraceProgram trace_program;
  trace_program.format = TracepointFormat::kIR;
  trace_program.text = absl::Substitute(kFunctionTraceTemplate, path, symbol);
  return trace_program;
}

StatusOr<Publish> DeployTrace(Stirling* stirling, TraceProgram trace_program_str) {
  auto trace_program = std::make_unique<DynamicTracepointDeployment>();
  bool success =
      google::protobuf::TextFormat::ParseFromString(trace_program_str.text, trace_program.get());
  if (!success) {
    return px::error::Internal("Unable to parse trace file");
  }

  sole::uuid trace_id = sole::uuid4();

  stirling->RegisterTracepoint(trace_id, std::move(trace_program));

  StatusOr<Publish> s;
  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    s = stirling->GetTracepointInfo(trace_id);
  } while (!s.ok() && s.code() == px::statuspb::Code::RESOURCE_UNAVAILABLE);

  return s;
}

int main(int argc, char** argv) {
  // Register signal handlers to clean-up on exit.
  signal(SIGINT, SignalHandler);
  signal(SIGQUIT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  signal(SIGHUP, SignalHandler);

  px::EnvironmentGuard env_guard(&argc, argv);

  PX_ASSIGN_OR_EXIT(TraceProgram trace_program, ParseArgs(argc, argv));

  // Make Stirling.
  std::unique_ptr<Stirling> stirling = Stirling::Create(std::make_unique<SourceRegistry>());
  g_stirling = stirling.get();

  // Enable use of USR1/USR2 for controlling debug.
  stirling->RegisterUserDebugSignalHandlers();

  // Get a publish proto message to subscribe from.
  Publish publication;
  stirling->GetPublishProto(&publication);
  IndexPublication(publication, &g_table_info_map);

  // Set a callback function that outputs the pushed records.
  stirling->RegisterDataPushCallback(StirlingWrapperCallback);

  // Start measuring process stats after init.
  ProcessStatsMonitor process_stats_monitor;
  g_process_stats_monitor = &process_stats_monitor;

  LOG(INFO) << "Trace spec:\n" << trace_program.text;

  StatusOr<Publish> trace_pub_status = DeployTrace(stirling.get(), trace_program);
  if (!trace_pub_status.ok()) {
    LOG(ERROR) << "Failed to deploy dynamic trace:\n"
               << trace_pub_status.status().ToProto().DebugString();
    return 1;
  }
  LOG(INFO) << "Successfully deployed dynamic trace!";

  // Get the publication, and add it to the index so it can be printed by
  // StirlingWrapperCallback.
  Publish& trace_pub = trace_pub_status.ValueOrDie();
  {
    absl::base_internal::SpinLockHolder lock(&g_callback_state_lock);
    IndexPublication(trace_pub, &g_table_info_map);
  }

  // Run Stirling.
  std::thread run_thread = std::thread(&Stirling::Run, stirling.get());

  // Wait for the thread to return.
  // This should never happen.
  run_thread.join();

  return 0;
}
