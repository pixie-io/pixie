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
  element_proto.set_state(state_);
  element_proto.set_type(type_);
  return element_proto;
}

bool InfoClassManager::SamplingRequired() const {
  if (CurrentTime() > NextSamplingTime()) {
    return true;
  }

  return false;
}

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
  // Get data from the Source.
  // Source manages its own buffer as appropriate.
  // The complexity of re-using same memory buffer then falls to the Data Source.
  auto data = source_->GetData();
  auto num_records = data.num_records;
  auto* data_buf = reinterpret_cast<uint8_t*>(data.buf);
  PL_CHECK_OK(data_table_->AppendData(data_buf, num_records));

  // Update the last sampling time.
  last_sampled_ = CurrentTime();

  sampling_count_++;

  return Status::OK();
}

Status InfoClassManager::PushData(PushDataCallback agent_callback) {
  PL_ASSIGN_OR_RETURN(auto record_batches, data_table_->GetColumnWrapperRecordBatches());
  for (auto& record_batch : *record_batches) {
    if (record_batch->size() > 0) {
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

  // Add metadata.
  // TODO(kgandhi): For M2, only add the source name. Later on add other information
  // from the SourceConnector.
  auto metadata_map = info_class_proto.mutable_metadata();
  std::string key = "source";
  std::string value = source_->source_name();
  (*metadata_map)[key] = value;

  // Add all the other fields for the proto.
  info_class_proto.set_name(name_);
  info_class_proto.set_id(id_);
  info_class_proto.set_subscribed(subscribed_);

  return info_class_proto;
}

}  // namespace stirling
}  // namespace pl
