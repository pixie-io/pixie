#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/metadata/state_manager.h"

namespace pl {
namespace md {

using pl::shared::k8s::metadatapb::MetadataResourceType;
using ResourceUpdate = pl::shared::k8s::metadatapb::ResourceUpdate2;

using testing::Pair;
using testing::UnorderedElementsAre;

constexpr char kUpdate1_0Pbtxt[] = R"(
  container_update {
    name: "container_name1"
    cid: "container_id1"
    start_timestamp_ns: 1001
  }
)";

constexpr char kUpdate1_1Pbtxt[] = R"(
  pod_update {
    name: "pod1"
    namespace: "pl"
    uid: "pod_id1"
    start_timestamp_ns: 1000
    container_ids: "container_id1"
    qos_class: QOS_CLASS_BURSTABLE
  }
)";

TEST(ApplyK8sUpdate, initialize_md_state) {
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> updates;
  AgentMetadataState metadata_state(123 /*agent_id*/);

  auto update1_0 = std::make_unique<ResourceUpdate>();
  auto update1_1 = std::make_unique<ResourceUpdate>();
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUpdate1_0Pbtxt, update1_0.get()));
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUpdate1_1Pbtxt, update1_1.get()));
  updates.enqueue(std::move(update1_0));
  updates.enqueue(std::move(update1_1));

  EXPECT_OK(AgentMetadataStateManager::ApplyK8sUpdates(2000 /*ts*/, &metadata_state, &updates));
  EXPECT_EQ(0, updates.size_approx());

  EXPECT_EQ(123, metadata_state.agent_id());

  K8sMetadataState* state = metadata_state.k8s_metadata_state();
  EXPECT_THAT(state->pods_by_name(), UnorderedElementsAre(Pair(Pair("pl", "pod1"), "pod_id1")));
  EXPECT_EQ("pod_id1", state->PodIDByName({"pl", "pod1"}));

  auto* pod_info = state->PodInfoByID("pod_id1");
  ASSERT_NE(nullptr, pod_info);
  EXPECT_EQ(1000, pod_info->start_time_ns());
  EXPECT_EQ("pod_id1", pod_info->uid());
  EXPECT_EQ("pod1", pod_info->name());
  EXPECT_EQ("pl", pod_info->ns());
  EXPECT_THAT(pod_info->containers(), UnorderedElementsAre("container_id1"));

  auto* container_info = state->ContainerInfoByID("container_id1");
  ASSERT_NE(nullptr, container_info);
  EXPECT_EQ("container_id1", container_info->cid());
  EXPECT_EQ("pod_id1", container_info->pod_id());
}

}  // namespace md
}  // namespace pl
