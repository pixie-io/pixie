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

#include <fstream>

#include "src/common/system/config.h"
#include "src/common/system/kernel_version.h"
#include "src/common/testing/testing.h"

DECLARE_string(proc_path);

namespace px {
namespace system {

namespace {
std::string GetPathToTestDataFile(const std::string_view fname) {
  constexpr char kTestDataBasePath[] = "src/common/system/testdata";
  return testing::BazelRunfilePath(std::filesystem::path(kTestDataBasePath) / fname);
}
}  // namespace

using ::px::testing::TempDir;
using ::testing::EndsWith;

// A list of kernel version identification methods that use /proc only.
// Used in a number of tests below.
const std::vector<KernelVersionSource> kProcFSKernelVersionSources = {
    KernelVersionSource::kProcVersionSignature, KernelVersionSource::kProcSysKernelVersion};

TEST(LinuxHeadersUtils, LinuxVersionCode) {
  KernelVersion version{4, 18, 1};
  EXPECT_EQ(version.code(), 266753);
}

TEST(LinuxHeadersUtils, ParseKernelVersionString) {
  EXPECT_OK_AND_EQ(ParseKernelVersionString("4.18.0-25-generic"), (KernelVersion{4, 18, 0}));
  EXPECT_OK_AND_EQ(ParseKernelVersionString("4.18.4"), (KernelVersion{4, 18, 4}));
  EXPECT_NOT_OK(ParseKernelVersionString("4.18."));
  EXPECT_NOT_OK(ParseKernelVersionString("linux-4.18.0-25-generic"));
}

TEST(LinuxHeadersUtils, GetKernelVersionFromUname) {
  StatusOr<KernelVersion> kernel_version_status = FindKernelVersion({KernelVersionSource::kUname});
  ASSERT_OK(kernel_version_status);
  KernelVersion kernel_version = kernel_version_status.ValueOrDie();

  // We don't know on what host this test will run, so we don't know what the version code will be.
  // But we can put some bounds, to check for obvious screw-ups.
  // We assume test will run on a Linux machine with kernel 3.x.x or higher,
  // and version 9.x.x or lower.
  // Yes, we're being very generous here, but we want this test to pass the test of time.
  EXPECT_GE(kernel_version.code(), 0x030000);
  EXPECT_LE(kernel_version.code(), 0x090000);
}

TEST(LinuxHeadersUtils, GetKernelVersionFromNoteSection) {
  StatusOr<KernelVersion> kernel_version_status =
      FindKernelVersion({KernelVersionSource::kVDSONoteSection});
  ASSERT_OK(kernel_version_status);
  KernelVersion kernel_version = kernel_version_status.ValueOrDie();

  // We don't know on what host this test will run, so we don't know what the version code will be.
  // But we can put some bounds, to check for obvious screw-ups.
  // We assume test will run on a Linux machine with kernel 3.x.x or higher,
  // and version 9.x.x or lower.
  // Yes, we're being very generous here, but we want this test to pass the test of time.
  EXPECT_GE(kernel_version.code(), 0x030000);
  EXPECT_LE(kernel_version.code(), 0x090000);
}

TEST(LinuxHeadersUtils, GetKernelVersionUbuntu) {
  // Setup: Point to a custom /proc filesystem.
  PX_SET_FOR_SCOPE(FLAGS_proc_path, GetPathToTestDataFile("sample_host_ubuntu/proc"));

  // Main test.
  StatusOr<KernelVersion> kernel_version_status = FindKernelVersion(kProcFSKernelVersionSources);
  ASSERT_OK(kernel_version_status);
  KernelVersion kernel_version = kernel_version_status.ValueOrDie();
  EXPECT_EQ(kernel_version.code(), 0x050441);
}

TEST(LinuxHeadersUtils, GetKernelVersionDebian) {
  // Setup: Point to a custom /proc filesystem.
  PX_SET_FOR_SCOPE(FLAGS_proc_path, GetPathToTestDataFile("sample_host_debian/proc"));

  // Main test.
  StatusOr<KernelVersion> kernel_version_status = FindKernelVersion(kProcFSKernelVersionSources);
  ASSERT_OK(kernel_version_status);
  KernelVersion kernel_version = kernel_version_status.ValueOrDie();
  EXPECT_EQ(kernel_version.code(), 0x041398);
}

TEST(LinuxHeadersUtils, KernelHeadersDistance) {
  // Distances should always be positive.
  EXPECT_GT(KernelHeadersDistance({4, 14, 255}, {4, 15, 10}), 0);

  // Order shouldn't matter.
  EXPECT_EQ(KernelHeadersDistance({4, 14, 255}, {4, 15, 255}),
            KernelHeadersDistance({4, 15, 255}, {4, 14, 255}));

  EXPECT_GT(KernelHeadersDistance({4, 14, 1}, {4, 15, 5}),
            KernelHeadersDistance({4, 14, 1}, {4, 14, 2}));

  // A change in the major version is considered larger than any change in the minor version.
  EXPECT_GT(KernelHeadersDistance({4, 14, 255}, {4, 15, 0}),
            KernelHeadersDistance({4, 14, 255}, {4, 14, 0}));
}

TEST(LinuxHeadersUtils, CompareKernelVersions) {
  EXPECT_EQ(CompareKernelVersions({1, 2, 3}, {1, 2, 3}), KernelVersionOrder::kSame);

  EXPECT_EQ(CompareKernelVersions({1, 2, 3}, {1, 2, 4}), KernelVersionOrder::kOlder);
  EXPECT_EQ(CompareKernelVersions({1, 2, 3}, {1, 3, 3}), KernelVersionOrder::kOlder);
  EXPECT_EQ(CompareKernelVersions({1, 2, 3}, {2, 2, 3}), KernelVersionOrder::kOlder);

  EXPECT_EQ(CompareKernelVersions({1, 2, 4}, {1, 2, 3}), KernelVersionOrder::kNewer);
  EXPECT_EQ(CompareKernelVersions({1, 3, 3}, {1, 2, 3}), KernelVersionOrder::kNewer);
  EXPECT_EQ(CompareKernelVersions({2, 2, 3}, {1, 2, 3}), KernelVersionOrder::kNewer);
}

}  // namespace system
}  // namespace px
