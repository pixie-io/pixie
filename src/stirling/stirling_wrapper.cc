#include <sys/syscall.h>
#include <unistd.h>
#define gettid() syscall(SYS_gettid)

#include <ctime>
#include <iomanip>
#include <thread>

#include "absl/strings/str_split.h"
#include "src/common/base/base.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/jvm_stats_connector.h"
#include "src/stirling/pid_runtime_connector.h"
#include "src/stirling/seq_gen_connector.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/system_stats_connector.h"
#include "src/stirling/types.h"

using pl::stirling::AgentMetadataType;
using pl::stirling::SourceRegistry;
using pl::stirling::SourceRegistrySpecifier;
using pl::stirling::Stirling;
using pl::stirling::stirlingpb::Publish;
using pl::stirling::stirlingpb::Subscribe;

using pl::types::ColumnWrapperRecordBatch;
using pl::types::DataType;
using pl::types::Duration64NSValue;
using pl::types::Float64Value;
using pl::types::Int64Value;
using pl::types::SharedColumnWrapper;
using pl::types::StringValue;
using pl::types::TabletID;
using pl::types::Time64NSValue;
using pl::types::UInt128Value;

using pl::stirling::JVMStatsConnector;
using pl::stirling::PIDRuntimeConnector;
using pl::stirling::SeqGenConnector;
using pl::stirling::SocketTraceConnector;
using pl::stirling::SystemStatsConnector;

using pl::stirling::DataElement;
using pl::stirling::kCQLTable;
using pl::stirling::kHTTPTable;
using pl::stirling::kJVMStatsTable;
using pl::stirling::kMySQLTable;

using pl::ArrayView;

DEFINE_string(sources, "kProd", "[kAll|kProd|kMetrics|kTracers] Choose sources to enable.");
DEFINE_string(print_record_batches, "",
              "Comma-separated list of tables to print. Defaults to tracers if not specified. Use "
              "'None' for none.");
DEFINE_bool(init_only, false, "If true, only runs the init phase and exits. For testing.");

std::unordered_map<uint64_t, std::string> table_id_to_name_map;
std::vector<std::string_view> table_print_enables = {kHTTPTable.name(), kMySQLTable.name(),
                                                     kCQLTable.name()};

void PrintRecordBatch(std::string_view prefix, const ArrayView<DataElement>& schema,
                      size_t num_records, const ColumnWrapperRecordBatch& record_batch) {
  for (size_t i = 0; i < num_records; ++i) {
    std::cout << "[" << prefix << "]";

    uint32_t j = 0;
    for (const auto& col : record_batch) {
      std::cout << " " << schema[j].name() << ":";
      switch (schema[j].type()) {
        case DataType::TIME64NS: {
          const auto val = col->Get<Time64NSValue>(i).val;
          std::time_t time = val / 1000000000UL;
          std::cout << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %X") << "]";
        } break;
        case DataType::INT64: {
          const auto val = col->Get<Int64Value>(i).val;
          std::cout << "[" << val << "]";
        } break;
        case DataType::FLOAT64: {
          const auto val = col->Get<Float64Value>(i).val;
          std::cout << "[" << val << "]";
        } break;
        case DataType::STRING: {
          const auto& val = col->Get<StringValue>(i);
          std::cout << "[" << val << "]";
        } break;
        case DataType::UINT128: {
          const auto& val = col->Get<UInt128Value>(i);
          std::cout << "[" << absl::Substitute("{$0,$1}", val.High64(), val.Low64()) << "]";
        } break;
        case DataType::DURATION64NS: {
          const auto secs = std::chrono::duration_cast<std::chrono::duration<double>>(
              std::chrono::nanoseconds(col->Get<Duration64NSValue>(i).val));
          std::cout << absl::Substitute("[$0 seconds]", secs.count());
        } break;
        default:
          LOG(DFATAL) << absl::Substitute("Unrecognized type: $0", ToString(schema[j].type()));
      }
      j++;
    }
    std::cout << std::endl;
  }
}

void StirlingWrapperCallback(uint64_t table_id, TabletID /* tablet_id */,
                             std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  // Note: Implicit assumption (not checked here) is that all columns have the same size
  size_t num_records = (*record_batch)[0]->Size();

  std::string name = table_id_to_name_map[table_id];

  if (std::find(table_print_enables.begin(), table_print_enables.end(), name) ==
      table_print_enables.end()) {
    return;
  }

  // Use assigned names, from registry.
  if (name == SeqGenConnector::kSeq0Table.name()) {
    PrintRecordBatch("SeqGen-0", SeqGenConnector::kSeq0Table.elements(), num_records,
                     *record_batch);
  } else if (name == SeqGenConnector::kSeq1Table.name()) {
    PrintRecordBatch("SeqGen-1", SeqGenConnector::kSeq1Table.elements(), num_records,
                     *record_batch);
  } else if (name == kMySQLTable.name()) {
    PrintRecordBatch("MySQLTrace", kMySQLTable.elements(), num_records, *record_batch);
  } else if (name == kCQLTable.name()) {
    PrintRecordBatch("CQLTrace", kCQLTable.elements(), num_records, *record_batch);
  } else if (name == PIDRuntimeConnector::kTable.name()) {
    PrintRecordBatch("PIDStat-BCC", PIDRuntimeConnector::kTable.elements(), num_records,
                     *record_batch);
  } else if (name == kHTTPTable.name()) {
    PrintRecordBatch("HTTPTrace", kHTTPTable.elements(), num_records, *record_batch);
  } else if (name == SystemStatsConnector::kProcessStatsTable.name()) {
    PrintRecordBatch("ProcessStats", SystemStatsConnector::kProcessStatsTable.elements(),
                     num_records, *record_batch);
  } else if (name == SystemStatsConnector::kNetworkStatsTable.name()) {
    PrintRecordBatch("NetStats", SystemStatsConnector::kNetworkStatsTable.elements(), num_records,
                     *record_batch);
  } else if (name == kJVMStatsTable.name()) {
    PrintRecordBatch("JVMStats", kJVMStatsTable.elements(), num_records, *record_batch);
  }
  // Can add other connectors, if desired, here.
}

AgentMetadataType AgentMetadataCallback() {
  // Injecting empty state here. If we want to monitor some pids we can directly add them to this
  // data structure.

  // Set ASID to 0, so {ASID, PID} == {PID}.
  return std::make_shared<const pl::md::AgentMetadataState>(/* asid */ 0);
}

// Put this in global space, so we can kill it in the signal handler.
Stirling* g_stirling = nullptr;

void SignalHandler(int signum) {
  // Important to call Stop(), because it releases BPF resources,
  // which would otherwise leak.
  if (g_stirling != nullptr) {
    g_stirling->Stop();
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
    table_print_enables = absl::StrSplit(FLAGS_print_record_batches, " ", absl::SkipWhitespace());
  }

  std::unique_ptr<SourceRegistry> registry = pl::stirling::CreateSourceRegistry(sources.value());

  // Make Stirling.
  std::unique_ptr<Stirling> stirling = Stirling::Create(std::move(registry));
  g_stirling = stirling.get();

  // Get a publish proto message to subscribe from.
  Publish publish_proto;
  stirling->GetPublishProto(&publish_proto);

  // Subscribe to all elements.
  // Stirling will update its schemas and sets up the data tables.
  auto subscribe_proto = pl::stirling::SubscribeToAllInfoClasses(publish_proto);
  PL_CHECK_OK(stirling->SetSubscription(subscribe_proto));

  // Get a map from InfoClassManager names to Table IDs
  table_id_to_name_map = stirling->TableIDToNameMap();

  // Set a dummy callback function (normally this would be in the agent).
  stirling->RegisterCallback(StirlingWrapperCallback);

  stirling->RegisterAgentMetadataCallback(AgentMetadataCallback);

  if (FLAGS_init_only) {
    LOG(INFO) << "Exiting after init.";
    return 0;
  }

  // Run Data Collector.
  std::thread run_thread = std::thread(&Stirling::Run, stirling.get());

  // Wait for the thread to return. This should never happen in this example.
  // But don't want the program to terminate.
  run_thread.join();

  // Another model of how to run Stirling:
  // stirling->RunAsThread();
  // stirling->WaitForThreadJoin();

  return 0;
}
