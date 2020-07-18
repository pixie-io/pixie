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
#include "src/stirling/http_table.h"
#include "src/stirling/mysql_table.h"
#include "src/stirling/pgsql_table.h"

using pl::stirling::IndexPublication;
using pl::stirling::PrintRecordBatch;
using pl::stirling::SourceRegistry;
using pl::stirling::SourceRegistrySpecifier;
using pl::stirling::Stirling;
using pl::stirling::stirlingpb::Publish;
using pl::stirling::stirlingpb::Subscribe;
using DynamicTracingProgram = pl::stirling::dynamic_tracing::ir::logical::Program;

using pl::types::ColumnWrapperRecordBatch;
using pl::types::TabletID;

using pl::stirling::kCQLTable;
using pl::stirling::kHTTPTable;
using pl::stirling::kMySQLTable;
using pl::stirling::kPGSQLTable;

DEFINE_string(sources, "kProd", "[kAll|kProd|kMetrics|kTracers] Choose sources to enable.");
DEFINE_string(trace, "", "Dynamic trace to deploy.");
DEFINE_string(print_record_batches, "",
              "Comma-separated list of tables to print. Defaults to tracers if not specified. Use "
              "'None' for none.");
DEFINE_bool(init_only, false, "If true, only runs the init phase and exits. For testing.");
DEFINE_int32(timeout_secs, 0,
             "If greater than 0, only runs for the specified amount of time and exits.");

std::vector<std::string> g_table_print_enables = {
    std::string(kHTTPTable.name()), std::string(kMySQLTable.name()), std::string(kCQLTable.name())};

// Put this in global space, so we can kill it in the signal handler.
Stirling* g_stirling = nullptr;
pl::ProcessStatsMonitor* g_process_stats_monitor = nullptr;
absl::flat_hash_map<uint64_t, const ::pl::stirling::stirlingpb::InfoClass*> g_table_info_map;
absl::base_internal::SpinLock g_callback_state_lock;

void StirlingWrapperCallback(uint64_t table_id, TabletID /* tablet_id */,
                             std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  absl::base_internal::SpinLockHolder lock(&g_callback_state_lock);

  // Find the table info from the publications.
  auto iter = g_table_info_map.find(table_id);
  if (iter == g_table_info_map.end()) {
    LOG(DFATAL) << absl::Substitute("Encountered unknown table id $0", table_id);
    return;
  }
  const pl::stirling::stirlingpb::InfoClass& table_info = *(iter->second);

  // Only output enabled tables (lookup by name).
  if (std::find(g_table_print_enables.begin(), g_table_print_enables.end(), table_info.name()) ==
      g_table_print_enables.end()) {
    return;
  }

  PrintRecordBatch(table_info.name(), table_info.schema(), *record_batch);
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

void Subscribe(Stirling* stirling) {
  // Declare as static, since g_table_info_map will point into this structure.
  static Publish publication;

  // Get the new publication.
  // Do this under a lock to avoid races with any currently executing StirlingWrapperCallback().
  {
    absl::base_internal::SpinLockHolder lock(&g_callback_state_lock);
    stirling->GetPublishProto(&publication);
    g_table_info_map = IndexPublication(publication);
  }

  // Subscribe to all elements.
  // Stirling will update its schemas and sets up the data tables.
  PL_CHECK_OK(stirling->SetSubscription(pl::stirling::SubscribeToAllInfoClasses(publication)));
}

void DeployTrace(Stirling* stirling) {
  PL_ASSIGN_OR_EXIT(std::string trace_program_str, pl::ReadFileToString(FLAGS_trace));

  auto trace_program = std::make_unique<DynamicTracingProgram>();
  bool success =
      google::protobuf::TextFormat::ParseFromString(trace_program_str, trace_program.get());
  if (!success) {
    LOG(ERROR) << "Unable to parse trace file";
    return;
  }

  // Automatically enable printing of this table.
  for (const auto& o : trace_program->outputs()) {
    g_table_print_enables.push_back(o.name());
  }

  int64_t trace_id = stirling->RegisterDynamicTrace(std::move(trace_program));

  pl::Status s;
  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    s = stirling->CheckDynamicTraceStatus(trace_id);
  } while (!s.ok() && s.code() == pl::statuspb::Code::RESOURCE_UNAVAILABLE);

  if (s.ok()) {
    LOG(INFO) << "Successfully deployed dynamic trace!";
  } else {
    LOG(WARNING) << s.ToString();
  }
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
  Subscribe(stirling.get());

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

  if (!FLAGS_trace.empty()) {
    // Dynamic traces are sources that are enabled while Stirling is running.
    // Deploy after a brief delay to emulate how it would be used in a PEM environment.
    // Could also consider removing this altogether.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    DeployTrace(stirling.get());

    // Update the subscription to enable the new trace.
    Subscribe(stirling.get());
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
