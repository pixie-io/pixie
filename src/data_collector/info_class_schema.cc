#include "src/data_collector/info_class_schema.h"

namespace pl {
namespace datacollector {

std::atomic<uint64_t> InfoClassSchema::global_id_ = 0;

datacollectorpb::Element InfoClassElement::ToProto() const {
  datacollectorpb::Element element_proto;
  element_proto.set_name(name_);
  element_proto.set_state(state_);
  element_proto.set_type(data_type_);
  return element_proto;
}

datacollectorpb::InfoClass InfoClassSchema::ToProto() const {
  datacollectorpb::InfoClass info_class_proto;
  // Populate the proto with Elements.
  for (auto element : elements_) {
    datacollectorpb::Element* element_proto_ptr = info_class_proto.add_elements();
    element_proto_ptr->MergeFrom(element.ToProto());
  }

  // Add metadata.
  // TODO(kgandhi): For M2, only add the source name. Later on add other information
  // from the SourceConnector.
  auto metadata_map = info_class_proto.mutable_metadata();
  std::string key = "source";
  std::string value = source_.name();
  (*metadata_map)[key] = value;

  // Add all the other fields for the proto.
  info_class_proto.set_name(name_);
  info_class_proto.set_id(id_);

  return info_class_proto;
}

}  // namespace datacollector
}  // namespace pl
