#include <ctime>
#include <iomanip>
#include <thread>

#include "src/common/base/base.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/pid_runtime_connector.h"
#include "src/stirling/seq_gen_connector.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/system_stats_connector.h"
#include "src/stirling/types.h"

using pl::stirling::AgentMetadataType;
using pl::stirling::SourceRegistry;
using pl::stirling::Stirling;
using pl::stirling::stirlingpb::Publish;
using pl::stirling::stirlingpb::Subscribe;

using pl::types::ColumnWrapperRecordBatch;
using pl::types::DataType;
using pl::types::Float64Value;
using pl::types::Int64Value;
using pl::types::SharedColumnWrapper;
using pl::types::StringValue;
using pl::types::TabletID;
using pl::types::Time64NSValue;
using pl::types::UInt128Value;

using pl::stirling::PIDRuntimeConnector;
using pl::stirling::SeqGenConnector;
using pl::stirling::SocketTraceConnector;
using pl::stirling::SystemStatsConnector;

using pl::stirling::DataElement;
using pl::stirling::kHTTPTable;

using pl::ConstVectorView;

DEFINE_string(source_name, "*", "The name of the source to report.");
DEFINE_bool(all_sources, false,
            "If true, turns on all sources. Default turns on only prod sources.");

std::unordered_map<uint64_t, std::string> table_id_to_name_map;

void PrintRecordBatch(std::string_view prefix, const ConstVectorView<DataElement>& schema,
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
          std::cout << absl::Substitute("{$0,$1}", val.High64(), val.Low64()) << " ";
        } break;
        default:
          CHECK(false) << absl::Substitute("Unrecognized type: $0", ToString(schema[j].type()));
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

  // Use assigned names, from registry.
  if (name == SeqGenConnector::kSeq0Table.name().data()) {
    PrintRecordBatch("SeqGen-0", SeqGenConnector::kSeq0Table.elements(), num_records,
                     *record_batch);
  } else if (name == SeqGenConnector::kSeq1Table.name().data()) {
    PrintRecordBatch("SeqGen-1", SeqGenConnector::kSeq1Table.elements(), num_records,
                     *record_batch);
  } else if (name == SocketTraceConnector::kMySQLTable.name().data()) {
    PrintRecordBatch("MySQLTrace", SocketTraceConnector::kMySQLTable.elements(), num_records,
                     *record_batch);
  } else if (name == PIDRuntimeConnector::kTable.name().data()) {
    PrintRecordBatch("PIDStat-BCC", PIDRuntimeConnector::kTable.elements(), num_records,
                     *record_batch);
  } else if (name == kHTTPTable.name().data()) {
    PrintRecordBatch("HTTPTrace", kHTTPTable.elements(), num_records, *record_batch);
  } else if (name == SystemStatsConnector::kProcessStatsTable.name().data()) {
    PrintRecordBatch("ProcessStats", SystemStatsConnector::kProcessStatsTable.elements(),
                     num_records, *record_batch);
  } else if (name == SystemStatsConnector::kNetworkStatsTable.name().data()) {
    PrintRecordBatch("NetStats", SystemStatsConnector::kNetworkStatsTable.elements(), num_records,
                     *record_batch);
  }
  // Can add other connectors, if desired, here.
}

AgentMetadataType AgentMetadataCallback() {
  // Injecting empty state here. If we want to monitor some pids we can directly add them to this
  // data structure.
  return std::make_shared<const pl::md::AgentMetadataState>(/* asid */ 1);
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

  pl::InitEnvironmentOrDie(&argc, argv);
  LOG(INFO) << "Stirling Wrapper PID: " << getpid() << " TID: " << std::this_thread::get_id();

  // Make Stirling.
  std::unique_ptr<SourceRegistry> registry = FLAGS_all_sources
                                                 ? pl::stirling::CreateAllSourceRegistry()
                                                 : pl::stirling::CreateProdSourceRegistry();
  std::unique_ptr<Stirling> stirling = Stirling::Create(std::move(registry));
  g_stirling = stirling.get();

  // Get a publish proto message to subscribe from.
  Publish publish_proto;
  stirling->GetPublishProto(&publish_proto);

  // Subscribe to all elements.
  // Stirling will update its schemas and sets up the data tables.
  pl::stirling::stirlingpb::Subscribe subscribe_proto;
  if (FLAGS_source_name == "*") {
    subscribe_proto = pl::stirling::SubscribeToAllInfoClasses(publish_proto);
  } else {
    subscribe_proto = pl::stirling::SubscribeToInfoClass(publish_proto, FLAGS_source_name);
  }
  PL_CHECK_OK(stirling->SetSubscription(subscribe_proto));

  // Get a map from InfoClassManager names to Table IDs
  table_id_to_name_map = stirling->TableIDToNameMap();

  // Set a dummy callback function (normally this would be in the agent).
  stirling->RegisterCallback(StirlingWrapperCallback);

  stirling->RegisterAgentMetadataCallback(AgentMetadataCallback);

  // Run Data Collector.
  std::thread run_thread = std::thread(&Stirling::Run, stirling.get());

  // Wait for the thread to return. This should never happen in this example.
  // But don't want the program to terminate.
  run_thread.join();

  // Another model of how to run Stirling:
  // stirling->RunAsThread();
  // stirling->WaitForThreadJoin();

  pl::ShutdownEnvironmentOrDie();

  return 0;
}
