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
#include "src/stirling/utils/linux_headers.h"

DECLARE_string(proc_path);

namespace px {
namespace stirling {
namespace utils {

using ::px::testing::TempDir;
using ::testing::EndsWith;

bool operator==(const system::KernelVersion& a, const system::KernelVersion& b) {
  return (a.version == b.version) && (a.major_rev == b.major_rev) && (a.minor_rev == b.minor_rev);
}

TEST(LinuxHeadersUtils, ModifyVersion) {
  // Test Setup

  // Use 000000 (which is not a real kernel version), so we can detect changes.
  std::string version_h_original = R"(#define LINUX_VERSION_CODE 000000
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
)";

  TempDir tmp_dir;

  std::filesystem::path version_h_dir = tmp_dir.path() / "include/generated/uapi/linux";
  std::filesystem::create_directories(version_h_dir);
  std::string version_h_filename = version_h_dir / "version.h";

  // Write the original file to disk.
  ASSERT_OK(WriteFileFromString(version_h_filename, version_h_original));

  // Functions Under Test

  StatusOr<system::KernelVersion> host_linux_version = system::FindKernelVersion();
  ASSERT_OK(host_linux_version);
  uint32_t host_linux_version_code = host_linux_version.ValueOrDie().code();
  EXPECT_GT(host_linux_version_code, 0);

  EXPECT_OK(ModifyKernelVersion(tmp_dir.path(), host_linux_version_code));

  // Read the file into a string.
  std::string expected_contents_template =
      "#define LINUX_VERSION_CODE $0\n"
      "#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))\n";

  std::string expected_contents =
      absl::Substitute(expected_contents_template, host_linux_version_code);

  EXPECT_OK_AND_EQ(ReadFileToString(version_h_filename), expected_contents);
}

const std::string_view kExpectedConfigContents = R"(#define CONFIG_64BIT 1
#define CONFIG_X86_64 1
#define CONFIG_X86 1
#define CONFIG_INSTRUCTION_DECODER 1
#define CONFIG_OUTPUT_FORMAT "elf64-x86-64"
#define CONFIG_ARCH_DEFCONFIG "arch/x86/configs/x86_64_defconfig"
#define CONFIG_LOCKDEP_SUPPORT 1
#define CONFIG_STACKTRACE_SUPPORT 1
#define CONFIG_MMU 1
#define CONFIG_X86_64_SMP 1
#define CONFIG_ARCH_SUPPORTS_UPROBES 1
#define CONFIG_FIX_EARLYCON_MEM 1
#define CONFIG_PGTABLE_LEVELS 4
#define CONFIG_DEFCONFIG_LIST "/lib/modules/$UNAME_RELEASE/.config"
#define CONFIG_IRQ_WORK 1
#define CONFIG_BUILDTIME_EXTABLE_SORT 1
#define CONFIG_THREAD_INFO_IN_TASK 1
#define CONFIG_HZ 250
#define CONFIG_OPROFILE_MODULE 1
)";

std::string PrepareAutoConf(std::filesystem::path linux_headers_dir) {
  std::filesystem::path include_generated_dir = linux_headers_dir / "include/generated";
  std::filesystem::create_directories(include_generated_dir);
  std::string autoconf_h_filename = include_generated_dir / "autoconf.h";
  return autoconf_h_filename;
}

TEST(LinuxHeadersUtils, GenAutoConf) {
  const std::string kConfigFile = testing::BazelRunfilePath("src/stirling/utils/testdata/config");

  TempDir linux_headers_dir;
  std::string autoconf_h_filename = PrepareAutoConf(linux_headers_dir.path());

  int hz;
  ASSERT_OK(GenAutoConf(linux_headers_dir.path(), kConfigFile, &hz));
  ASSERT_OK_AND_EQ(ReadFileToString(autoconf_h_filename), kExpectedConfigContents);
}

TEST(LinuxHeadersUtils, GenAutoConfGz) {
  const std::string kConfigFile =
      testing::BazelRunfilePath("src/stirling/utils/testdata/config.gz");

  TempDir linux_headers_dir;
  std::string autoconf_h_filename = PrepareAutoConf(linux_headers_dir.path());

  int hz;
  ASSERT_OK(GenAutoConf(linux_headers_dir.path(), kConfigFile, &hz));
  ASSERT_OK_AND_EQ(ReadFileToString(autoconf_h_filename), kExpectedConfigContents);
}

TEST(LinuxHeadersUtils, FindClosestPackagedLinuxHeaders) {
  const std::string kTestSrcDir =
      testing::BazelRunfilePath("src/stirling/utils/testdata/test_header_packages");

  std::string prefix = "src/stirling/utils/testdata/test_header_packages/linux-headers-";
#if X86_64
  prefix = prefix + "x86_64-";
#elif AARCH64
  prefix = prefix + "arm64-";
#endif

  {
    ASSERT_OK_AND_ASSIGN(
        PackagedLinuxHeadersSpec match,
        FindClosestPackagedLinuxHeaders(kTestSrcDir, system::KernelVersion{4, 4, 18}));
    EXPECT_THAT(match.path.string(), EndsWith(prefix + "4.14.176.tar.gz"));
  }

  {
    ASSERT_OK_AND_ASSIGN(
        PackagedLinuxHeadersSpec match,
        FindClosestPackagedLinuxHeaders(kTestSrcDir, system::KernelVersion{4, 15, 10}));
    EXPECT_THAT(match.path.string(), EndsWith(prefix + "4.14.176.tar.gz"));
  }

  {
    ASSERT_OK_AND_ASSIGN(
        PackagedLinuxHeadersSpec match,
        FindClosestPackagedLinuxHeaders(kTestSrcDir, system::KernelVersion{4, 18, 1}));
    EXPECT_THAT(match.path.string(), EndsWith(prefix + "4.18.20.tar.gz"));
  }

  {
    ASSERT_OK_AND_ASSIGN(
        PackagedLinuxHeadersSpec match,
        FindClosestPackagedLinuxHeaders(kTestSrcDir, system::KernelVersion{5, 0, 0}));
    EXPECT_THAT(match.path.string(), EndsWith(prefix + "5.3.18.tar.gz"));
  }

  {
    ASSERT_OK_AND_ASSIGN(
        PackagedLinuxHeadersSpec match,
        FindClosestPackagedLinuxHeaders(kTestSrcDir, system::KernelVersion{5, 7, 20}));
    EXPECT_THAT(match.path.string(), EndsWith(prefix + "5.3.18.tar.gz"));
  }
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
