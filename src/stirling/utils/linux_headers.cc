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

#include "src/stirling/utils/linux_headers.h"

#include <elf.h>
#include <link.h>
#include <sys/auxv.h>
#include <sys/utsname.h>

#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

#include <absl/strings/numbers.h>
#include <absl/strings/str_replace.h>

#include "src/common/base/file.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/fs/temp_file.h"
#include "src/common/minitar/minitar.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_pid_path.h"
#include "src/common/zlib/zlib_wrapper.h"

namespace px {
namespace stirling {
namespace utils {

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

Status ModifyKernelVersion(const std::filesystem::path& linux_headers_base,
                           uint32_t linux_version_code) {
  std::filesystem::path version_file_path =
      linux_headers_base / "include/generated/uapi/linux/version.h";

  // Read the file into a string.
  PX_ASSIGN_OR_RETURN(std::string file_contents, ReadFileToString(version_file_path));

  // Modify the version code.
  LOG(INFO) << absl::Substitute("Overriding linux version code to $0", linux_version_code);
  std::string linux_version_code_override =
      absl::Substitute("#define LINUX_VERSION_CODE $0", linux_version_code);
  static std::regex e("#define LINUX_VERSION_CODE ([0-9]*)");
  std::string new_file_contents = std::regex_replace(file_contents, e, linux_version_code_override);

  // Write the modified file back.
  PX_RETURN_IF_ERROR(WriteFileFromString(version_file_path, new_file_contents));

  return Status::OK();
}

// There is no standard place where Linux distributions place their kernel config files,
// (assuming they even make it available at all). This function looks at a number of common places
// where the config could be found:
//  - /proc/config or /proc/config.gz: Available if the distro has enabled this kernel feature.
//  - /boot/config-<uname>: Common place to store the config.
//  - /lib/modules/<uname>/config: Used by RHEL8 CoreOS, and potentially other RHEL distros.
StatusOr<std::filesystem::path> FindKernelConfig() {
  const system::Config& syscfg = system::Config::GetInstance();
  PX_ASSIGN_OR_RETURN(const std::string uname, GetUname());

  const std::vector<std::filesystem::path> search_paths = {
      // Used when CONFIG_IKCONFIG=y is set.
      ProcPath("config"),
      // Used when CONFIG_IKCONFIG_PROC=y is set.
      ProcPath("config.gz"),
      // Search for /boot/config-<uname>
      syscfg.ToHostPath(absl::StrCat("/boot/config-", uname)),
      // Search for /lib/modules/<uname>/config
      syscfg.ToHostPath(absl::StrCat("/lib/modules/", uname, "/config")),
      // TODO(yzhao): https://github.com/lima-vm/alpine-lima/issues/67 once this issue is resolved,
      // we might consider change these 2 paths into something recommended by rancher-desktop.
      // The path used by `alpine-lima` in "Live CD" boot mechanism.
      ProcPidRootPath(1, "media", "sr0", "boot", "config-virt"),
      // The path used by `alpine-lima` in "Live CD" boot mechanism on Mac machine.
      ProcPidRootPath(1, "media", "vda", "boot", "config-virt"),
  };
  std::vector<std::string> searched;

  for (const auto& path : search_paths) {
    if (fs::Exists(path)) {
      LOG(INFO) << absl::Substitute("Found kernel config at: $0.", path.string());
      return path;
    }
    searched.push_back(path.string());
  }
  return error::NotFound("No kernel config found. Searched: $0.", absl::StrJoin(searched, ","));
}

Status GenAutoConf(const std::filesystem::path& linux_headers_base,
                   const std::filesystem::path& config_file, int* hz) {
  // Sample config file format:
  //  CONFIG_A=y
  //  CONFIG_B=m
  //  CONFIG_FOO="foofoo"
  //
  // Sample autoconf.h output format:
  //  #define CONFIG_A 1
  //  #define CONFIG_B_MODULE 1
  //  #define CONFIG_FOO "foofoo"

  std::ifstream fin(config_file);

  // If file is gzipped, then unzip it and read that instead.
  if (config_file.extension() == ".gz") {
    PX_ASSIGN_OR_RETURN(std::string config_gzip_contents, ReadFileToString(config_file));
    PX_ASSIGN_OR_RETURN(std::string config_contents, px::zlib::Inflate(config_gzip_contents));

    std::unique_ptr<fs::TempFile> tmp_file = fs::TempFile::Create();
    std::filesystem::path tmp_file_path = tmp_file->path();
    PX_RETURN_IF_ERROR(WriteFileFromString(tmp_file_path, config_contents));
    fin = std::ifstream(tmp_file_path);
  }

  std::filesystem::path autoconf_file_path = linux_headers_base / "include/generated/autoconf.h";
  std::ofstream fout(autoconf_file_path);

  // Generate autoconf.h from the config file.
  // Perform the equivalent of:
  // grep "^CONFIG" fin | sed -e "s/=y/ 1/g" -e "s/=m/_MODULE 1/g" -e "s/=/ /g" -e
  // "s/^CONFIG/#define CONFIG/g"
  std::string line;
  while (std::getline(fin, line)) {
    if (!absl::StartsWith(line, "CONFIG_")) {
      continue;
    }

    // While scanning, look for HZ value.
    if (hz != nullptr && absl::StartsWith(line, "CONFIG_HZ=")) {
      std::vector<std::string_view> tokens = absl::StrSplit(line, "=");
      if (tokens.size() == 2) {
        bool ok = absl::SimpleAtoi(tokens[1], hz);
        LOG_IF(WARNING, !ok) << "Extracting CONFIG_HZ value failed";
      }
    }

    // Equivalent of sed -e "s/=y/ 1/g" -e "s/=m/_MODULE 1/g" -e "s/=/ /g".
    std::string line_out =
        absl::StrReplaceAll(line, {{"=y", " 1"}, {"=m", "_MODULE 1"}, {"=", " "}});

    // Prefix line with a #define
    line_out = absl::StrCat("#define ", line_out);
    fout << line_out << '\n';
  }

  return Status::OK();
}

Status GenTimeconst(const std::filesystem::path& linux_headers_base, int hz) {
  const std::filesystem::path kPackagedHeadersRoot = "/px";

  std::filesystem::path timeconst_path = linux_headers_base / "include/generated/timeconst.h";
  std::string src_file =
      absl::StrCat(kPackagedHeadersRoot.string(), "/timeconst_", std::to_string(hz), ".h");
  PX_RETURN_IF_ERROR(
      fs::Copy(src_file, timeconst_path, std::filesystem::copy_options::overwrite_existing));

  return Status::OK();
}

Status ApplyConfigPatches(const std::filesystem::path& linux_headers_base) {
  Status s;
  int hz = 0;

  // Find kernel config.
  PX_ASSIGN_OR_RETURN(std::filesystem::path kernel_config, FindKernelConfig());

  // Attempt to generate autconf.h based on the config.
  // While scanning, also pull out the CONFIG_HZ value.
  PX_RETURN_IF_ERROR(GenAutoConf(linux_headers_base, kernel_config, &hz));

  // Attempt to generate timeconst.h based on the HZ in the config.
  PX_RETURN_IF_ERROR(GenTimeconst(linux_headers_base, hz));

  return Status::OK();
}

StatusOr<std::filesystem::path> FindLinuxHeadersDirectory(
    const std::filesystem::path& lib_modules_dir) {
  // bcc/loader.cc looks for Linux headers in the following order:
  //   /lib/modules/<uname>/source
  //   /lib/modules/<uname>/build

  std::filesystem::path lib_modules_source_dir = lib_modules_dir / "source";
  std::filesystem::path lib_modules_build_dir = lib_modules_dir / "build";

  if (fs::Exists(lib_modules_source_dir)) {
    return lib_modules_source_dir;
  } else if (fs::Exists(lib_modules_build_dir)) {
    return lib_modules_build_dir;
  }
  return error::NotFound("Could not found 'source' or 'build' under $0", lib_modules_dir.string());
}

Status LinkHostLinuxHeaders(const std::filesystem::path& lib_modules_dir) {
  // Host dir is where we must mount host directories into the container.
  const std::filesystem::path kHostDir = "/host";

  // Careful. Must use operator+ instead of operator/ here.
  // operator/ will replace the path if the second argument appears to be an absolute path.
  std::filesystem::path host_lib_modules_dir = kHostDir;
  host_lib_modules_dir += lib_modules_dir;
  LOG(INFO) << absl::Substitute("Looking for host mounted headers at $0",
                                host_lib_modules_dir.string());

  std::filesystem::path host_lib_modules_source_dir = host_lib_modules_dir / "source";
  std::filesystem::path host_lib_modules_build_dir = host_lib_modules_dir / "build";
  std::filesystem::path lib_modules_source_dir = lib_modules_dir / "source";
  std::filesystem::path lib_modules_build_dir = lib_modules_dir / "build";

  // Since the host directory is assumed to be a mount, any symlinks will be broken.
  // Adjust these symlinks by prepending kHostDir in hopes of fixing them.

  std::error_code ec;
  if (std::filesystem::is_symlink(host_lib_modules_source_dir, ec)) {
    auto target = std::filesystem::read_symlink(host_lib_modules_source_dir, ec);

    // Careful. Must use operator+ instead of operator/ here.
    // operator/ will replace the path if the second argument appears to be an absolute path.
    host_lib_modules_source_dir = kHostDir;
    host_lib_modules_source_dir += target;
    ECHECK(!ec);
  }
  if (std::filesystem::is_symlink(host_lib_modules_build_dir, ec)) {
    auto target = std::filesystem::read_symlink(host_lib_modules_build_dir, ec);

    // Careful. Must use operator+ instead of operator/ here.
    // operator/ will replace the path if the second argument appears to be an absolute path.
    host_lib_modules_build_dir = kHostDir;
    host_lib_modules_build_dir += target;
    ECHECK(!ec);
  }

  VLOG(1) << absl::Substitute("source_dir $0", host_lib_modules_source_dir.string());
  VLOG(1) << absl::Substitute("build_dir $0", host_lib_modules_build_dir.string());

  if (fs::Exists(host_lib_modules_source_dir)) {
    PX_RETURN_IF_ERROR(
        fs::CreateSymlinkIfNotExists(host_lib_modules_source_dir, lib_modules_source_dir));
    LOG(INFO) << absl::Substitute("Linked linux headers found at $0 to symlink at $1",
                                  host_lib_modules_source_dir.string(),
                                  lib_modules_source_dir.string());
  }

  if (fs::Exists(host_lib_modules_build_dir)) {
    PX_RETURN_IF_ERROR(
        fs::CreateSymlinkIfNotExists(host_lib_modules_build_dir, lib_modules_build_dir));
    LOG(INFO) << absl::Substitute("Linked linux headers found at $0 to symlink at $1",
                                  host_lib_modules_build_dir.string(),
                                  lib_modules_build_dir.string());
  }

  return Status::OK();
}

Status ExtractPackagedHeaders(const PackagedLinuxHeadersSpec* headers_package,
                              const std::string staging_directory,
                              const std::string expected_directory) {
  std::filesystem::create_directories(staging_directory);
  // Extract the files from the tarball, stripping the leading prefix
  // "usr/src/linux-headers-$0.$1.$2-pl" to avoid unnecessary nesting in the staging directory.
  ::px::tools::Minitar minitar(headers_package->path.string());
  const std::string prefix_to_strip = expected_directory.substr(1);
  PX_RETURN_IF_ERROR(minitar.Extract(staging_directory, prefix_to_strip));
  // Check that the staging path was created.
  if (!fs::Exists(staging_directory)) {
    return error::Internal(
        "Package extraction did not result in the expected headers directory: $0.",
        expected_directory);
  }
  return Status::OK();
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

StatusOr<PackagedLinuxHeadersSpec> FindClosestPackagedLinuxHeaders(
    const std::filesystem::path& packaged_headers_root, KernelVersion kernel_version) {
#if X86_64
  const std::string kHeaderDirPrefix =
      std::filesystem::path(packaged_headers_root / "linux-headers-x86_64-").string();
#elif AARCH64
  const std::string kHeaderDirPrefix =
      std::filesystem::path(packaged_headers_root / "linux-headers-arm64-").string();
#else
#error Architecture not supported
#endif
  const std::string_view kHeaderDirSuffix = ".tar.gz";

  PackagedLinuxHeadersSpec selected;

  if (fs::Exists(packaged_headers_root)) {
    for (const auto& p : std::filesystem::directory_iterator(packaged_headers_root)) {
      VLOG(1) << absl::Substitute("File: $0", p.path().string());
      std::string path = p.path().string();

      if (!absl::StartsWith(path, kHeaderDirPrefix) || !absl::EndsWith(path, kHeaderDirSuffix)) {
        continue;
      }

      std::string_view version_string_view(path);
      version_string_view.remove_prefix(kHeaderDirPrefix.size());
      version_string_view.remove_suffix(kHeaderDirSuffix.size());
      std::string version_string(version_string_view);

      StatusOr<KernelVersion> headers_kernel_version_status =
          ParseKernelVersionString(version_string);
      if (!headers_kernel_version_status.ok()) {
        LOG(WARNING) << absl::Substitute(
            "Ignoring $0 since it does not conform to the naming convention", path);
        continue;
      }
      KernelVersion headers_kernel_version = headers_kernel_version_status.ValueOrDie();

      if (selected.path.empty() || KernelHeadersDistance(kernel_version, headers_kernel_version) <
                                       KernelHeadersDistance(kernel_version, selected.version)) {
        selected = {headers_kernel_version, p};
      }
    }
  }

  if (selected.path.empty()) {
    return error::Internal("Could not find packaged headers to install. Search location: $0",
                           packaged_headers_root.string());
  }

  return selected;
}

Status InstallPackagedLinuxHeaders(const std::filesystem::path& lib_modules_dir) {
  // This is the directory in our container images that contains packaged linux headers.
  const std::filesystem::path kPackagedHeadersRoot = "/px";

  std::filesystem::path lib_modules_build_dir = lib_modules_dir / "build";

  LOG(INFO) << "Attempting to install packaged headers.";

  PX_ASSIGN_OR_RETURN(KernelVersion kernel_version, GetKernelVersion());

  PX_ASSIGN_OR_RETURN(PackagedLinuxHeadersSpec packaged_headers,
                      FindClosestPackagedLinuxHeaders(kPackagedHeadersRoot, kernel_version));
  LOG(INFO) << absl::Substitute("Using packaged header: $0", packaged_headers.path.string());
  const std::string version =
      absl::Substitute("$0.$1.$2-pl", packaged_headers.version.version,
                       packaged_headers.version.major_rev, packaged_headers.version.minor_rev);
  const std::string staging_directory = absl::StrCat("/usr/src/staging/linux-headers-", version);
  const std::string expected_directory = absl::StrCat("/usr/src/linux-headers-" + version);
  // Verify that the target directory doesn't already exist.
  // If someone built a tar.gz with an incorrect directory structure, this check wouldn't save us.
  if (fs::Exists(expected_directory)) {
    return error::Internal(
        "Not attempting to install packaged headers because the target directory already exists: "
        "$0",
        expected_directory);
  }
  // Extract the packaged headers to a staging directory, stripping the expected target directory
  // prefix.
  PX_RETURN_IF_ERROR(
      ExtractPackagedHeaders(&packaged_headers, staging_directory, expected_directory));
  // Modify version.h to the specific kernel version in the staged headers.
  PX_RETURN_IF_ERROR(ModifyKernelVersion(staging_directory, kernel_version.code()));
  // Find valid kernel config and patch the staged headers to match.
  PX_RETURN_IF_ERROR(ApplyConfigPatches(staging_directory));
  // Move the staged headers to the expected, target directory.
  std::filesystem::rename(staging_directory, expected_directory);
  PX_RETURN_IF_ERROR(fs::CreateSymlinkIfNotExists(expected_directory, lib_modules_build_dir));
  LOG(INFO) << absl::Substitute("Successfully installed packaged copy of headers at $0",
                                lib_modules_build_dir.string());
  g_packaged_headers_installed = true;
  return Status::OK();
}

StatusOr<std::filesystem::path> FindOrInstallLinuxHeaders() {
  PX_ASSIGN_OR_RETURN(std::string uname, GetUname());
  LOG(INFO) << absl::Substitute("Detected kernel release (uname -r): $0", uname);

  std::filesystem::path lib_modules_dir = "/lib/modules/" + uname;
  std::filesystem::path headers_dir;

  // Some strategies require the base directory to be present.
  // This does nothing if the directory already exists.
  PX_RETURN_IF_ERROR(fs::CreateDirectories(lib_modules_dir));

  auto status_or = FindLinuxHeadersDirectory(lib_modules_dir);
  // TODO(yzhao): Consider add a PX_RETURN_IF_OK() macro to return the held value of StatusOr.
  // A problem, when implementing PX_RETURN_IF_OK() in similar manner as PX_RETURN_IF_ERROR(),
  // is that the return value of the input expression to PX_RETURN_IF_OK() cannot be bound to
  // a non-const reference. Therefore, the following code will invoke StatusOr's copy constructor:
  // auto status_or = ...;
  // PX_RETURN_IF_OK(status_or);
  if (status_or.ok()) {
    return status_or.ConsumeValueOrDie();
  }

  auto status = LinkHostLinuxHeaders(lib_modules_dir);
  LOG_IF(INFO, !status.ok()) << absl::Substitute(
      "Failed to link host's Linux headers to $0, error: $1", lib_modules_dir.string(),
      status.ToString());
  if (status.ok()) {
    auto status_or = FindLinuxHeadersDirectory(lib_modules_dir);
    if (status_or.ok()) {
      return status_or.ConsumeValueOrDie();
    }
  }

  status = InstallPackagedLinuxHeaders(lib_modules_dir);
  LOG_IF(INFO, !status.ok()) << absl::Substitute(
      "Failed to install packaged Linux headers to $0, error: $1", lib_modules_dir.string(),
      status.ToString());
  if (status.ok()) {
    auto status_or = FindLinuxHeadersDirectory(lib_modules_dir);
    if (status_or.ok()) {
      return status_or.ConsumeValueOrDie();
    }
  }

  return error::Internal("Could not find any linux headers to use.");
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
