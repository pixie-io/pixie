#pragma once

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/substitute.h"
#include "src/common/base/base.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"

namespace pl {
namespace metadatapb {
namespace testutils {
/*
 * Template for a resource update.
 */
const char* kResourceUpdateTmpl = R"(
$0 {
  $1
}
)";

/*
 *  Templates for pod updates.
 */
const char* kRunningPodUpdatePbTxt = R"(
uid: "1_uid"
name: "running_pod"
namespace: "pl"
start_timestamp_ns: 5
container_ids: "pod1_container_1"
qos_class: QOS_CLASS_GUARANTEED
)";

const char* kToBeTerminatedPodUpdatePbTxt = R"(
uid: "2_uid"
name: "terminating_pod"
namespace: "pl"
start_timestamp_ns: 10
container_ids: "pod2_container_1"
qos_class: QOS_CLASS_BEST_EFFORT
)";

const char* kTerminatedPodUpdatePbTxt = R"(
uid: "2_uid"
name: "terminating_pod"
namespace: "pl"
start_timestamp_ns: 10
stop_timestamp_ns: 15
container_ids: "pod2_container_1"
qos_class: QOS_CLASS_BEST_EFFORT
)";

/*
 * Templates for container updates.
 */
const char* kRunningContainerUpdatePbTxt = R"(
cid: "pod1_container_1"
name: "running_container"
start_timestamp_ns: 6
)";

const char* kTerminatingContainerUpdatePbTxt = R"(
cid: "pod2_container_1"
name: "terminating_container"
start_timestamp_ns: 7
)";

const char* kTerminatedContainerUpdatePbTxt = R"(
cid: "pod2_container_1"
name: "terminating_container"
start_timestamp_ns: 7
stop_timestamp_ns: 14
)";

std::unique_ptr<pl::shared::k8s::metadatapb::ResourceUpdate> CreateRunningPodUpdatePB() {
  auto update = std::make_unique<pl::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto = absl::Substitute(kResourceUpdateTmpl, "pod_update", kRunningPodUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<pl::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatingPodUpdatePB() {
  auto update = std::make_unique<pl::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "pod_update", kToBeTerminatedPodUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<pl::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatedPodUpdatePB() {
  auto update = std::make_unique<pl::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "pod_update", kTerminatedContainerUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<pl::shared::k8s::metadatapb::ResourceUpdate> CreateRunningContainerUpdatePB() {
  auto update = std::make_unique<pl::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "container_update", kRunningContainerUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<pl::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatingContainerUpdatePB() {
  auto update = std::make_unique<pl::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "container_update", kTerminatingContainerUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

std::unique_ptr<pl::shared::k8s::metadatapb::ResourceUpdate> CreateTerminatedContainerUpdatePB() {
  auto update = std::make_unique<pl::shared::k8s::metadatapb::ResourceUpdate>();
  auto update_proto =
      absl::Substitute(kResourceUpdateTmpl, "container_update", kTerminatedContainerUpdatePbTxt);
  CHECK(google::protobuf::TextFormat::MergeFromString(update_proto, update.get()))
      << "Failed to parse proto";
  return update;
}

}  // namespace testutils
}  // namespace metadatapb
}  // namespace pl
