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

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace utils {

struct KernelVersion {
  uint8_t version = 0;
  uint8_t major_rev = 0;
  uint8_t minor_rev = 0;

  uint32_t code() const { return (version << 16) | (major_rev << 8) | (minor_rev); }
  std::string ToString() const {
    return absl::Substitute("$0.$1.$2", version, major_rev, minor_rev);
  }
};

enum class KernelVersionSource {
  // Get the Linux version from VDSO .note section.
  // Most reliable if available.
  kVDSONoteSection,

  // Get the Linux version from /proc/version_signature.
  kProcVersionSignature,

  // Get the Linux version from /proc/sys/kernel/version.
  kProcSysKernelVersion,

  // Get the Linux version from `uname -r`.
  kUname,
};

// Order matters.
// Uname should always be last because it does not report the correct minor version on
// Ubuntu/Debian version
const std::vector<KernelVersionSource> kDefaultKernelVersionSources = {
    KernelVersionSource::kVDSONoteSection, KernelVersionSource::kProcVersionSignature,
    KernelVersionSource::kProcSysKernelVersion, KernelVersionSource::kUname};

uint64_t KernelHeadersDistance(KernelVersion a, KernelVersion b);

enum class KernelVersionOrder {
  kOlder,
  kSame,
  kNewer,
};

// Compares two kernel versions and detect their relationship.
KernelVersionOrder CompareKernelVersions(KernelVersion a, KernelVersion b);

/**
 * Parses a the Linux version code from a string.
 *
 * @param linux_release the kernel version string (e.g. 4.15.8-foobar).
 * @return Linux version as {version, major, minor}.
 */
StatusOr<KernelVersion> ParseKernelVersionString(const std::string& linux_release);

/**
 * Returns the kernel version from vDSO .note section.
 */
StatusOr<KernelVersion> GetLinuxVersionFromNoteSection();

/**
 * Returns the kernel version from /proc/version_signature.
 * This is required for Ubuntu, which does not report the correct minor version through uname.
 * Other distributions do not have /proc/version signature.
 *
 * @return Kernel release version (e.g. 4.15.4)
 */
StatusOr<std::string> GetProcVersionSignature();

/**
 * Determines the linux kernel version.
 * It returns the cached value if the value is present, otherwise performs the search in the same
 * way as FindKernelVersion().
 */
StatusOr<KernelVersion> GetKernelVersion(
    std::vector<KernelVersionSource> sources = kDefaultKernelVersionSources);

/**
 * Determines the linux kernel version.
 * It first searches /proc/version_signature (for Ubuntu distros).
 * If /proc/version_signature does not exist, it uses uname.
 *
 * Note that the version number reported by uname on Ubuntu distros does not include the minor
 * version, and thus cannot be used to get an accurate version.
 *
 * @return The kernel version, or error if it could not be determined.
 */
StatusOr<KernelVersion> FindKernelVersion(
    std::vector<KernelVersionSource> sources = kDefaultKernelVersionSources);

/**
 * Returns the cached linux kernel version.
 * @return The pre-computed kernel version, or {0, 0, 0} if not found.
 */
KernelVersion GetCachedKernelVersion();

/**
 * Modifies the version.h on the filesystem to the specified version.
 * This is to enable BPF programs to load on host systems which do
 * not have their own linux headers installed.
 *
 * In theory, this is actually quite dangerous, but in practice it seems to work.
 *
 * @param linux_headers_base path to root directory of Linux headers root to modify.
 * @param uname_kernel_release the desired linux release (e.g. 4.18.0-25-generic) to use.
 * @return Status error if unable to modify the sources.
 */
Status ModifyKernelVersion(const std::filesystem::path& linux_headers_base,
                           uint32_t linux_version_code);

/**
 * Tries to find a kernel config file that matches. Currently only searches /boot.
 * In the future could be expanded to search /proc/config too.
 *
 * @return Path to a valid kernel config from the host.
 */
StatusOr<std::filesystem::path> FindKernelConfig();

/**
 * Replace the include/generated/autoconf.h in the packaged headers with a new one generated from a
 * given linux config.
 *
 * @param linux_headers_base Path to linux headers root directory with autoconf.h to be patched.
 * @param config_file Config file from which to extract config parameters.
 * @param hz Output: value of CONFIG_HZ if found in the linux config.
 * @return error if autoconf.h could not be generated.
 */
Status GenAutoConf(const std::filesystem::path& linux_headers_base,
                   const std::filesystem::path& config_file, int* hz = nullptr);

/**
 * Replace the include/generated/timeconst.h in the packaged headers with a new one based on
 * CONFIG_HZ.
 *
 * @param linux_headers_base Path to linux headers root directory with timeconst.h to be patched.
 * @param hz Value of CONFIG_HZ.
 * @return error if timeconst.h could not be generated.
 */
Status GenTimeconst(const std::filesystem::path& linux_headers_base, int hz);

/**
 * Tries to find a valid kernel config, and patches the prepackaged headers
 * with re-generated header files to match the config.
 * In particular, autoconf.h and timeconst.h, are currently replaced.
 *
 * @param linux_headers_base Path to linux headers root directory to be patched.
 * @param linux config file to use to generate the patch.
 * @return Error if config file could not be found, or if patch fails.
 */
Status ApplyConfigPatches(const std::filesystem::path& linux_headers_base);

struct PackagedLinuxHeadersSpec {
  KernelVersion version;
  // This path stores either (1) the path to the tarball (before it has been extracted),
  // or (2) the path the extracted headers (after it has been extracted).
  std::filesystem::path path;
};

StatusOr<PackagedLinuxHeadersSpec> FindClosestPackagedLinuxHeaders(
    const std::filesystem::path& packaged_headers_root, KernelVersion kernel_version);

Status InstallPackagedLinuxHeaders(const std::filesystem::path& lib_modules_dir);

// After headers are installed, this variable is set to true.
// Future calls to InstallPackagedLinuxHeaders() will not re-install the headers,
// but this variable lets us know the headers were installed on the filesystem.
inline bool g_packaged_headers_installed = false;

/**
 * This function attempts to ensure that the host system has Linux headers.
 * Currently this required by Stirling, so that we can deploy BPF probes.
 *
 * The function first tries to locate the host system's headers.
 * If found, then it makes no modifications.
 * If host linux headers are not found, then it attempts to install packaged headers.
 *
 * Note that the packaged headers can only be installed if they are available.
 * In a containerized environment, the container should have the packaged headers in the image for
 * this to work.
 *
 * @return Status error if no headers.
 */
Status FindOrInstallLinuxHeaders();

}  // namespace utils
}  // namespace stirling
}  // namespace px
