#include <memory>
#include <utility>
#include <vector>

#include "src/common/macros.h"

#include "src/stirling/data_table.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

std::atomic<uint64_t> InfoClassManager::global_id_ = 0;

stirlingpb::Element InfoClassElement::ToProto() const {
  stirlingpb::Element element_proto;
  element_proto.set_name(name_);
  element_proto.set_type(type_);
  return element_proto;
}

Status InfoClassManager::PopulateSchemaFromSource() {
  if (source_ == nullptr) {
    return error::ResourceUnavailable("Source connector has not been initialized.");
  }
  auto elements = source_->elements();
  for (const auto& element : elements) {
    elements_.emplace_back(InfoClassElement(element));
  }
  return Status::OK();
}

bool InfoClassManager::SamplingRequired() const { return CurrentTime() > NextSamplingTime(); }

bool InfoClassManager::PushRequired() const {
  if (CurrentTime() > NextPushTime()) {
    return true;
  }

  if (data_table_->OccupancyPct() > occupancy_pct_threshold_) {
    return true;
  }

  if (data_table_->Occupancy() > occupancy_threshold_) {
    return true;
  }

  return false;
}

Status InfoClassManager::SampleData() {
  source_->TransferData(data_table_->GetActiveRecordBatch());

  // Update the last sampling time.
  last_sampled_ = CurrentTime();

  sampling_count_++;

  return Status::OK();
}

Status InfoClassManager::PushData(PushDataCallback agent_callback) {
  PL_ASSIGN_OR_RETURN(auto record_batches, data_table_->GetRecordBatches());
  for (auto& record_batch : *record_batches) {
    if (!record_batch->empty()) {
      agent_callback(id(), std::move(record_batch));
    }
  }

  // Update the last pushed time.
  last_pushed_ = CurrentTime();

  push_count_++;

  return Status::OK();
}

stirlingpb::InfoClass InfoClassManager::ToProto() const {
  stirlingpb::InfoClass info_class_proto;
  // Populate the proto with Elements.
  for (auto element : elements_) {
    stirlingpb::Element* element_proto_ptr = info_class_proto.add_elements();
    element_proto_ptr->MergeFrom(element.ToProto());
  }

  // Add all the other fields for the proto.
  info_class_proto.set_name(name_);
  info_class_proto.set_id(id_);
  info_class_proto.set_subscribed(subscribed_);
  info_class_proto.set_sampling_period_millis(sampling_period_.count());
  info_class_proto.set_push_period_millis(push_period_.count());

  return info_class_proto;
}

}  // namespace stirling
}  // namespace pl
