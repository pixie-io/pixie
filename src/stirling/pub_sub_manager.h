#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/common/status.h"
#include "src/shared/types/proto/types.pb.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/proto/collector_config.pb.h"

using pl::Status;

namespace pl {
namespace stirling {

class PubSubManager {
 public:
  PubSubManager() = default;
  ~PubSubManager() = default;

  /**
   * @brief Create a proto message from InfoClassManagers (where each have a schema).
   *
   * @param publish_pb pointer to a Publish proto message.
   * @param info_class_mgrs Reference to a vector of info class manager unique pointers.
   */
  void GeneratePublishProto(stirlingpb::Publish* publish_pb,
                            const InfoClassManagerVec& info_class_mgrs);

  /**
   * @brief Update the ElementState for each InfoElement in the InfoClassManager
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

}  // namespace stirling
}  // namespace pl
