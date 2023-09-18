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
namespace system {

struct KernelVersion {
  uint8_t version = 0;
  uint8_t major_rev = 0;
  uint8_t minor_rev = 0;

  uint32_t code() const { return (version << 16) | (major_rev << 8) | (minor_rev); }
  std::string ToString() const {
    return absl::Substitute("$0.$1.$2", version, major_rev, minor_rev);
  }

  bool operator==(const KernelVersion& other) const {
    return (version == other.version) && (major_rev == other.major_rev) &&
           (minor_rev == other.minor_rev);
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
 * Effectively runs `uname -r` and returns the output.
 */
StatusOr<std::string> GetUname();

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

}  // namespace system
}  // namespace px
