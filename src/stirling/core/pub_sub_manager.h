#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/proto/wrapper/types_pb_wrapper.h"
#include "src/stirling/core/info_class_manager.h"
#include "src/stirling/proto/stirling.pb.h"

namespace pl {
namespace stirling {

class PubSubManager {
 public:
  PubSubManager() = default;
  ~PubSubManager() = default;

  /**
   * Create a proto message from InfoClassManagers (where each have a schema).
   *
   * @param publish_pb pointer to a Publish proto message.
   * @param info_class_mgrs Reference to a vector of info class manager unique pointers.
   * @param filter Generate a publish proto for a single info class, specified by name.
   */
  void PopulatePublishProto(stirlingpb::Publish* publish_pb,
                            const InfoClassManagerVec& info_class_mgrs,
                            std::optional<std::string_view> filter = {});

  /**
   * Update the ElementState for each InfoElement in the InfoClassManager
   * in info_class_mgrs_ from a subscription message and notify the data collector
   * about the update to schemas. The data collector can then proceed to configure
   * SourceConnectors and DataTable with the subscribed information.
   *
   * @param subscribe_proto
   * @param info_class_mgrs Reference to a vector of info class manager unique pointers.
   * @return Status
   */
  Status UpdateSchemaFromSubscribe(const stirlingpb::Subscribe& subscribe_proto,
                                   const InfoClassManagerVec& info_class_mgrs);
};

/**
 * Utility function to index a publish message by ID, for quick access.
 */
inline void IndexPublication(const stirlingpb::Publish& pub,
                             absl::flat_hash_map<uint64_t, stirlingpb::InfoClass>* map) {
  for (const auto& info_class : pub.published_info_classes()) {
    (*map)[info_class.id()] = info_class;
  }
}

/**
 * Convenience function to subscribe to all info classes of a published proto message.
 */
stirlingpb::Subscribe SubscribeToAllInfoClasses(const stirlingpb::Publish& publish_proto);

/**
 * Convenience function to subscribe to a single info classes of a published proto message.
 */
stirlingpb::Subscribe SubscribeToInfoClass(const stirlingpb::Publish& publish_proto,
                                           std::string_view name);

}  // namespace stirling
}  // namespace pl
