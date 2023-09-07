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

#include "src/common/system/kernel_version.h"

#include <elf.h>
#include <link.h>
#include <sys/auxv.h>
#include <sys/utsname.h>

#include <limits>
#include <vector>

#include <absl/strings/str_replace.h>
#include "src/common/system/proc_pid_path.h"

namespace px {
namespace system {

using px::system::ProcPath;
using px::system::ProcPidRootPath;

StatusOr<KernelVersion> ParseKernelVersionString(const std::string& linux_release_str) {
  KernelVersion kernel_version;

  static std::regex uname_regex("^([0-9]+)\\.([0-9]+)\\.([0-9]+)");
  std::smatch match;
  bool s = std::regex_search(linux_release_str, match, uname_regex);
  if (!s || match.size() != 4) {
    return error::Internal("Could not parse kernel version string: [status=$0, match_size=$1]", s,
                           match.size());
  }

  uint32_t tmp;

  // SimpleAtoi should always succeed, due to regex match.
  ECHECK(absl::SimpleAtoi(match[1].str(), &tmp));
  if (tmp > std::numeric_limits<uint16_t>::max()) {
    return error::Internal("Kernel version does not appear valid %d", tmp);
  }
  kernel_version.version = tmp;

  ECHECK(absl::SimpleAtoi(match[2].str(), &tmp));
  if (tmp > std::numeric_limits<uint8_t>::max()) {
    return error::Internal("Kernel major_rev does not appear valid %d", tmp);
  }
  kernel_version.major_rev = tmp;

  ECHECK(absl::SimpleAtoi(match[3].str(), &tmp));
  if (tmp > std::numeric_limits<uint8_t>::max()) {
    return error::Internal("Kernel minor_rev does not appear valid %d", tmp);
  }
  kernel_version.minor_rev = tmp;

  return kernel_version;
}

StatusOr<std::string> GetUname() {
  // The following effectively runs `uname -r`.
  struct utsname buffer;
  if (uname(&buffer) != 0) {
    LOG(ERROR) << "Could not determine kernel version";
    return error::Internal("Could not determine kernel version (uname -r)");
  }

  std::string version_string(buffer.release);
  LOG(INFO) << absl::Substitute("Obtained Linux version string from `uname`: $0", version_string);

  return version_string;
}

StatusOr<std::string> GetProcVersionSignature() {
  const auto version_signature_path = ProcPath("version_signature");
  PX_ASSIGN_OR_RETURN(std::string version_signature, ReadFileToString(version_signature_path));

  LOG(INFO) << absl::Substitute("Obtained Linux version string from $0: $1",
                                version_signature_path.string(), version_signature);

  // Example version signature:
  // Ubuntu 4.15.0-96.97-generic 4.15.18

  std::vector<std::string_view> tokens =
      absl::StrSplit(version_signature, " ", absl::SkipWhitespace());
  if (!tokens.empty()) {
    return std::string(tokens.back());
  }

  return error::NotFound("Could not parse /proc/version_signature file");
}

StatusOr<std::string> GetProcSysKernelVersion() {
  const auto proc_sys_kernel_version = ProcPath("sys", "kernel", "version");
  PX_ASSIGN_OR_RETURN(const std::string version_string, ReadFileToString(proc_sys_kernel_version));

  LOG(INFO) << absl::Substitute("Obtained Linux version string from $0: $1",
                                proc_sys_kernel_version.string(), version_string);

  // Example contents:
  // #1 SMP Debian 4.19.152-1 (2020-10-18)

  static std::regex version_regex(R"([0-9]+\.[0-9]+\.[0-9]+)");
  std::smatch matches;
  bool match_found = std::regex_search(version_string, matches, version_regex);
  if (match_found) {
    return std::string(matches[0]);
  }

  return error::NotFound("Could not find version number in /proc/sys/kernel/version file.");
}

// The Linux vDSO is a virtual dynamic shared object exposed by the Linux kernel.
// The shared object is in ELF format and can contains metadata in a .note ELF section.
// These notes--which are key, value pairs--can contain the Linux version code.
// This function searches the vDSO to find a note about the Linux kernel version.
StatusOr<KernelVersion> GetLinuxVersionFromNoteSection() {
  // Get the vDSO object through getauxval().
  // More information here: https://man7.org/linux/man-pages/man7/vdso.7.html
  auto* vdso = reinterpret_cast<char*>(getauxval(AT_SYSINFO_EHDR));
  if (vdso == nullptr) {
    return error::NotFound("Could not find vDSO.");
  }

  if (!absl::StartsWith(vdso, ELFMAG)) {
    return error::NotFound("vDSO does not appear to be ELF.");
  }

  // Get the ELF header.
  auto elf_hdr = reinterpret_cast<const ElfW(Ehdr)*>(vdso);

  // Go through all ELF section headers, looking for a .note section.
  for (int i = 0; i < elf_hdr->e_shnum; ++i) {
    auto sec_hdr =
        reinterpret_cast<const ElfW(Shdr)*>(vdso + elf_hdr->e_shoff + (i * elf_hdr->e_shentsize));

    // Look for a .note section.
    if (sec_hdr->sh_type == SHT_NOTE) {
      std::string_view note_data(reinterpret_cast<const char*>(vdso + sec_hdr->sh_offset),
                                 sec_hdr->sh_size);

      // Now prcess the .note data as {name, description} pairs.
      // We search for a note that has name "Linux".
      while (!note_data.empty()) {
        auto notes_hdr = reinterpret_cast<const ElfW(Nhdr)*>(note_data.data());
        note_data.remove_prefix(sizeof(const ElfW(Nhdr)));

        std::string_view name(note_data.data(), notes_hdr->n_namesz);
        note_data.remove_prefix(SnapUpToMultiple(name.size(), sizeof(ElfW(Word))));

        std::string_view desc(note_data.data(), notes_hdr->n_descsz);
        note_data.remove_prefix(SnapUpToMultiple(desc.size(), sizeof(ElfW(Word))));

        // Strip off null terminator.
        while (!name.empty() && name.back() == '\x00') {
          name.remove_suffix(1);
        }

        if (name == "Linux" && desc.size() == 4 && notes_hdr->n_type == 0) {
          uint32_t code = *reinterpret_cast<const uint32_t*>(desc.data());

          uint8_t version = (code >> 16);
          uint8_t major_rev = (code >> 8);
          uint8_t minor_rev = (code >> 0);

          if (code >> 24 != 0 || version < 1 || version > 9) {
            return error::Internal("Linux version from vDSO appears corrupted.");
          }

          return KernelVersion{version, major_rev, minor_rev};
        }
      }
    }
  }

  return error::NotFound("Could not extract kernel version from vDSO .note section.");
}

StatusOr<KernelVersion> FindKernelVersion(std::vector<KernelVersionSource> sources) {
  for (const auto& source : sources) {
    switch (source) {
      // Use vDSO .note section to find Linux kernel version.
      case KernelVersionSource::kVDSONoteSection: {
        const StatusOr<KernelVersion> version = GetLinuxVersionFromNoteSection();
        if (version.ok()) {
          LOG(INFO) << "Found Linux kernel version using .note section.";
          return version.ValueOrDie();
        }
      } break;

      // Check /proc/version_signature.
      // Required for Ubuntu distributions.
      case KernelVersionSource::kProcVersionSignature: {
        const StatusOr<std::string> version_string = GetProcVersionSignature();
        if (version_string.ok()) {
          LOG(INFO) << "Found Linux kernel version using /proc/version_signature.";
          return ParseKernelVersionString(version_string.ValueOrDie());
        }
      } break;

      // Use /proc/sys/kernel/version.
      // Required for Debian distributions.
      case KernelVersionSource::kProcSysKernelVersion: {
        const StatusOr<std::string> version_string = GetProcSysKernelVersion();
        if (version_string.ok()) {
          LOG(INFO) << "Found Linux kernel version using /proc/sys/kernel/version.";
          return ParseKernelVersionString(version_string.ValueOrDie());
        }
      } break;

      // Use `uname -r`.
      // Use this as a lower priority, because on Debian and Ubuntu,
      // the uname does  not provide the correct minor version.
      case KernelVersionSource::kUname: {
        const StatusOr<std::string> version_string = GetUname();
        if (version_string.ok()) {
          LOG(INFO) << "Found Linux kernel version using uname.";
          return ParseKernelVersionString(version_string.ValueOrDie());
        }
      } break;

      default:
        LOG(DFATAL) << absl::Substitute("Unhandled KernelVersionSource: $0",
                                        magic_enum::enum_name(source));
        break;
    }
  }

  return error::Internal("Could not determine kernel version.");
}

StatusOr<KernelVersion> GetKernelVersion(std::vector<KernelVersionSource> sources) {
  static std::optional<KernelVersion> g_kernel_version;

  if (g_kernel_version.has_value()) {
    return g_kernel_version.value();
  }

  auto version_or = FindKernelVersion(sources);
  if (version_or.ok()) {
    g_kernel_version = version_or.ValueOrDie();
  }
  return version_or;
}

KernelVersion GetCachedKernelVersion() {
  static auto kernel_version = GetKernelVersion();
  return kernel_version.ConsumeValueOr({0, 0, 0});
}

uint64_t KernelHeadersDistance(KernelVersion a, KernelVersion b) {
  // minor_rev range: 0-255
  // major_rev range: 0-255
  // version range: 0-65535

  // Distance function makes sure major rev change is greater than 2x the minor rev change,
  // so two versions on the same major rev are always closer than two version on different major
  // revs. Similar logic for version.

  return (abs(a.minor_rev - b.minor_rev)) + (abs(a.major_rev - b.major_rev) << 9) +
         (abs(a.version - b.version) << 18);
}

KernelVersionOrder CompareKernelVersions(KernelVersion a, KernelVersion b) {
  if (a.version < b.version) {
    return KernelVersionOrder::kOlder;
  }
  if (a.version > b.version) {
    return KernelVersionOrder::kNewer;
  }
  if (a.major_rev < b.major_rev) {
    return KernelVersionOrder::kOlder;
  }
  if (a.major_rev > b.major_rev) {
    return KernelVersionOrder::kNewer;
  }
  if (a.minor_rev < b.minor_rev) {
    return KernelVersionOrder::kOlder;
  }
  if (a.minor_rev > b.minor_rev) {
    return KernelVersionOrder::kNewer;
  }
  return KernelVersionOrder::kSame;
}

}  // namespace system
}  // namespace px
