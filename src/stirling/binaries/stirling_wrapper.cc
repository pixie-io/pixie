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
#include <absl/strings/str_split.h>
#include <google/protobuf/text_format.h>

#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/common/perf/profiler.h"
#include "src/common/signal/signal.h"
#include "src/stirling/core/output.h"
#include "src/stirling/core/pub_sub_manager.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dynamic_tracer.h"
#include "src/stirling/stirling.h"

#ifdef PXL_SUPPORT
#include "src/carnot/planner/probes/tracepoint_generator.h"
#include "src/shared/tracepoint_translation/translation.h"
#endif

using ::px::ProcessStatsMonitor;

using ::px::Status;
using ::px::StatusOr;

using ::px::stirling::CreateSourceRegistryFromFlag;
using ::px::stirling::IndexPublication;
using ::px::stirling::SourceConnectorGroup;
using ::px::stirling::SourceRegistry;
using ::px::stirling::Stirling;
using ::px::stirling::ToString;
using ::px::stirling::stirlingpb::InfoClass;
using ::px::stirling::stirlingpb::Publish;
using ::px::types::ColumnWrapperRecordBatch;
using ::px::types::TabletID;

using DynamicTracepointDeployment =
    ::px::stirling::dynamic_tracing::ir::logical::TracepointDeployment;

DEFINE_string(trace, "",
              "Dynamic trace to deploy. Either (1) the path to a file containing PxL or IR trace "
              "spec, or (2) <path to object file>:<symbol_name> for full-function tracing.");
DEFINE_string(print_record_batches,
              "http_events,mysql_events,pgsql_events,redis_events,cql_events,dns_events",
              "Comma-separated list of tables to print.");
DEFINE_bool(init_only, false, "If true, only runs the init phase and exits. For testing.");
DEFINE_int32(timeout_secs, -1,
             "If non-negative, only runs for the specified amount of time and exits.");
DEFINE_bool(color_output, true, "If true, output logs will use colors.");
DEFINE_bool(enable_heap_profiler, false, "If true, heap profiling is enabled.");

// Put this in global space, so we can kill it in the signal handler.
Stirling* g_stirling = nullptr;
ProcessStatsMonitor* g_process_stats_monitor = nullptr;

//-----------------------------------------------------------------------------
// Callback/Printing Code
//-----------------------------------------------------------------------------

absl::flat_hash_set<std::string> g_table_print_enables;

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

  if (g_table_print_enables.contains(table_info.schema().name())) {
    // Only output enabled tables (lookup by name).
    std::cout << ToString(table_info.schema().name(), table_info.schema(), *record_batch);
  }

  return Status::OK();
}

//-----------------------------------------------------------------------------
// Signal Handling Code
//-----------------------------------------------------------------------------

// This signal handler is meant for graceful termination.
void TerminationHandler(int signum) {
  std::cerr << "\n\nStopping, might take a few seconds ..." << std::endl;
  // Important to call Stop(), because it releases BPF resources,
  // which would otherwise leak.
  if (g_stirling != nullptr) {
    g_stirling->Stop();
  }

  if (FLAGS_enable_heap_profiler && ::px::profiler::Heap::IsProfilerStarted()) {
    LOG(INFO) << "===== Stopping heap profiler.";
    ::px::profiler::Heap::StopProfiler();
  }

  if (g_process_stats_monitor != nullptr) {
    g_process_stats_monitor->PrintCPUTime();
  }
  exit(signum);
}

// DeathHandler is meant for fatal errors (like seg-faults),
// where no graceful termination is performed.
class DeathHandler : public px::FatalErrorHandlerInterface {
 public:
  DeathHandler() = default;
  void OnFatalError() const override {}
};

std::unique_ptr<px::SignalAction> g_signal_action;

//-----------------------------------------------------------------------------
// DynamicTracing Specific Code
//-----------------------------------------------------------------------------

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

// Get dynamic tracing program from either flags or environment variable.
std::optional<TraceProgram> GetTraceProgram() {
  std::string env_tracepoint(gflags::StringFromEnv("STIRLING_AUTO_TRACE", ""));

  if (!env_tracepoint.empty() && !FLAGS_trace.empty()) {
    LOG(WARNING)
        << "Trace specified through flags and environment variable. Flags take precedence.";
  }

  if (!FLAGS_trace.empty()) {
    TraceProgram trace_program;
    if (absl::StrContains(FLAGS_trace, ":")) {
      std::vector<std::string_view> split = absl::StrSplit(FLAGS_trace, ":");
      if (split.size() > 2) {
        LOG(ERROR) << "Inline tracing should be of the format <path to object file>:<symbol_name>";
        exit(1);
      }
      trace_program.format = TracepointFormat::kIR;
      trace_program.text = absl::Substitute(kFunctionTraceTemplate, split[0], split[1]);
    } else {
      trace_program.format =
          (absl::EndsWith(FLAGS_trace, ".pxl")) ? TracepointFormat::kPXL : TracepointFormat::kIR;
      PX_ASSIGN_OR_EXIT(trace_program.text, px::ReadFileToString(FLAGS_trace));
    }
    return trace_program;
  }

  if (!env_tracepoint.empty()) {
    TraceProgram trace_program;
    trace_program.format = TracepointFormat::kPXL;
    trace_program.text = std::move(env_tracepoint);
    return trace_program;
  }

  return std::nullopt;
}

StatusOr<Publish> DeployTrace(Stirling* stirling, TraceProgram trace_program_str) {
  std::unique_ptr<DynamicTracepointDeployment> trace_program;

  if (trace_program_str.format == TracepointFormat::kPXL) {
#ifdef PXL_SUPPORT
    PX_ASSIGN_OR_RETURN(auto compiled_tracepoint,
                        px::carnot::planner::compiler::CompileTracepoint(trace_program_str.text));
    LOG(INFO) << compiled_tracepoint.DebugString();
    trace_program = std::make_unique<DynamicTracepointDeployment>();
    ::px::tracepoint::ConvertPlannerTracepointToStirlingTracepoint(compiled_tracepoint,
                                                                   trace_program.get());
#else
    return px::error::Internal(
        "Cannot deploy tracepoint. stirling_wrapper was not built with PxL support.");
#endif
  } else {
    trace_program = std::make_unique<DynamicTracepointDeployment>();
    bool success =
        google::protobuf::TextFormat::ParseFromString(trace_program_str.text, trace_program.get());
    if (!success) {
      return px::error::Internal("Unable to parse trace file");
    }
  }

  // Automatically enable printing of this table.
  for (const auto& tracepoint : trace_program->tracepoints()) {
    g_table_print_enables.insert(tracepoint.table_name());
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

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(int argc, char** argv) {
  // Register signal handlers to gracefully clean-up on exit.
  signal(SIGINT, TerminationHandler);
  signal(SIGQUIT, TerminationHandler);
  signal(SIGTERM, TerminationHandler);
  signal(SIGHUP, TerminationHandler);

  // This handles fatal (non-graceful) errors.
  g_signal_action = std::make_unique<px::SignalAction>();
  DeathHandler err_handler;
  g_signal_action->RegisterFatalErrorHandler(err_handler);

  px::EnvironmentGuard env_guard(&argc, argv);
  // Override the default coloring set by the environment. This is useful for tests.
  FLAGS_colorlogtostderr = FLAGS_color_output;

  if (FLAGS_enable_heap_profiler) {
    CHECK(::px::profiler::Heap::ProfilerAvailable());
    LOG(INFO) << "===== Enabling heap profiler.";
    ::px::profiler::Heap::StartProfiler("stirling_heap");
  }

  LOG(INFO) << "Stirling Wrapper PID: " << getpid();

  if (!FLAGS_trace.empty()) {
    // In dynamic tracing mode, don't load any other sources.
    // Presumably, user only wants their dynamic trace.
    LOG(INFO) << "Dynamic Trace provided. All other data sources will be disabled.";
    FLAGS_stirling_sources = "";
  }

  if (!FLAGS_print_record_batches.empty()) {
    // Controls which tables are dumped to STDOUT in stirling_wrapper.
    g_table_print_enables = absl::StrSplit(FLAGS_print_record_batches, ",", absl::SkipWhitespace());
  }

  // Make Stirling.
  std::unique_ptr<Stirling> stirling = Stirling::Create(CreateSourceRegistryFromFlag());
  g_stirling = stirling.get();

  // Enable use of USR1/USR2 for controlling debug.
  stirling->RegisterUserDebugSignalHandlers();

  // Get a publish proto message to subscribe from.
  Publish publication;
  stirling->GetPublishProto(&publication);
  IndexPublication(publication, &g_table_info_map);

  // Set a callback function that outputs the pushed records.
  stirling->RegisterDataPushCallback(StirlingWrapperCallback);

  if (FLAGS_init_only) {
    LOG(INFO) << "Exiting after init.";
    return 0;
  }

  // Start measuring process stats after init.
  ProcessStatsMonitor process_stats_monitor;
  g_process_stats_monitor = &process_stats_monitor;

  // Run Data Collector.
  std::thread run_thread = std::thread(&Stirling::Run, stirling.get());

  // Wait until thread starts.
  PX_CHECK_OK(stirling->WaitUntilRunning(/* timeout */ std::chrono::seconds(5)));

  std::optional<TraceProgram> trace_program = GetTraceProgram();

  if (trace_program.has_value()) {
    LOG(INFO) << "Received trace spec:\n" << trace_program.value().text;

    // Dynamic traces are sources that are enabled while Stirling is running.
    // Deploy after a brief delay to emulate how it would be used in a PEM environment.
    // Could also consider removing this altogether.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    StatusOr<Publish> trace_pub_status = DeployTrace(stirling.get(), trace_program.value());
    if (!trace_pub_status.ok()) {
      LOG(ERROR) << "Failed to deploy dynamic trace:\n"
                 << trace_pub_status.status().ToProto().DebugString();
    } else {
      LOG(INFO) << "Successfully deployed dynamic trace!";

      // Get the publication, and add it to the index so it can be printed by
      // StirlingWrapperCallback.
      Publish& trace_pub = trace_pub_status.ValueOrDie();
      {
        absl::base_internal::SpinLockHolder lock(&g_callback_state_lock);
        IndexPublication(trace_pub, &g_table_info_map);
      }
    }
  }

  if (FLAGS_timeout_secs >= 0) {
    // Run for the specified amount of time, then terminate.
    LOG(INFO) << absl::Substitute("Running for $0 seconds.", FLAGS_timeout_secs);
    std::this_thread::sleep_for(std::chrono::seconds(FLAGS_timeout_secs));
    stirling->Stop();
  }

  // Wait for the thread to return.
  // This should never happen unless --timeout_secs is specified.
  run_thread.join();

  // Another model of how to run Stirling:
  // stirling->RunAsThread();
  // stirling->WaitForThreadJoin();

  return 0;
}
