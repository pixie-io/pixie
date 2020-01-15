#ifdef __linux__

#include "src/stirling/utils/linux_headers.h"

#include <sys/utsname.h>

#include <sstream>
#include <vector>

#include <absl/strings/numbers.h>

#include "src/common/base/file.h"
#include "src/common/fs/fs_wrapper.h"

namespace pl {
namespace stirling {
namespace utils {

StatusOr<std::string> GetUname() {
  // The following effectively runs `uname -r`.
  struct utsname buffer;
  if (uname(&buffer) != 0) {
    LOG(ERROR) << "Could not determine kernel version";
    return error::Internal("Could not determine kernel version (uname -r)");
  }
  return std::string(buffer.release);
}

StatusOr<uint32_t> ParseUname(const std::string& linux_release) {
  uint32_t kernel_version;
  uint32_t major_rev;
  uint32_t minor_rev;

  std::regex uname_regex("^([0-9]+)\\.([0-9]+)\\.([0-9]+)");
  std::smatch match;
  bool s = std::regex_search(linux_release, match, uname_regex);
  if (!s || match.size() != 4) {
    return error::Internal("Could not parse uname: [status=$0, match_size=$1]", s, match.size());
  }

  // SimpleAtoi should always succeed, due to regex match.
  ECHECK(absl::SimpleAtoi(match[1].str(), &kernel_version));
  ECHECK(absl::SimpleAtoi(match[2].str(), &major_rev));
  ECHECK(absl::SimpleAtoi(match[3].str(), &minor_rev));

  uint32_t version_code = (kernel_version << 16) | (major_rev << 8) | (minor_rev);
  return version_code;
}

Status ModifyKernelVersion(const std::filesystem::path& linux_headers_base,
                           const std::string& linux_release) {
  std::filesystem::path version_file_path =
      linux_headers_base / "include/generated/uapi/linux/version.h";

  // Read the file into a string.
  PL_ASSIGN_OR_RETURN(std::string file_contents, ReadFileToString(version_file_path));

  // Modify the version code.
  PL_ASSIGN_OR_RETURN(uint32_t linux_version_code, ParseUname(linux_release));
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
  if (std::filesystem::exists(lib_modules_source_dir)) {
    lib_modules_kdir = lib_modules_source_dir;
  } else if (std::filesystem::exists(lib_modules_build_dir)) {
    lib_modules_kdir = lib_modules_build_dir;
  }

  return lib_modules_kdir;
}

Status LinkHostLinuxHeaders(const std::filesystem::path& lib_modules_dir) {
  // Host dir is where we must mount host directories into the container.
  const std::filesystem::path kHostDir = "/host";

  std::filesystem::path host_lib_modules_dir = kHostDir / lib_modules_dir;
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
    host_lib_modules_source_dir =
        kHostDir / std::filesystem::read_symlink(host_lib_modules_source_dir, ec);
    ECHECK(!ec);
  }
  if (std::filesystem::is_symlink(host_lib_modules_build_dir, ec)) {
    host_lib_modules_build_dir =
        kHostDir / std::filesystem::read_symlink(host_lib_modules_build_dir, ec);
    ECHECK(!ec);
  }

  VLOG(1) << absl::Substitute("source_dir $0", host_lib_modules_source_dir.string());
  VLOG(1) << absl::Substitute("build_dir $0", host_lib_modules_build_dir.string());

  if (std::filesystem::exists(host_lib_modules_source_dir)) {
    PL_RETURN_IF_ERROR(fs::CreateSymlink(host_lib_modules_source_dir, lib_modules_source_dir));
    LOG(INFO) << absl::Substitute("Linked linux headers found at $0",
                                  host_lib_modules_source_dir.string());
  }

  if (std::filesystem::exists(host_lib_modules_build_dir)) {
    PL_RETURN_IF_ERROR(fs::CreateSymlink(host_lib_modules_build_dir, lib_modules_build_dir));
    LOG(INFO) << absl::Substitute("Linked linux headers found at $0",
                                  host_lib_modules_build_dir.string());
  }

  return Status::OK();
}

Status InstallPackagedLinuxHeaders(const std::filesystem::path& lib_modules_dir,
                                   const std::string& uname) {
  std::filesystem::path lib_modules_build_dir = lib_modules_dir / "build";

  LOG(INFO) << absl::Substitute("Attempting to install packaged headers to $0",
                                lib_modules_build_dir.string());

  // TODO(oazizi): /usr/src/linux-headers-4.14.104-pl is tied to our container build. Too brittle.
  std::filesystem::path packaged_headers = "/usr/src/linux-headers-4.14.104-pl";
  LOG(INFO) << absl::Substitute("Looking for packaged headers at $0", packaged_headers.string());
  if (std::filesystem::exists(packaged_headers)) {
    PL_RETURN_IF_ERROR(ModifyKernelVersion(packaged_headers, uname));
    PL_RETURN_IF_ERROR(fs::CreateSymlink(packaged_headers, lib_modules_build_dir));
    LOG(INFO) << "Successfully installed packaged copy of headers.";
    return Status::OK();
  }

  return error::Internal("Could not find packaged headers to install. Search location: $0",
                         packaged_headers.string());
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
        PL_RETURN_IF_ERROR(InstallPackagedLinuxHeaders(lib_modules_dir, uname));
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
