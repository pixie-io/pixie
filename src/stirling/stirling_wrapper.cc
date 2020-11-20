#include <sys/syscall.h>
#include <unistd.h>
#define gettid() syscall(SYS_gettid)

#include <ctime>
#include <iomanip>
#include <thread>

#include <absl/base/internal/spinlock.h>
#include <absl/strings/str_split.h>
#include <google/protobuf/text_format.h>

#include "src/common/base/base.h"
#include "src/stirling/dynamic_tracing/dynamic_tracer.h"
#include "src/stirling/output.h"
#include "src/stirling/pub_sub_manager.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"

#include "src/stirling/cass_table.h"
#include "src/stirling/conn_stats_table.h"
#include "src/stirling/dns_table.h"
#include "src/stirling/http_table.h"
#include "src/stirling/mysql_table.h"
#include "src/stirling/pgsql_table.h"

#include "src/carnot/planner/probes/tracepoint_generator.h"

using pl::Status;
using pl::StatusOr;

using pl::stirling::IndexPublication;
using pl::stirling::PrintRecordBatch;
using pl::stirling::SourceRegistry;
using pl::stirling::SourceRegistrySpecifier;
using pl::stirling::Stirling;
using pl::stirling::stirlingpb::Publish;
using pl::stirling::stirlingpb::Subscribe;
using DynamicTracepointDeployment =
    pl::stirling::dynamic_tracing::ir::logical::TracepointDeployment;

using pl::types::ColumnWrapperRecordBatch;
using pl::types::TabletID;

using pl::stirling::kConnStatsTable;
using pl::stirling::kCQLTable;
using pl::stirling::kDNSTable;
using pl::stirling::kHTTPTable;
using pl::stirling::kMySQLTable;
using pl::stirling::kPGSQLTable;

DEFINE_string(sources, "kProd", "[kAll|kProd|kMetrics|kTracers] Choose sources to enable.");
DEFINE_string(trace, "",
              "Dynamic trace to deploy. Either (1) the path to a file containing PxL or IR trace "
              "spec, or (2) <path to object file>:<symbol_name> for full-function tracing.");
DEFINE_string(print_record_batches,
              absl::Substitute("$0,$1,$2,$3", kHTTPTable.name(), kMySQLTable.name(),
                               kCQLTable.name(), kDNSTable.name()),
              "Comma-separated list of tables to print. Defaults to tracers if not specified. Use "
              "'None' for none.");
DEFINE_bool(init_only, false, "If true, only runs the init phase and exits. For testing.");
DEFINE_int32(timeout_secs, 0,
             "If greater than 0, only runs for the specified amount of time and exits.");

std::vector<std::string> g_table_print_enables;

// Put this in global space, so we can kill it in the signal handler.
Stirling* g_stirling = nullptr;
pl::ProcessStatsMonitor* g_process_stats_monitor = nullptr;
absl::flat_hash_map<uint64_t, ::pl::stirling::stirlingpb::InfoClass> g_table_info_map;
absl::base_internal::SpinLock g_callback_state_lock;

Status StirlingWrapperCallback(uint64_t table_id, TabletID /* tablet_id */,
                               std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  absl::base_internal::SpinLockHolder lock(&g_callback_state_lock);

  // Find the table info from the publications.
  auto iter = g_table_info_map.find(table_id);
  if (iter == g_table_info_map.end()) {
    return pl::error::Internal("Encountered unknown table id $0", table_id);
  }
  const pl::stirling::stirlingpb::InfoClass& table_info = iter->second;

  // Only output enabled tables (lookup by name).
  if (std::find(g_table_print_enables.begin(), g_table_print_enables.end(),
                table_info.schema().name()) == g_table_print_enables.end()) {
    return Status::OK();
  }

  PrintRecordBatch(table_info.schema().name(), table_info.schema(), *record_batch);
  return Status::OK();
}

void SignalHandler(int signum) {
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

// Get dynamic tracing program from either flags or enviornment variable.
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
      PL_ASSIGN_OR_EXIT(trace_program.text, pl::ReadFileToString(FLAGS_trace));
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
    PL_ASSIGN_OR_RETURN(DynamicTracepointDeployment compiled_tracepoint,
                        pl::carnot::planner::compiler::CompileTracepoint(trace_program_str.text));
    LOG(INFO) << compiled_tracepoint.DebugString();
    trace_program = std::make_unique<DynamicTracepointDeployment>(std::move(compiled_tracepoint));
  } else {
    trace_program = std::make_unique<DynamicTracepointDeployment>();
    bool success =
        google::protobuf::TextFormat::ParseFromString(trace_program_str.text, trace_program.get());
    if (!success) {
      return pl::error::Internal("Unable to parse trace file");
    }
  }

  // Automatically enable printing of this table.
  for (const auto& tracepoint : trace_program->tracepoints()) {
    g_table_print_enables.push_back(tracepoint.table_name());
  }

  sole::uuid trace_id = sole::uuid4();

  stirling->RegisterTracepoint(trace_id, std::move(trace_program));

  StatusOr<Publish> s;
  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    s = stirling->GetTracepointInfo(trace_id);
  } while (!s.ok() && s.code() == pl::statuspb::Code::RESOURCE_UNAVAILABLE);

  return s;
}

// A simple wrapper that shows how the data collector is to be hooked up
// In this case, agent and sources are fake.
int main(int argc, char** argv) {
  // Register signal handlers to clean-up on exit.
  // TODO(oazizi): Create a separate signal handling thread.
  // For now this is okay, because all these signals will be handled by the main thread,
  // which is just waiting for the worker thread to return.
  signal(SIGINT, SignalHandler);
  signal(SIGQUIT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  signal(SIGHUP, SignalHandler);

  pl::EnvironmentGuard env_guard(&argc, argv);

  LOG(INFO) << "Stirling Wrapper PID: " << getpid() << " TID: " << gettid();

  if (!FLAGS_trace.empty()) {
    // In dynamic tracing mode, don't load any other sources.
    // Presumably, user only wants their dynamic trace.
    LOG(INFO) << "Dynamic Trace provided. All other data sources will be disabled.";
    FLAGS_sources = "kNone";
  }

  std::optional<SourceRegistrySpecifier> sources =
      magic_enum::enum_cast<SourceRegistrySpecifier>(FLAGS_sources);
  if (!sources.has_value()) {
    LOG(ERROR) << absl::Substitute("$0 is not a valid source register specifier", FLAGS_sources);
  }

  if (!FLAGS_print_record_batches.empty()) {
    g_table_print_enables = absl::StrSplit(FLAGS_print_record_batches, ",", absl::SkipWhitespace());
  }

  std::unique_ptr<SourceRegistry> registry = pl::stirling::CreateSourceRegistry(sources.value());

  // Make Stirling.
  std::unique_ptr<Stirling> stirling = Stirling::Create(std::move(registry));
  g_stirling = stirling.get();

  // Get a publish proto message to subscribe from.
  Publish publication;
  stirling->GetPublishProto(&publication);
  IndexPublication(publication, &g_table_info_map);

  // Subscribe to all elements.
  // Stirling will update its schemas and sets up the data tables.
  PL_CHECK_OK(stirling->SetSubscription(pl::stirling::SubscribeToAllInfoClasses(publication)));

  // Set a dummy callback function (normally this would be in the agent).
  stirling->RegisterDataPushCallback(StirlingWrapperCallback);

  if (FLAGS_init_only) {
    LOG(INFO) << "Exiting after init.";
    return 0;
  }

  // Start measuring process stats after init.
  pl::ProcessStatsMonitor process_stats_monitor;
  g_process_stats_monitor = &process_stats_monitor;

  // Run Data Collector.
  std::thread run_thread = std::thread(&Stirling::Run, stirling.get());

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

      // Update the subscription to enable the new trace.
      PL_CHECK_OK(stirling->SetSubscription(pl::stirling::SubscribeToAllInfoClasses(trace_pub)));
    }
  }

  if (FLAGS_timeout_secs > 0) {
    // Run for the specified amount of time, then terminate.
    LOG(INFO) << absl::Substitute("Running for $0 seconds.", FLAGS_timeout_secs);
    std::this_thread::sleep_for(std::chrono::milliseconds(FLAGS_timeout_secs));
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
