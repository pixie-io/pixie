#include <iostream>
#include <utility>

#include "src/data_collector/data_collector.h"
#include "src/data_collector/data_collector_config.h"
#include "src/data_collector/source_connector.h"

namespace pl {
namespace datacollector {

DataCollector::DataCollector() { config_ = std::make_unique<DataCollectorConfig>(schemas_); }

// Add an EBPF source to the Data Collector.
Status DataCollector::AddEBPFSource(const std::string& name, const std::string& ebpf_src,
                                    const std::string& kernel_event, const std::string& fn_name) {
  // Step 1: Create the Connector (with EBPF program attached).
  auto source = std::make_unique<EBPFConnector>(name, ebpf_src, kernel_event, fn_name);

  return AddSource(name, std::move(source));
}

// Add an OpenTracing source to the Data Collector.
Status DataCollector::AddOpenTracingSource(const std::string& name) {
  // Step 1: Create the Connector
  auto source = std::make_unique<OpenTracingConnector>(name);

  return AddSource(name, std::move(source));
}

Status DataCollector::AddSource(const std::string& name, std::unique_ptr<SourceConnector> source) {
  // Step 2: Ask the Connector for the Schema.
  // Eventually, should return a vector of Schemas.
  auto schema = std::make_unique<InfoClassSchema>(name);
  PL_RETURN_IF_ERROR(source->PopulateSchema(schema.get()));

  // Step 3: Make the corresponding Data Table.
  auto data_table = std::make_unique<DataTable>(*schema);

  // Step 4: Connect this Info Class to its related objects.
  schema->SetSourceConnector(source.get());
  schema->SetDataTable(data_table.get());

  // Step 5: Keep pointers to all the objects
  sources_.push_back(std::move(source));
  tables_.push_back(std::move(data_table));
  schemas_.push_back(std::move(schema));

  return Status::OK();
}

// Main call to start the data collection.
void DataCollector::Run() {
  run_thread_ = std::thread(&DataCollector::RunThread, this);
  // TODO(oazizi): Make sure this is not called multiple times...don't want thread proliferation.
}

void DataCollector::Wait() { run_thread_.join(); }

// Main Data Collector loop.
// Poll on Data Source Through connectors, when appropriate, then go to sleep.
// Must run as a thread, so only call from Run() as a thread.
void DataCollector::RunThread() {
  while (true) {
    std::chrono::milliseconds current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    // Probe each source for its data, and push upstream.
    for (const auto& schema : schemas_) {
      if (current_time > schema->NextSamplingTime()) {
        auto source = schema->GetSourceConnector();
        auto data_table = schema->GetDataTable();

        // Get pointer to data.
        // Source manages its own buffer as appropriate.
        // For example, EBPFConnector may want to copy data to user-space,
        // and then provide a pointer to the data.
        // The complexity of re-using same memory buffer then falls to the Data Source.
        auto* data = reinterpret_cast<char*>(source->GetData());

        data_table->AppendData(data, 1);

        // TODO(oazizi): Tell source how much data was consumed, so it can release the memory.

        // Optional: Update sampling periods if we are dropping data
      }
    }

    // Figure out how long to sleep.
    SleepUntilNextTick();

    // FIXME(oazizi): Remove this.
    std::cout << "." << std::flush;
  }
}

// Helper function: Figure out when to wake up next.
void DataCollector::SleepUntilNextTick() {
  // This is bogus.
  // The amount to sleep depends on when the earliest Source needs to be sampled again.
  std::this_thread::sleep_for(std::chrono::seconds(1));  // FIXME
}

}  // namespace datacollector
}  // namespace pl
