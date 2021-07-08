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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <absl/container/flat_hash_set.h>
#include "src/common/system/config_mock.h"
#include "src/common/testing/testing.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"

namespace px {
namespace md {

namespace {
constexpr char kTestDataBasePath[] = "src/shared/metadata";

std::string GetPathToTestDataFile(const std::string& fname) {
  return testing::TestFilePath(std::string(kTestDataBasePath) + "/" + fname);
}
}  // namespace

class CGroupMetadataReaderTest : public ::testing::Test {
 protected:
  void SetupMockConfig(system::MockConfig* sysconfig, const std::string& proc_path,
                       const std::string& sysfs_path) {
    using ::testing::Return;
    using ::testing::ReturnRef;

    proc_path_ = GetPathToTestDataFile(proc_path);
    sysfs_path_ = GetPathToTestDataFile(sysfs_path);

    EXPECT_CALL(*sysconfig, HasConfig()).WillRepeatedly(Return(true));
    EXPECT_CALL(*sysconfig, PageSize()).WillRepeatedly(Return(4096));
    EXPECT_CALL(*sysconfig, KernelTicksPerSecond()).WillRepeatedly(Return(10000000));
    EXPECT_CALL(*sysconfig, ClockRealTimeOffset()).WillRepeatedly(Return(128));

    EXPECT_CALL(*sysconfig, proc_path()).WillRepeatedly(ReturnRef(proc_path_));
    EXPECT_CALL(*sysconfig, sysfs_path()).WillRepeatedly(ReturnRef(sysfs_path_));
  }

  void SetUp() override {
    system::MockConfig sysconfig;
    SetupMockConfig(&sysconfig, "testdata/proc1", "testdata/sysfs1");
    md_reader_ = std::make_unique<CGroupMetadataReader>(sysconfig);
  }

  std::unique_ptr<CGroupMetadataReader> md_reader_;
  std::filesystem::path proc_path_;
  std::filesystem::path sysfs_path_;
};

TEST_F(CGroupMetadataReaderTest, read_pid_list) {
  absl::flat_hash_set<uint32_t> pid_set;
  ASSERT_OK(md_reader_->ReadPIDs(PodQOSClass::kBestEffort, "abcd", "c123", ContainerType::kDocker,
                                 &pid_set));
  EXPECT_THAT(pid_set, ::testing::UnorderedElementsAre(123, 456, 789));
}

}  // namespace md
}  // namespace px
