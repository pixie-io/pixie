#include <memory>
#include <utility>
#include <vector>

#include "src/common/macros.h"

#include "src/stirling/data_table.h"
#include "src/stirling/info_class_schema.h"
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

bool InfoClassManager::SamplingRequired() const { return (CurrentTime() > NextSamplingTime()); }

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

RawDataBuf InfoClassManager::GetData() {
  sampling_count_++;
  last_sampled_ = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());

  return source_->GetData();
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

  return info_class_proto;
}

}  // namespace stirling
}  // namespace pl
