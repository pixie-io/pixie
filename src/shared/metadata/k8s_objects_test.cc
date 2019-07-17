#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/shared/metadata/k8s_objects.h"

namespace pl {
namespace md {

TEST(PodInfo, basic_accessors) {
  PodInfo pod_info("123", "pl", "pod1");
  pod_info.set_start_time_ns(123);
  pod_info.set_stop_time_ns(256);

  EXPECT_EQ("123", pod_info.uid());
  EXPECT_EQ("pl", pod_info.ns());
  EXPECT_EQ("pod1", pod_info.name());

  EXPECT_EQ(123, pod_info.start_time_ns());
  EXPECT_EQ(256, pod_info.stop_time_ns());

  EXPECT_EQ(K8sObjectType::kPod, pod_info.type());
}

TEST(PodInfo, add_delete_containers) {
  PodInfo pod_info("123", "pl", "pod1");
  pod_info.AddContainer("ABCD");
  pod_info.AddContainer("ABCD2");
  pod_info.AddContainer("ABCD3");
  pod_info.RmContainer("ABCD");

  EXPECT_THAT(pod_info.containers(), testing::UnorderedElementsAre("ABCD2", "ABCD3"));

  pod_info.RmContainer("ABCD3");
  EXPECT_THAT(pod_info.containers(), testing::UnorderedElementsAre("ABCD2"));
}

TEST(PodInfo, clone) {
  PodInfo pod_info("123", "pl", "pod1");
  pod_info.set_start_time_ns(123);
  pod_info.set_stop_time_ns(256);
  pod_info.AddContainer("ABCD");
  pod_info.AddContainer("ABCD2");

  std::unique_ptr<PodInfo> cloned(static_cast<PodInfo*>(pod_info.Clone().release()));
  EXPECT_EQ(cloned->uid(), pod_info.uid());
  EXPECT_EQ(cloned->name(), pod_info.name());

  EXPECT_EQ(cloned->start_time_ns(), pod_info.start_time_ns());
  EXPECT_EQ(cloned->stop_time_ns(), pod_info.stop_time_ns());

  EXPECT_EQ(cloned->type(), pod_info.type());
  EXPECT_EQ(cloned->containers(), pod_info.containers());
}

TEST(ContainerInfo, pod_id) {
  ContainerInfo cinfo("container1");

  EXPECT_EQ("", cinfo.pod_id());
  cinfo.set_pod_id("pod1");
  EXPECT_EQ("pod1", cinfo.pod_id());
}

TEST(ContainerInfo, add_delete_pids) {
  ContainerInfo cinfo("container1");
  cinfo.set_pod_id("pod1");

  cinfo.AddPID(1);
  cinfo.AddPID(2);
  cinfo.AddPID(2);
  cinfo.AddPID(5);
  cinfo.RmPID(3);

  EXPECT_THAT(cinfo.pids(), testing::UnorderedElementsAre(1, 2, 5));

  cinfo.RmPID(2);
  EXPECT_THAT(cinfo.pids(), testing::UnorderedElementsAre(1, 5));
}

TEST(ContainerInfo, clone) {
  ContainerInfo orig("container1");
  orig.set_pod_id("pod1");

  orig.AddPID(0);
  orig.AddPID(1);
  orig.AddPID(15);

  orig.set_start_time_ns(128);
  orig.set_start_time_ns(256);

  auto cloned = orig.Clone();

  EXPECT_EQ(cloned->pod_id(), orig.pod_id());
  EXPECT_EQ(cloned->cid(), orig.cid());
  EXPECT_EQ(cloned->pids(), orig.pids());
  EXPECT_EQ(cloned->start_time_ns(), orig.start_time_ns());
  EXPECT_EQ(cloned->stop_time_ns(), orig.stop_time_ns());
}

}  // namespace md
}  // namespace pl
