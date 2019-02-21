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

void StirlingWrapperCallback(uint64_t num_records,
                             std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  for (uint32_t i = 0; i < num_records; ++i) {
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

std::unique_ptr<SourceRegistry> CreateRegistry() {
  // Create a registry of sources;
  std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>("fake_news");

  // bpftrace source
  registry->RegisterOrDie<CPUStatBPFTraceConnector>("CPU stats bpftrace source");

  // RegisterAllSources(registry.get());

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
