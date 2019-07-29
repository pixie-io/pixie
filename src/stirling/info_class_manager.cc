#include <memory>
#include <utility>
#include <vector>

#include "src/common/base/base.h"

#include "src/stirling/data_table.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

stirlingpb::Element InfoClassElement::ToProto() const {
  stirlingpb::Element element_proto;
  element_proto.set_name(name_.data());
  element_proto.set_type(type_);
  element_proto.set_ptype(ptype_);
  return element_proto;
}

Status InfoClassManager::PopulateSchemaFromSource() {
  if (source_ == nullptr) {
    return error::ResourceUnavailable("Source connector has not been initialized.");
  }
  auto elements = source_->elements(source_table_num_);
  for (const auto& element : elements) {
    elements_.emplace_back(InfoClassElement(element));
  }
  return Status::OK();
}

bool InfoClassManager::SamplingRequired() const { return CurrentTime() > NextSamplingTime(); }

bool InfoClassManager::PushRequired() const {
  // Note: It's okay to exercise an early Push, by returning true before the final return,
  // but it is not okay to 'return false' in this function.

  if (data_table_->OccupancyPct() > occupancy_pct_threshold_) {
    return true;
  }

  if (data_table_->Occupancy() > occupancy_threshold_) {
    return true;
  }

  return CurrentTime() > NextPushTime();
}

void InfoClassManager::SampleData() {
  source_->TransferData(source_table_num_, data_table_);

  // Update the last sampling time.
  last_sampled_ = CurrentTime();

  sampling_count_++;
}

void InfoClassManager::PushData(PushDataCallback agent_callback) {
  auto record_batches = data_table_->ConsumeRecordBatches();
  for (auto& record_batch : *record_batches) {
    if (!record_batch->empty()) {
      agent_callback(id(), std::move(record_batch));
    }
  }

  // Update the last pushed time.
  last_pushed_ = CurrentTime();

  push_count_++;
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
