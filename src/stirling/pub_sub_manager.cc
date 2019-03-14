#include <algorithm>

#include "src/common/error.h"
#include "src/stirling/pub_sub_manager.h"

namespace pl {
namespace stirling {

using stirlingpb::InfoClass;
using stirlingpb::Publish;
using stirlingpb::Subscribe;

void PubSubManager::GeneratePublishProto(Publish* publish_pb,
                                         const InfoClassManagerVec& info_class_mgrs) {
  CHECK(publish_pb != nullptr);
  // For each InfoClassManager get its proto and update publish_message.
  for (auto& schema : info_class_mgrs) {
    InfoClass* info_class_proto = publish_pb->add_published_info_classes();
    info_class_proto->MergeFrom(schema->ToProto());
  }
}

Status PubSubManager::UpdateSchemaFromSubscribe(const Subscribe& subscribe_proto,
                                                const InfoClassManagerVec& info_class_mgrs) {
  int num_info_classes = subscribe_proto.subscribed_info_classes_size();
  for (int info_class_idx = 0; info_class_idx < num_info_classes; ++info_class_idx) {
    auto info_class_proto = subscribe_proto.subscribed_info_classes(info_class_idx);
    uint64_t id = info_class_proto.id();

    auto it = std::find_if(info_class_mgrs.begin(), info_class_mgrs.end(),
                           [&id](const std::unique_ptr<InfoClassManager>& info_class_ptr) {
                             return info_class_ptr->id() == id;
                           });

    // Check that the InfoClass exists in the map.
    if (it == info_class_mgrs.end()) {
      return Status(pl::error::NOT_FOUND, "Info Class Schema not found in Config map");
    }

    // Check that the number or elements are the same between the proto
    // and the InfoClassManager object.

    size_t num_elements = info_class_proto.elements_size();
    if (num_elements != (*it)->Schema().size()) {
      return Status(pl::error::INTERNAL, "Number of elements in InfoClassManager does not match");
    }

    (*it)->SetSubscription(info_class_proto.subscribed());
    (*it)->SetSamplingPeriod(std::chrono::milliseconds{info_class_proto.sampling_period_millis()});
    (*it)->SetPushPeriod(std::chrono::milliseconds{info_class_proto.push_period_millis()});
  }
  return Status::OK();
}

}  // namespace stirling
}  // namespace pl
