#include <sys/syscall.h>
#include <unistd.h>
#define gettid() syscall(SYS_gettid)

#include <ctime>
#include <iomanip>
#include <thread>

#include <absl/strings/str_split.h>

#include "src/common/base/base.h"
#include "src/stirling/output.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"

#include "src/stirling/cass_table.h"
#include "src/stirling/http_table.h"
#include "src/stirling/mysql_table.h"
#include "src/stirling/pgsql_table.h"

using pl::stirling::PrintRecordBatch;
using pl::stirling::SourceRegistry;
using pl::stirling::SourceRegistrySpecifier;
using pl::stirling::Stirling;
using pl::stirling::stirlingpb::Publish;
using pl::stirling::stirlingpb::Subscribe;

using pl::types::ColumnWrapperRecordBatch;
using pl::types::TabletID;

using pl::stirling::kCQLTable;
using pl::stirling::kHTTPTable;
using pl::stirling::kMySQLTable;
using pl::stirling::kPGSQLTable;

DEFINE_string(sources, "kProd", "[kAll|kProd|kMetrics|kTracers] Choose sources to enable.");
DEFINE_string(print_record_batches, "",
              "Comma-separated list of tables to print. Defaults to tracers if not specified. Use "
              "'None' for none.");
DEFINE_bool(init_only, false, "If true, only runs the init phase and exits. For testing.");
DEFINE_int32(timeout_secs, 0,
             "If greater than 0, only runs for the specified amount of time and exits.");

std::unordered_map<uint64_t, std::string> table_id_to_name_map;
std::vector<std::string_view> table_print_enables = {kHTTPTable.name(), kMySQLTable.name(),
                                                     kCQLTable.name()};

absl::TimeZone tz;

Publish* g_publication = nullptr;

void StirlingWrapperCallback(uint64_t table_id, TabletID /* tablet_id */,
                             std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  std::string name = table_id_to_name_map[table_id];

  // Only output enabled tables (lookup by name).
  if (std::find(table_print_enables.begin(), table_print_enables.end(), name) ==
      table_print_enables.end()) {
    return;
  }

  // Get the table schema for the table.
  DCHECK(g_publication != nullptr);
  const auto& info_classes = g_publication->published_info_classes();
  auto iter = std::find_if(
      info_classes.begin(), info_classes.end(),
      [&name](const pl::stirling::stirlingpb::InfoClass& x) { return x.name() == name; });
  if (iter == info_classes.end()) {
    return;
  }

  PrintRecordBatch(name, iter->schema(), *record_batch);
}

// Put this in global space, so we can kill it in the signal handler.
Stirling* g_stirling = nullptr;
pl::ProcessStatsMonitor* g_process_stats_monitor = nullptr;

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
    table_print_enables = absl::StrSplit(FLAGS_print_record_batches, ",", absl::SkipWhitespace());
  }

  std::unique_ptr<SourceRegistry> registry = pl::stirling::CreateSourceRegistry(sources.value());

  // Make Stirling.
  std::unique_ptr<Stirling> stirling = Stirling::Create(std::move(registry));
  g_stirling = stirling.get();

  // Get a publish proto message to subscribe from.
  Publish publication;
  stirling->GetPublishProto(&publication);
  g_publication = &publication;

  // Subscribe to all elements.
  // Stirling will update its schemas and sets up the data tables.
  auto subscription = pl::stirling::SubscribeToAllInfoClasses(publication);
  PL_CHECK_OK(stirling->SetSubscription(subscription));

  // Get a map from InfoClassManager names to Table IDs
  table_id_to_name_map = stirling->TableIDToNameMap();

  // Set a dummy callback function (normally this would be in the agent).
  stirling->RegisterDataPushCallback(StirlingWrapperCallback);

  // Timezone used by the callback function to print timestamps.
  CHECK(absl::LoadTimeZone("America/Los_Angeles", &tz));

  if (FLAGS_init_only) {
    LOG(INFO) << "Exiting after init.";
    return 0;
  }

  // Start measuring process stats after init.
  pl::ProcessStatsMonitor process_stats_monitor;
  g_process_stats_monitor = &process_stats_monitor;

  // Run Data Collector.
  std::thread run_thread = std::thread(&Stirling::Run, stirling.get());

  if (FLAGS_timeout_secs > 0) {
    // Run for the specified amount of time, then terminate.
    LOG(INFO) << absl::Substitute("Running for $0 seconds.", FLAGS_timeout_secs);
    sleep(FLAGS_timeout_secs);
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
