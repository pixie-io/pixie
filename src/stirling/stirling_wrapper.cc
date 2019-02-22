#include <glog/logging.h>
#include <ctime>
#include <iomanip>

#include "src/common/env.h"
#include "src/common/error.h"
#include "src/common/macros.h"
#include "src/common/status.h"
#include "src/stirling/bpftrace_connector.h"
#include "src/stirling/proc_stat_connector.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"

using pl::stirling::SourceRegistry;
using pl::stirling::SourceType;
using pl::stirling::Stirling;

using pl::carnot::udf::Int64ValueColumnWrapper;
using pl::carnot::udf::SharedColumnWrapper;
using pl::stirling::ColumnWrapperRecordBatch;

using pl::stirling::CPUStatBPFTraceConnector;
using pl::stirling::FakeProcStatConnector;

std::unordered_map<uint64_t, std::string> table_id_to_name_map;

void PrintCPUStatBPFTraceRecordBatch(uint64_t num_records,
                                     std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  for (uint32_t i = 0; i < num_records; ++i) {
    std::cout << "[CPUStatsBPFTrace] ";

    uint32_t j = 0;
    for (SharedColumnWrapper col : *record_batch) {
      auto typedCol = std::static_pointer_cast<Int64ValueColumnWrapper>(col);
      if (j == 0) {
        auto ns_count = (*typedCol)[i].val;
        std::time_t time = ns_count / 1000000000ULL;
        std::cout << std::put_time(std::localtime(&time), "%Y-%m-%d %X") << " | ";
      } else {
        int64_t val = (*typedCol)[i].val;
        std::cout << val << " ";
      }

      j++;
    }
  }
  std::cout << std::endl;
}

void StirlingWrapperCallback(uint64_t table_id,
                             std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  // Note: Implicit assumption (not checked here) is that all columns have the same size
  uint64_t num_records = (*record_batch)[0]->Size();

  std::string name = table_id_to_name_map[table_id];

  // Uses InfoClassSchema names, which come from Registry.
  // So use Registry names here.
  if (name == "CPU stats bpftrace source") {
    PrintCPUStatBPFTraceRecordBatch(num_records, std::move(record_batch));
  }
  // TODO(oazizi): May want to implement this at some point.
  // else {
  //  std::cout << "[" << name << "] <TODO: print out data>" << std::endl;
  //}
}

std::unique_ptr<SourceRegistry> CreateRegistry() {
  // Create a registry of sources;
  std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>("fake_news");

  RegisterAllSources(registry.get());

  std::cout << "Registered sources: " << std::endl;
  auto registered_sources = registry->sources_map();
  for (auto registered_source : registered_sources) {
    std::cout << "    " << registered_source.first << std::endl;
  }

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
  Stirling data_collector(std::move(registry));

  // TODO(oazizi): Should this automatically be done by the constructor?
  PL_CHECK_OK(data_collector.CreateSourceConnectors());

  // Get a publish proto message to subscribe from.
  auto publish_proto = data_collector.GetPublishProto();

  auto subscribe_proto = pl::stirling::SubscribeToAllElements(publish_proto);

  // Get subscription and then data collector updates its schemas and sets
  // up the data tables.
  PL_CHECK_OK(data_collector.SetSubscription(subscribe_proto));
  // Get a map from InfoClassSchema names to Table IDs
  table_id_to_name_map = data_collector.TableIDToNameMap();

  // Set a dummy callback function (normally this would be in the agent).
  data_collector.RegisterCallback(StirlingWrapperCallback);

  // Run Data Collector.
  data_collector.Run();

  // Wait for the thread to return. This should never happen in this example.
  // But don't want the program to terminate.
  data_collector.Wait();

  pl::ShutdownEnvironmentOrDie();

  return 0;
}
