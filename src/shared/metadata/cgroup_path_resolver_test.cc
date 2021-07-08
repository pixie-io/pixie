/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/shared/metadata/cgroup_path_resolver.h"

namespace px {
namespace md {

constexpr std::string_view kPodID = "01234567-cccc-dddd-eeee-ffff000011112222";
constexpr std::string_view kContainerID =
    "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62";

TEST(CGroupPathResolver, GKEFormat) {
  std::string cgroup_kubepod_path =
      "/sys/fs/cgroup/cpu,cpuacct/kubepods/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/"
      "14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs";
  ASSERT_OK_AND_ASSIGN(CGroupTemplateSpec spec,
                       CreateCGroupTemplateSpecFromPath(cgroup_kubepod_path));
  EXPECT_EQ(spec.templated_path, "/sys/fs/cgroup/cpu,cpuacct/kubepods/$2/pod$0/$1/cgroup.procs");
  EXPECT_EQ(spec.pod_id_separators.value_or('\0'), '-');
  EXPECT_EQ(spec.qos_separator, '/');

  CGroupPathResolver path_resolver(spec);
  EXPECT_EQ(path_resolver.PodPath(PodQOSClass::kGuaranteed, kPodID, kContainerID),
            "/sys/fs/cgroup/cpu,cpuacct/kubepods/pod01234567-cccc-dddd-eeee-ffff000011112222/"
            "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
  EXPECT_EQ(
      path_resolver.PodPath(PodQOSClass::kBestEffort, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods/besteffort/pod01234567-cccc-dddd-eeee-ffff000011112222/"
      "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
  EXPECT_EQ(
      path_resolver.PodPath(PodQOSClass::kBurstable, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods/burstable/pod01234567-cccc-dddd-eeee-ffff000011112222/"
      "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
}

TEST(CGroupPathResolver, StandardFormatDocker) {
  std::string cgroup_kubepod_path =
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2.slice/"
      "docker-14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d.scope/cgroup.procs";
  ASSERT_OK_AND_ASSIGN(CGroupTemplateSpec spec,
                       CreateCGroupTemplateSpecFromPath(cgroup_kubepod_path));
  EXPECT_EQ(spec.templated_path,
            "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-$2-pod$0.slice/docker-$1.scope/"
            "cgroup.procs");
  EXPECT_EQ(spec.pod_id_separators.value_or('\0'), '_');
  EXPECT_EQ(spec.qos_separator, '-');

  CGroupPathResolver path_resolver(spec);
  EXPECT_EQ(
      path_resolver.PodPath(PodQOSClass::kGuaranteed, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "docker-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
  EXPECT_EQ(
      path_resolver.PodPath(PodQOSClass::kBestEffort, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-besteffort-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "docker-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
  EXPECT_EQ(
      path_resolver.PodPath(PodQOSClass::kBurstable, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-burstable-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "docker-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
}

TEST(CGroupPathResolver, StandardFormatCRIO) {
  std::string cgroup_kubepod_path =
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2.slice/"
      "crio-14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d.scope/cgroup.procs";
  ASSERT_OK_AND_ASSIGN(CGroupTemplateSpec spec,
                       CreateCGroupTemplateSpecFromPath(cgroup_kubepod_path));
  EXPECT_EQ(spec.templated_path,
            "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-$2-pod$0.slice/crio-$1.scope/"
            "cgroup.procs");
  EXPECT_EQ(spec.pod_id_separators.value_or('\0'), '_');
  EXPECT_EQ(spec.qos_separator, '-');

  CGroupPathResolver path_resolver(spec);
  EXPECT_EQ(
      path_resolver.PodPath(PodQOSClass::kGuaranteed, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "crio-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
  EXPECT_EQ(
      path_resolver.PodPath(PodQOSClass::kBestEffort, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-besteffort-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "crio-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
  EXPECT_EQ(
      path_resolver.PodPath(PodQOSClass::kBurstable, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-burstable-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "crio-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
}

TEST(CGroupPathResolver, BareMetalK8s_1_21) {
  std::string cgroup_kubepod_path =
      "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
      "kubepods-besteffort-pod1544eb37_e4f7_49eb_8cc4_3d01c41be77b.slice:cri-containerd:"
      "8618d3540ce713dd59ed0549719643a71dd482c40c21685773e7ac1291b004f5/cgroup.procs";
  ASSERT_OK_AND_ASSIGN(CGroupTemplateSpec spec,
                       CreateCGroupTemplateSpecFromPath(cgroup_kubepod_path));
  EXPECT_EQ(spec.templated_path,
            "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
            "kubepods-$2-pod$0.slice:cri-containerd:$1/cgroup.procs");
  EXPECT_EQ(spec.pod_id_separators.value_or('\0'), '_');
  EXPECT_EQ(spec.qos_separator, '-');

  CGroupPathResolver path_resolver(spec);
  EXPECT_EQ(path_resolver.PodPath(PodQOSClass::kGuaranteed, kPodID, kContainerID),
            "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
            "kubepods-pod01234567_cccc_dddd_eeee_ffff000011112222.slice:cri-containerd:"
            "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
  EXPECT_EQ(path_resolver.PodPath(PodQOSClass::kBestEffort, kPodID, kContainerID),
            "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
            "kubepods-besteffort-pod01234567_cccc_dddd_eeee_ffff000011112222.slice:cri-containerd:"
            "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
  EXPECT_EQ(path_resolver.PodPath(PodQOSClass::kBurstable, kPodID, kContainerID),
            "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
            "kubepods-burstable-pod01234567_cccc_dddd_eeee_ffff000011112222.slice:cri-containerd:"
            "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
}

namespace {
constexpr char kTestDataBasePath[] = "src/shared/metadata";

std::string GetPathToTestDataFile(const std::string& fname) {
  return testing::TestFilePath(std::string(kTestDataBasePath) + "/" + fname);
}
}  // namespace

TEST(LegacyCGroupPathResolverTest, GKEFormat) {
  ASSERT_OK_AND_ASSIGN(auto path_resolver,
                       LegacyCGroupPathResolver::Create(GetPathToTestDataFile("testdata/sysfs1")));

  EXPECT_EQ(path_resolver->PodPath(PodQOSClass::kBurstable, "abcd", "c123", ContainerType::kDocker),
            GetPathToTestDataFile(
                "testdata/sysfs1/cgroup/cpu,cpuacct/kubepods/burstable/podabcd/c123/cgroup.procs"));

  EXPECT_EQ(
      path_resolver->PodPath(PodQOSClass::kBestEffort, "abcd", "c123", ContainerType::kDocker),
      GetPathToTestDataFile(
          "testdata/sysfs1/cgroup/cpu,cpuacct/kubepods/besteffort/podabcd/c123/cgroup.procs"));

  EXPECT_EQ(
      path_resolver->PodPath(PodQOSClass::kGuaranteed, "abcd", "c123", ContainerType::kDocker),
      GetPathToTestDataFile(
          "testdata/sysfs1/cgroup/cpu,cpuacct/kubepods/podabcd/c123/cgroup.procs"));
}

TEST(LegacyCGroupPathResolverTest, StandardFormat) {
  ASSERT_OK_AND_ASSIGN(auto path_resolver,
                       LegacyCGroupPathResolver::Create(GetPathToTestDataFile("testdata/sysfs2")));

  EXPECT_EQ(
      GetPathToTestDataFile(
          "testdata/sysfs2/cgroup/cpu,cpuacct/kubepods.slice/kubepods-burstable.slice/"
          "kubepods-burstable-pod5a1d1140_a486_478c_afae_bbc975ff9c3b.slice/"
          "docker-2b41fe4bb7a365960f1e7ed6c09651252b29387b44c9e14ad17e3bc392e7c640.scope/"
          "cgroup.procs"),
      path_resolver->PodPath(PodQOSClass::kBurstable, "5a1d1140-a486-478c-afae-bbc975ff9c3b",
                             "2b41fe4bb7a365960f1e7ed6c09651252b29387b44c9e14ad17e3bc392e7c640",
                             ContainerType::kDocker));

  EXPECT_EQ(
      GetPathToTestDataFile(
          "testdata/sysfs2/cgroup/cpu,cpuacct/kubepods.slice/kubepods-besteffort.slice/"
          "kubepods-besteffort-pod15b6301f_94d0_44ac_a2a8_6816c7a3fa32.slice/"
          "docker-159757ef9efdc09be13490c8615f1402c170cdd406dad6053ebe0df2db89fcaa.scope/"
          "cgroup.procs"),
      path_resolver->PodPath(PodQOSClass::kBestEffort, "15b6301f-94d0-44ac-a2a8-6816c7a3fa32",
                             "159757ef9efdc09be13490c8615f1402c170cdd406dad6053ebe0df2db89fcaa",
                             ContainerType::kDocker));

  EXPECT_EQ(
      GetPathToTestDataFile(
          "testdata/sysfs2/cgroup/cpu,cpuacct/kubepods.slice/"
          "kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2.slice/"
          "docker-b9055cee13e1f37ecb63030593b27f4adc43cbd6629aa7781ffdf53fbaecfa46.scope/"
          "cgroup.procs"),
      path_resolver->PodPath(PodQOSClass::kGuaranteed, "8dbc5577-d0e2-4706-8787-57d52c03ddf2",
                             "b9055cee13e1f37ecb63030593b27f4adc43cbd6629aa7781ffdf53fbaecfa46",
                             ContainerType::kDocker));

  EXPECT_EQ(
      GetPathToTestDataFile(
          "testdata/sysfs2/cgroup/cpu,cpuacct/kubepods.slice/kubepods-burstable.slice/"
          "kubepods-burstable-pod5a1d1140_a486_478c_afae_bbc975ff9c3b.slice/"
          "docker-2b41fe4bb7a365960f1e7ed6c09651252b29387b44c9e14ad17e3bc392e7c640.scope/"
          "cgroup.procs"),
      path_resolver->PodPath(PodQOSClass::kBurstable, "5a1d1140-a486-478c-afae-bbc975ff9c3b",
                             "2b41fe4bb7a365960f1e7ed6c09651252b29387b44c9e14ad17e3bc392e7c640",
                             ContainerType::kUnknown));

  EXPECT_EQ(
      GetPathToTestDataFile(
          "testdata/sysfs2/cgroup/cpu,cpuacct/kubepods.slice/kubepods-burstable.slice/"
          "kubepods-burstable-pod5a1d1140_a486_478c_afae_bbc975ff9c3b.slice/"
          "crio-2b41fe4bb7a365960f1e7ed6c09651252b29387b44c9e14ad17e3bc392e7c640.scope/"
          "cgroup.procs"),
      path_resolver->PodPath(PodQOSClass::kBurstable, "5a1d1140-a486-478c-afae-bbc975ff9c3b",
                             "2b41fe4bb7a365960f1e7ed6c09651252b29387b44c9e14ad17e3bc392e7c640",
                             ContainerType::kCRIO));

  EXPECT_EQ(
      GetPathToTestDataFile(
          "testdata/sysfs2/cgroup/cpu,cpuacct/kubepods.slice/kubepods-burstable.slice/"
          "kubepods-burstable-pod5a1d1140_a486_478c_afae_bbc975ff9c3b.slice/"
          "cri-containerd-2b41fe4bb7a365960f1e7ed6c09651252b29387b44c9e14ad17e3bc392e7c640.scope/"
          "cgroup.procs"),
      path_resolver->PodPath(PodQOSClass::kBurstable, "5a1d1140-a486-478c-afae-bbc975ff9c3b",
                             "2b41fe4bb7a365960f1e7ed6c09651252b29387b44c9e14ad17e3bc392e7c640",
                             ContainerType::kContainerd));
}

}  // namespace md
}  // namespace px
