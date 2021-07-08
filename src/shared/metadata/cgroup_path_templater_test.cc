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
#include "src/shared/metadata/cgroup_path_templater.h"

namespace px {
namespace md {

constexpr std::string_view kPodID = "pod01234567-cccc-dddd-eeee-ffff000011112222";
constexpr std::string_view kContainerID =
    "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62";

TEST(CGroupPathTemplater, GKEFormat) {
  std::string cgroup_kubepod_path =
      "/sys/fs/cgroup/cpu,cpuacct/kubepods/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/"
      "14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs";
  ASSERT_OK_AND_ASSIGN(CGroupTemplateSpec spec,
                       CreateCGroupTemplateSpecFromPath(cgroup_kubepod_path));
  EXPECT_EQ(spec.templated_path, "/sys/fs/cgroup/cpu,cpuacct/kubepods/$2/$0/$1/cgroup.procs");
  EXPECT_EQ(spec.pod_id_separators.value_or('\0'), '-');
  EXPECT_EQ(spec.qos_separator, '/');

  CGroupTemplater cgroup_templater(spec);
  EXPECT_EQ(cgroup_templater.PodPath(PodQOSClass::kGuaranteed, kPodID, kContainerID),
            "/sys/fs/cgroup/cpu,cpuacct/kubepods/pod01234567-cccc-dddd-eeee-ffff000011112222/"
            "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
  EXPECT_EQ(
      cgroup_templater.PodPath(PodQOSClass::kBestEffort, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods/besteffort/pod01234567-cccc-dddd-eeee-ffff000011112222/"
      "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
  EXPECT_EQ(
      cgroup_templater.PodPath(PodQOSClass::kBurstable, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods/burstable/pod01234567-cccc-dddd-eeee-ffff000011112222/"
      "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
}

TEST(CGroupPathTemplater, StandardFormatDocker) {
  std::string cgroup_kubepod_path =
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2.slice/"
      "docker-14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d.scope/cgroup.procs";
  ASSERT_OK_AND_ASSIGN(CGroupTemplateSpec spec,
                       CreateCGroupTemplateSpecFromPath(cgroup_kubepod_path));
  EXPECT_EQ(spec.templated_path,
            "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-$2-$0.slice/docker-$1.scope/"
            "cgroup.procs");
  EXPECT_EQ(spec.pod_id_separators.value_or('\0'), '_');
  EXPECT_EQ(spec.qos_separator, '-');

  CGroupTemplater cgroup_templater(spec);
  EXPECT_EQ(
      cgroup_templater.PodPath(PodQOSClass::kGuaranteed, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "docker-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
  EXPECT_EQ(
      cgroup_templater.PodPath(PodQOSClass::kBestEffort, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-besteffort-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "docker-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
  EXPECT_EQ(
      cgroup_templater.PodPath(PodQOSClass::kBurstable, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-burstable-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "docker-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
}

TEST(CGroupPathTemplater, StandardFormatCRIO) {
  std::string cgroup_kubepod_path =
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2.slice/"
      "crio-14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d.scope/cgroup.procs";
  ASSERT_OK_AND_ASSIGN(CGroupTemplateSpec spec,
                       CreateCGroupTemplateSpecFromPath(cgroup_kubepod_path));
  EXPECT_EQ(
      spec.templated_path,
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-$2-$0.slice/crio-$1.scope/cgroup.procs");
  EXPECT_EQ(spec.pod_id_separators.value_or('\0'), '_');
  EXPECT_EQ(spec.qos_separator, '-');

  CGroupTemplater cgroup_templater(spec);
  EXPECT_EQ(
      cgroup_templater.PodPath(PodQOSClass::kGuaranteed, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "crio-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
  EXPECT_EQ(
      cgroup_templater.PodPath(PodQOSClass::kBestEffort, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-besteffort-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "crio-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
  EXPECT_EQ(
      cgroup_templater.PodPath(PodQOSClass::kBurstable, kPodID, kContainerID),
      "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/"
      "kubepods-burstable-pod01234567_cccc_dddd_eeee_ffff000011112222.slice/"
      "crio-a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62.scope/cgroup.procs");
}

TEST(CGroupPathTemplater, BareMetalK8s_1_21) {
  std::string cgroup_kubepod_path =
      "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
      "kubepods-besteffort-pod1544eb37_e4f7_49eb_8cc4_3d01c41be77b.slice:cri-containerd:"
      "8618d3540ce713dd59ed0549719643a71dd482c40c21685773e7ac1291b004f5/cgroup.procs";
  ASSERT_OK_AND_ASSIGN(CGroupTemplateSpec spec,
                       CreateCGroupTemplateSpecFromPath(cgroup_kubepod_path));
  EXPECT_EQ(spec.templated_path,
            "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
            "kubepods-$2-$0.slice:cri-containerd:$1/cgroup.procs");
  EXPECT_EQ(spec.pod_id_separators.value_or('\0'), '_');
  EXPECT_EQ(spec.qos_separator, '-');

  CGroupTemplater cgroup_templater(spec);
  EXPECT_EQ(cgroup_templater.PodPath(PodQOSClass::kGuaranteed, kPodID, kContainerID),
            "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
            "kubepods-pod01234567_cccc_dddd_eeee_ffff000011112222.slice:cri-containerd:"
            "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
  EXPECT_EQ(cgroup_templater.PodPath(PodQOSClass::kBestEffort, kPodID, kContainerID),
            "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
            "kubepods-besteffort-pod01234567_cccc_dddd_eeee_ffff000011112222.slice:cri-containerd:"
            "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
  EXPECT_EQ(cgroup_templater.PodPath(PodQOSClass::kBurstable, kPodID, kContainerID),
            "/sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/"
            "kubepods-burstable-pod01234567_cccc_dddd_eeee_ffff000011112222.slice:cri-containerd:"
            "a7638fe3934b37419cc56bca73465a02b354ba6e98e10272542d84eb2014dd62/cgroup.procs");
}

}  // namespace md
}  // namespace px
