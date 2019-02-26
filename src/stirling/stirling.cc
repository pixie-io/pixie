#include <chrono>
#include <utility>

#include "src/common/common.h"
#include "src/stirling/bpftrace_connector.h"
#include "src/stirling/pub_sub_manager.h"
#include "src/stirling/source_connector.h"
#include "src/stirling/stirling.h"

namespace pl {
namespace stirling {

stirlingpb::Subscribe SubscribeToAllElements(const stirlingpb::Publish& publish_proto) {
  stirlingpb::Subscribe subscribe_proto;

  for (int i = 0; i < publish_proto.published_info_classes_size(); ++i) {
    auto sub_info_class = subscribe_proto.add_subscribed_info_classes();
    sub_info_class->MergeFrom(publish_proto.published_info_classes(i));
    for (int j = 0; j < sub_info_class->elements_size(); ++j) {
      auto element = sub_info_class->mutable_elements(j);
      element->set_state(stirlingpb::Element_State::Element_State_SUBSCRIBED);
    }
  }
  return subscribe_proto;
}

Status Stirling::Init() {
  PL_RETURN_IF_ERROR(CreateSourceConnectors());
  return Status::OK();
}

Status Stirling::CreateSourceConnectors() {
  if (!registry_) {
    return error::NotFound("Source registry doesn't exist");
  }
  auto sources = registry_->sources();
  for (auto const& [name, registry_element] : sources) {
    Status s = AddSource(name, registry_element.create_source_fn(name));

    if (!s.ok()) {
      LOG(WARNING) << absl::StrFormat("Source Connector (registry name=%s) not instantiated", name);
      LOG(WARNING) << s.status().ToString();
    }
  }
  return Status::OK();
}

Status Stirling::AddSource(const std::string& name, std::unique_ptr<SourceConnector> source) {
  // Step 1: Init the source.
  PL_RETURN_IF_ERROR(source->Init());

  // TODO(oazizi): What if a Source has multiple InfoClasses?
  auto mgr = std::make_unique<InfoClassManager>(name);

  // Step 3: Ask the Connector to populate the Schema.
  PL_CHECK_OK(source->PopulateSchema(mgr.get()));
  mgr->SetSourceConnector(source.get());

  // Step 5: Keep pointers to all the objects
  sources_.push_back(std::move(source));
  info_class_mgrs_.push_back(std::move(mgr));

  return Status::OK();
}

stirlingpb::Publish Stirling::GetPublishProto() { return config_->GeneratePublishProto(); }

Status Stirling::SetSubscription(const stirlingpb::Subscribe& subscribe_proto) {
  // Update schemas based on the subscribe_proto.
  // TODO(kgandhi/oazizi) : Rethink implicit schemas_ update. May be move the update
  // function into InfoClassManager
  PL_CHECK_OK(config_->UpdateSchemaFromSubscribe(subscribe_proto));

  // TODO(kgandhi/oazizi): Clear the tables based on new subscription.

  // Generate the tables required based on subscribed Elements.
  for (const auto& mgr : info_class_mgrs_) {
    auto data_table = std::make_unique<ColumnWrapperDataTable>(mgr->Schema());
    mgr->SetDataTable(data_table.get());
    mgr->SetSamplingPeriod(kDefaultSamplingPeriod);
    tables_.push_back(std::move(data_table));
  }

  return Status::OK();
}

// Main call to start the data collection.
void Stirling::Run() {
  run_thread_ = std::thread(&Stirling::RunThread, this);
  // TODO(oazizi): Make sure this is not called multiple times...don't want thread proliferation.
}

void Stirling::Wait() { run_thread_.join(); }

// Main Data Collector loop.
// Poll on Data Source Through connectors, when appropriate, then go to sleep.
// Must run as a thread, so only call from Run() as a thread.
void Stirling::RunThread() {
  // TODO(oazizi): Remove this. Done to make sure first sample is collected.
  std::this_thread::sleep_for(std::chrono::seconds(1));

  bool run = true;
  while (run) {
    // Run through every InfoClass being managed.
    for (const auto& mgr : info_class_mgrs_) {
      // Phase 1: Probe each source for its data.
      if (mgr->SamplingRequired()) {
        auto source = mgr->GetSourceConnector();
        auto data_table = mgr->GetDataTable();

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
      if (mgr->PushRequired()) {
        auto data_table = mgr->GetDataTable();

        auto record_batches = data_table->GetColumnWrapperRecordBatches();
        auto record_batches_ptr_raw = record_batches.ValueOrDie().get();
        for (auto& record_batch : *record_batches_ptr_raw) {
          if (record_batch->size() > 0) {
            agent_callback_(mgr->id(), std::move(record_batch));
          }
        }
      }

      // Optional: Update sampling periods if we are dropping data.
    }

    // Figure out how long to sleep.
    SleepUntilNextTick();
  }
}

// Helper function: Figure out when to wake up next.
void Stirling::SleepUntilNextTick() {
  // FIXME(oazizi): This is bogus.
  // The amount to sleep depends on when the earliest Source needs to be sampled again.
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

}  // namespace stirling
}  // namespace pl
