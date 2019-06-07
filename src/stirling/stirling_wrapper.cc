#include <ctime>
#include <iomanip>
#include <thread>

#include "src/common/base/base.h"
#include "src/stirling/bcc_connector.h"
#include "src/stirling/bpftrace_connector.h"
#include "src/stirling/cgroup_stats_connector.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/seq_gen_connector.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/types.h"

using pl::stirling::SourceRegistry;
using pl::stirling::SourceType;
using pl::stirling::Stirling;
using pl::stirling::stirlingpb::Publish;
using pl::stirling::stirlingpb::Subscribe;

using pl::types::ColumnWrapperRecordBatch;
using pl::types::DataType;
using pl::types::Float64Value;
using pl::types::Int64Value;
using pl::types::SharedColumnWrapper;
using pl::types::StringValue;
using pl::types::Time64NSValue;

using pl::stirling::CGroupStatsConnector;
using pl::stirling::CPUStatBPFTraceConnector;
using pl::stirling::PIDCPUUseBCCConnector;
using pl::stirling::PIDCPUUseBPFTraceConnector;
using pl::stirling::SeqGenConnector;
using pl::stirling::SocketTraceConnector;

using pl::stirling::DataElement;

using pl::ConstVectorView;

DEFINE_string(source_name, "*", "The name of the source to report.");

std::unordered_map<uint64_t, std::string> table_id_to_name_map;

void PrintRecordBatch(std::string prefix, const ConstVectorView<DataElement>& schema,
                      uint64_t num_records, const ColumnWrapperRecordBatch& record_batch) {
  for (uint32_t i = 0; i < num_records; ++i) {
    std::cout << "[" << prefix << "] ";

    uint32_t j = 0;
    for (SharedColumnWrapper col : record_batch) {
      switch (schema[j].type()) {
        case DataType::TIME64NS: {
          const auto val = col->Get<Time64NSValue>(i).val;
          std::time_t time = val / 1000000000ULL;
          std::cout << std::put_time(std::localtime(&time), "%Y-%m-%d %X") << " | ";
        } break;
        case DataType::INT64: {
          const auto val = col->Get<Int64Value>(i).val;
          std::cout << val << " ";
        } break;
        case DataType::FLOAT64: {
          const auto val = col->Get<Float64Value>(i).val;
          std::cout << val << " ";
        } break;
        case DataType::STRING: {
          const auto& val = col->Get<StringValue>(i);
          std::cout << val << " ";
        } break;
        default:
          CHECK(false) << absl::StrFormat("Unrecognized type: $%s", ToString(schema[j].type()));
      }

      j++;
    }
    std::cout << std::endl;
  }
}

void StirlingWrapperCallback(uint64_t table_id,
                             std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  // Note: Implicit assumption (not checked here) is that all columns have the same size
  uint64_t num_records = (*record_batch)[0]->Size();

  std::string name = table_id_to_name_map[table_id];

  // Use assigned names, from registry.
  if (name == CPUStatBPFTraceConnector::kTable.name().get()) {
    PrintRecordBatch("CPUStat-BPFTrace", CPUStatBPFTraceConnector::kTable.elements(), num_records,
                     *record_batch);
  } else if (name == SeqGenConnector::kSeq0Table.name().get()) {
    PrintRecordBatch("SeqGen-0", SeqGenConnector::kSeq0Table.elements(), num_records,
                     *record_batch);
  } else if (name == SeqGenConnector::kSeq1Table.name().get()) {
    PrintRecordBatch("SeqGen-1", SeqGenConnector::kSeq1Table.elements(), num_records,
                     *record_batch);
  } else if (name == PIDCPUUseBPFTraceConnector::kTable.name().get()) {
    PrintRecordBatch("PIDStat-BPFTrace", PIDCPUUseBPFTraceConnector::kTable.elements(), num_records,
                     *record_batch);
  } else if (name == SocketTraceConnector::kMySQLTable.name().get()) {
    PrintRecordBatch("MySQLTrace", SocketTraceConnector::kMySQLTable.elements(), num_records,
                     *record_batch);
  } else if (name == PIDCPUUseBCCConnector::kTable.name().get()) {
    PrintRecordBatch("PIDStat-BCC", PIDCPUUseBCCConnector::kTable.elements(), num_records,
                     *record_batch);
  } else if (name == SocketTraceConnector::kHTTPTable.name().get()) {
    PrintRecordBatch("HTTPTrace", SocketTraceConnector::kHTTPTable.elements(), num_records,
                     *record_batch);
  } else if (name == CGroupStatsConnector::kCPUTable.name().get()) {
    PrintRecordBatch("CGroupStats", CGroupStatsConnector::kCPUTable.elements(), num_records,
                     *record_batch);
  } else if (name == CGroupStatsConnector::kNetworkTable.name().get()) {
    PrintRecordBatch("NetStats", CGroupStatsConnector::kNetworkTable.elements(), num_records,
                     *record_batch);
  }
  // Can add other connectors, if desired, here.
}

std::unique_ptr<SourceRegistry> CreateRegistry() {
  // Create a registry of sources;
  std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();
  RegisterAllSources(registry.get());
  return registry;
}

// A simple wrapper that shows how the data collector is to be hooked up
// In this case, agent and sources are fake.
int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);
  LOG(INFO) << "Stirling Wrapper";

  // Create a registry of relevant sources.
  std::unique_ptr<SourceRegistry> registry = CreateRegistry();

  // Make Stirling.
  auto data_collector = Stirling::Create(std::move(registry));

  // Get a publish proto message to subscribe from.
  Publish publish_proto;
  data_collector->GetPublishProto(&publish_proto);

  // Subscribe to all elements.
  // Stirling will update its schemas and sets up the data tables.
  pl::stirling::stirlingpb::Subscribe subscribe_proto;
  if (FLAGS_source_name == "*") {
    subscribe_proto = pl::stirling::SubscribeToAllInfoClasses(publish_proto);
  } else {
    subscribe_proto = pl::stirling::SubscribeToInfoClass(publish_proto, FLAGS_source_name);
  }
  PL_CHECK_OK(data_collector->SetSubscription(subscribe_proto));

  // Get a map from InfoClassManager names to Table IDs
  table_id_to_name_map = data_collector->TableIDToNameMap();

  // Set a dummy callback function (normally this would be in the agent).
  data_collector->RegisterCallback(StirlingWrapperCallback);

  // Run Data Collector.
  std::thread run_thread = std::thread(&Stirling::Run, data_collector.get());

  // Wait for the thread to return. This should never happen in this example.
  // But don't want the program to terminate.
  run_thread.join();

  // Another model of how to run the Data Collector:
  // data_collector->RunAsThread();
  // data_collector->WaitForThreadJoin();

  pl::ShutdownEnvironmentOrDie();

  return 0;
}
