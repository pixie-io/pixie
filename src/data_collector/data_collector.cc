#include <iostream>
#include <utility>

#include "src/data_collector/data_collector.h"
#include "src/data_collector/pub_sub_manager.h"
#include "src/data_collector/source_connector.h"

namespace pl {
namespace datacollector {

Status DataCollector::CreateSourceConnectors() {
  if (!registry_) {
    return error::NotFound("Source registry doesn't exist");
  }
  auto sources_map = registry_->sources_map();
  for (auto const& [name, registry_element] : sources_map) {
    PL_CHECK_OK(AddSource(name, registry_element.create_source_fn()));
  }
  return Status::OK();
}

Status DataCollector::AddSource(const std::string& name, std::unique_ptr<SourceConnector> source) {
  // Step 1: Init the source.
  PL_CHECK_OK(source->Init());

  // Step 2: Ask the Connector for the Schema.
  // Eventually, should return a vector of Schemas.
  auto schema = std::make_unique<InfoClassSchema>(name);
  PL_RETURN_IF_ERROR(source->PopulateSchema(schema.get()));

  // Step 3: Make the corresponding Data Table.
  auto data_table = std::make_unique<ColumnWrapperDataTable>(*schema);

  // Step 4: Connect this Info Class to its related objects.
  schema->SetSourceConnector(source.get());
  schema->SetDataTable(data_table.get());
  schema->SetSamplingPeriod(kDefaultSamplingPeriod);

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
  bool run = true;
  while (run) {
    // Run through every InfoClass being managed.
    for (const auto& schema : schemas_) {
      // Phase 1: Probe each source for its data.
      if (schema->SamplingRequired()) {
        auto source = schema->GetSourceConnector();
        auto data_table = schema->GetDataTable();

        // Get pointer to data.
        // Source manages its own buffer as appropriate.
        // For example, EBPFConnector may want to copy data to user-space,
        // and then provide a pointer to the data.
        // The complexity of re-using same memory buffer then falls to the Data Source.
        auto source_data = source->GetData();
        auto num_records = source_data.num_records;
        auto* data_buf = reinterpret_cast<uint8_t*>(source_data.buf);
        PL_CHECK_OK(data_table->AppendData(data_buf, num_records));
      }

      // Phase 2: Push Data upstream.
      if (schema->PushRequired()) {
        auto data_table = schema->GetDataTable();
        auto record_batches = data_table->GetColumnWrapperRecordBatches();
        PL_UNUSED(record_batches);

        // TODO(oazizi): Hook this up.
        // for each record batch:
        //     agent_callback_(schema->id(), std::move(columns));
      }

      // Optional: Update sampling periods if we are dropping data.
    }

    // Figure out how long to sleep.
    SleepUntilNextTick();

    // FIXME(oazizi): Remove this.
    std::cout << "." << std::flush;
  }
}

// Helper function: Figure out when to wake up next.
void DataCollector::SleepUntilNextTick() {
  // FIXME(oazizi): This is bogus.
  // The amount to sleep depends on when the earliest Source needs to be sampled again.
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

}  // namespace datacollector
}  // namespace pl
