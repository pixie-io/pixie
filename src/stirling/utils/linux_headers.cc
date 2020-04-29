#ifdef __linux__

#include "src/stirling/utils/linux_headers.h"

#include <sys/utsname.h>

#include <limits>
#include <sstream>
#include <vector>

#include <absl/strings/numbers.h>

#include "src/common/base/file.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"

namespace pl {
namespace stirling {
namespace utils {

StatusOr<KernelVersion> ParseKernelVersionString(const std::string& linux_release_str) {
  KernelVersion kernel_version;

  std::regex uname_regex("^([0-9]+)\\.([0-9]+)\\.([0-9]+)");
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

StatusOr<KernelVersion> GetKernelVersion() {
  // First option is to check /proc/version_signature, which exists on Ubuntu distributions.
  // This has to be higher priority because uname -r does not include the minor rev number with
  // uname.
  StatusOr<std::string> version_string_status = GetProcVersionSignature();

  // Second option is to use `uname -r`.
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
  std::regex e("#define LINUX_VERSION_CODE ([0-9]*)");
  std::string new_file_contents = std::regex_replace(file_contents, e, linux_version_code_override);

  // Write the modified file back.
  PL_RETURN_IF_ERROR(WriteFileFromString(version_file_path, new_file_contents));

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

StatusOr<std::filesystem::path> FindClosestPackagedHeader(
    std::filesystem::path packaged_headers_root, KernelVersion kernel_version) {
  const std::string kHeaderDirPrefix =
      std::filesystem::path(packaged_headers_root / "linux-headers-").string();
  const std::string_view kHeaderDirSuffix = "-pl";

  std::filesystem::path selected_headers;
  KernelVersion selected_headers_version;

  if (fs::Exists(packaged_headers_root).ok()) {
    for (const auto& p : std::filesystem::directory_iterator(packaged_headers_root)) {
      VLOG(1) << absl::Substitute("Directory: $0", p.path().string());
      std::string path = p.path().string();

      if (!absl::StartsWith(path, kHeaderDirPrefix) || !absl::EndsWith(path, kHeaderDirSuffix)) {
        LOG(DFATAL) << absl::Substitute(
            "Did not expect anything but Pixie headers in this directory. Found $0", path);
        continue;
      }

      std::string_view version_string_view(path);
      version_string_view.remove_prefix(kHeaderDirPrefix.size());
      version_string_view.remove_suffix(kHeaderDirSuffix.size());
      std::string version_string(version_string_view);

      StatusOr<KernelVersion> headers_kernel_version_status =
          ParseKernelVersionString(version_string);
      if (!headers_kernel_version_status.ok()) {
        LOG(DFATAL) << absl::Substitute(
            "Did not expect an unparseable linux-headers format in this directory. Found $0", path);
        continue;
      }
      KernelVersion headers_kernel_version = headers_kernel_version_status.ValueOrDie();

      if (selected_headers.empty() ||
          KernelHeadersDistance(kernel_version, headers_kernel_version) <
              KernelHeadersDistance(kernel_version, selected_headers_version)) {
        selected_headers = p;
        selected_headers_version = headers_kernel_version;
      }
    }
  }

  if (selected_headers.empty()) {
    return error::Internal("Could not find packaged headers to install. Search location: $0",
                           packaged_headers_root.string());
  }

  return selected_headers;
}

Status InstallPackagedLinuxHeaders(const std::filesystem::path& lib_modules_dir) {
  // This is the directory in our container images that contains packaged linux headers.
  const std::filesystem::path kPackagedHeadersRoot = "/usr/src";

  std::filesystem::path lib_modules_build_dir = lib_modules_dir / "build";

  LOG(INFO) << absl::Substitute("Attempting to install packaged headers to $0",
                                lib_modules_build_dir.string());

  PL_ASSIGN_OR_RETURN(KernelVersion kernel_version, GetKernelVersion());

  PL_ASSIGN_OR_RETURN(std::filesystem::path packaged_headers,
                      FindClosestPackagedHeader(kPackagedHeadersRoot, kernel_version));
  PL_RETURN_IF_ERROR(ModifyKernelVersion(packaged_headers, kernel_version.code()));
  PL_RETURN_IF_ERROR(fs::CreateSymlinkIfNotExists(packaged_headers, lib_modules_build_dir));
  LOG(INFO) << absl::Substitute("Successfully installed packaged copy of headers at $0",
                                lib_modules_build_dir.string());
  return Status::OK();
}

Status FindOrInstallLinuxHeaders(const std::vector<LinuxHeaderStrategy>& attempt_order) {
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
      LOG(INFO) << absl::Substitute("Using linux headers found at $0", headers_dir.string());
      return Status::OK();
    }
  }

  return error::Internal("Could not find any linux headers to use.");
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl

#endif
