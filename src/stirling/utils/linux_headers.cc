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
#include "src/common/zlib/zlib_wrapper.h"

namespace px {
namespace stirling {
namespace utils {

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
  std::filesystem::path version_signature_path =
      system::Config::GetInstance().proc_path() / "version_signature";
  PL_ASSIGN_OR_RETURN(std::string version_signature, ReadFileToString(version_signature_path));

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
  std::filesystem::path proc_sys_kernel_version =
      system::Config::GetInstance().proc_path() / "sys/kernel/version";
  PL_ASSIGN_OR_RETURN(std::string version_string, ReadFileToString(proc_sys_kernel_version));

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

StatusOr<KernelVersion> GetKernelVersion() {
  // First option is to check /proc/version_signature.
  // Required for Ubuntu distributions.
  StatusOr<std::string> version_string_status = GetProcVersionSignature();

  // Second option is to use /proc/sys/kernel/version.
  // Required for Debian distributions.
  if (!version_string_status.ok()) {
    version_string_status = GetProcSysKernelVersion();
  }

  // Last option is to use `uname -r`.
  // This must be the last option, because on Debian and Ubuntu, the uname does
  // not provide the correct minor version.
  if (!version_string_status.ok()) {
    version_string_status = GetUname();
  }

  PL_RETURN_IF_ERROR(version_string_status);
  return ParseKernelVersionString(version_string_status.ConsumeValueOrDie());
}

Status ModifyKernelVersion(const std::filesystem::path& linux_headers_base,
                           uint32_t linux_version_code) {
  std::filesystem::path version_file_path =
      linux_headers_base / "include/generated/uapi/linux/version.h";

  // Read the file into a string.
  PL_ASSIGN_OR_RETURN(std::string file_contents, ReadFileToString(version_file_path));

  // Modify the version code.
  LOG(INFO) << absl::Substitute("Overriding linux version code to $0", linux_version_code);
  std::string linux_version_code_override =
      absl::Substitute("#define LINUX_VERSION_CODE $0", linux_version_code);
  static std::regex e("#define LINUX_VERSION_CODE ([0-9]*)");
  std::string new_file_contents = std::regex_replace(file_contents, e, linux_version_code_override);

  // Write the modified file back.
  PL_RETURN_IF_ERROR(WriteFileFromString(version_file_path, new_file_contents));

  return Status::OK();
}

// There is no standard place where Linux distributions place their kernel config files,
// (assuming they even make it available at all). This function looks at a number of common places
// where the config could be found:
//  - /proc/config or /proc/config.gz: Available if the distro has enabled this kernel feature.
//  - /boot/config-<uname>: Common place to store the config.
//  - /lib/modules/<uname>/config: Used by RHEL8 CoreOS, and potentially other RHEL distros.
StatusOr<std::filesystem::path> FindKernelConfig() {
  const system::Config& sysconfig = system::Config::GetInstance();

  PL_ASSIGN_OR_RETURN(std::string uname, GetUname());

  // Search for /boot/config-<uname>
  std::string boot_kconfig = absl::StrCat("/boot/config-", uname);

  // Search for /lib/modules/<uname>/config
  std::string lib_modules_config = absl::StrCat("/lib/modules/", uname, "/config");

  std::vector<std::string> search_paths = {"/proc/config", "/proc/config.gz", boot_kconfig,
                                           lib_modules_config};
  for (const auto& path : search_paths) {
    std::filesystem::path config_path = path;
    std::filesystem::path host_path = sysconfig.ToHostPath(config_path);
    if (fs::Exists(host_path).ok()) {
      LOG(INFO) << absl::Substitute("Found kernel config at $0", host_path.string());
      return host_path;
    }
  }

  return error::NotFound("No kernel config found. Paths searched: $0",
                         absl::StrJoin(search_paths, ","));
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
    PL_ASSIGN_OR_RETURN(std::string config_gzip_contents, ReadFileToString(config_file));
    PL_ASSIGN_OR_RETURN(std::string config_contents, px::zlib::Inflate(config_gzip_contents));

    std::unique_ptr<fs::TempFile> tmp_file = fs::TempFile::Create();
    std::filesystem::path tmp_file_path = tmp_file->path();
    PL_RETURN_IF_ERROR(WriteFileFromString(tmp_file_path, config_contents));
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
  const std::filesystem::path kPackagedHeadersRoot = "/pl";

  std::filesystem::path timeconst_path = linux_headers_base / "include/generated/timeconst.h";
  std::string src_file =
      absl::StrCat(kPackagedHeadersRoot.string(), "/timeconst_", std::to_string(hz), ".h");
  PL_RETURN_IF_ERROR(
      fs::Copy(src_file, timeconst_path, std::filesystem::copy_options::overwrite_existing));

  return Status::OK();
}

Status ApplyConfigPatches(const std::filesystem::path& linux_headers_base) {
  Status s;
  int hz = 0;

  // Find kernel config.
  PL_ASSIGN_OR_RETURN(std::filesystem::path kernel_config, FindKernelConfig());

  // Attempt to generate autconf.h based on the config.
  // While scanning, also pull out the CONFIG_HZ value.
  PL_RETURN_IF_ERROR(GenAutoConf(linux_headers_base, kernel_config, &hz));

  // Attempt to generate timeconst.h based on the HZ in the config.
  PL_RETURN_IF_ERROR(GenTimeconst(linux_headers_base, hz));

  return Status::OK();
}

std::filesystem::path FindLinuxHeadersDirectory(const std::filesystem::path& lib_modules_dir) {
  // bcc/loader.cc looks for Linux headers in the following order:
  //   /lib/modules/<uname>/source
  //   /lib/modules/<uname>/build

  std::filesystem::path lib_modules_source_dir = lib_modules_dir / "source";
  std::filesystem::path lib_modules_build_dir = lib_modules_dir / "build";

  std::filesystem::path lib_modules_kdir;
  if (fs::Exists(lib_modules_source_dir).ok()) {
    lib_modules_kdir = lib_modules_source_dir;
  } else if (fs::Exists(lib_modules_build_dir).ok()) {
    lib_modules_kdir = lib_modules_build_dir;
  }

  return lib_modules_kdir;
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

  if (fs::Exists(host_lib_modules_source_dir).ok()) {
    PL_RETURN_IF_ERROR(
        fs::CreateSymlinkIfNotExists(host_lib_modules_source_dir, lib_modules_source_dir));
    LOG(INFO) << absl::Substitute("Linked linux headers found at $0",
                                  host_lib_modules_source_dir.string());
  }

  if (fs::Exists(host_lib_modules_build_dir).ok()) {
    PL_RETURN_IF_ERROR(
        fs::CreateSymlinkIfNotExists(host_lib_modules_build_dir, lib_modules_build_dir));
    LOG(INFO) << absl::Substitute("Linked linux headers found at $0",
                                  host_lib_modules_build_dir.string());
  }

  return Status::OK();
}

Status ExtractPackagedHeaders(PackagedLinuxHeadersSpec* headers_package) {
  std::string expected_directory =
      absl::Substitute("/usr/src/linux-headers-$0.$1.$2-pl", headers_package->version.version,
                       headers_package->version.major_rev, headers_package->version.minor_rev);

  // This is a loose check that we don't clobber what we *think* should the output directory.
  // If someone built a tar.gz with an incorrect directory structure, this check wouldn't save us.
  if (fs::Exists(expected_directory).ok()) {
    return error::Internal("Refusing to clobber existing directory");
  }

  // Extract the files.
  ::px::tools::Minitar minitar(headers_package->path.string());
  PL_RETURN_IF_ERROR(minitar.Extract("/"));

  // Update the path to the extracted copy.
  headers_package->path = expected_directory;
  if (!fs::Exists(headers_package->path).ok()) {
    return error::Internal("Package extraction did not result in the expected headers directory");
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

StatusOr<PackagedLinuxHeadersSpec> FindClosestPackagedLinuxHeaders(
    const std::filesystem::path& packaged_headers_root, KernelVersion kernel_version) {
  const std::string kHeaderDirPrefix =
      std::filesystem::path(packaged_headers_root / "linux-headers-").string();
  const std::string_view kHeaderDirSuffix = ".tar.gz";

  PackagedLinuxHeadersSpec selected;

  if (fs::Exists(packaged_headers_root).ok()) {
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
  const std::filesystem::path kPackagedHeadersRoot = "/pl";

  std::filesystem::path lib_modules_build_dir = lib_modules_dir / "build";

  LOG(INFO) << "Attempting to install packaged headers.";

  PL_ASSIGN_OR_RETURN(KernelVersion kernel_version, GetKernelVersion());

  PL_ASSIGN_OR_RETURN(PackagedLinuxHeadersSpec packaged_headers,
                      FindClosestPackagedLinuxHeaders(kPackagedHeadersRoot, kernel_version));
  LOG(INFO) << absl::Substitute("Using packaged header: $0", packaged_headers.path.string());
  PL_RETURN_IF_ERROR(ExtractPackagedHeaders(&packaged_headers));
  PL_RETURN_IF_ERROR(ModifyKernelVersion(packaged_headers.path, kernel_version.code()));
  PL_RETURN_IF_ERROR(ApplyConfigPatches(packaged_headers.path));
  PL_RETURN_IF_ERROR(fs::CreateSymlinkIfNotExists(packaged_headers.path, lib_modules_build_dir));
  LOG(INFO) << absl::Substitute("Successfully installed packaged copy of headers at $0",
                                lib_modules_build_dir.string());
  g_packaged_headers_installed = true;
  return Status::OK();
}

StatusOr<std::filesystem::path> FindOrInstallLinuxHeaders(
    const std::vector<LinuxHeaderStrategy>& attempt_order) {
  PL_ASSIGN_OR_RETURN(std::string uname, GetUname());
  LOG(INFO) << absl::Substitute("Detected kernel release (uname -r): $0", uname);

  std::filesystem::path lib_modules_dir = "/lib/modules/" + uname;
  std::filesystem::path headers_dir;

  // Some strategies require the base directory to be present.
  // This does nothing if the directory already exists.
  PL_RETURN_IF_ERROR(fs::CreateDirectories(lib_modules_dir));

  for (const auto& attempt : attempt_order) {
    // Some attempts require linking or installing headers. Do this first.
    switch (attempt) {
      case LinuxHeaderStrategy::kSearchLocalHeaders:
        // Nothing to link or install.
        break;
      case LinuxHeaderStrategy::kLinkHostHeaders: {
        PL_RETURN_IF_ERROR(LinkHostLinuxHeaders(lib_modules_dir));
      } break;
      case LinuxHeaderStrategy::kInstallPackagedHeaders: {
        PL_RETURN_IF_ERROR(InstallPackagedLinuxHeaders(lib_modules_dir));
        break;
      }
    }

    // Now check for a healthy set of headers.
    headers_dir = FindLinuxHeadersDirectory(lib_modules_dir);
    if (!headers_dir.empty()) {
      return headers_dir;
    }
  }

  return error::Internal("Could not find any linux headers to use.");
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
