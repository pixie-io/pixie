#ifdef __linux__

#include <sys/utsname.h>
#include <fstream>
#include <sstream>

#include "absl/strings/numbers.h"
#include "src/common/base/file.h"
#include "src/stirling/utils/linux_headers.h"

namespace pl {
namespace stirling {
namespace utils {

namespace fs = std::experimental::filesystem;

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
  CHECK(absl::SimpleAtoi(match[1].str(), &kernel_version));
  CHECK(absl::SimpleAtoi(match[2].str(), &major_rev));
  CHECK(absl::SimpleAtoi(match[3].str(), &minor_rev));

  uint32_t version_code = (kernel_version << 16) | (major_rev << 8) | (minor_rev);
  return version_code;
}

Status ModifyKernelVersion(const fs::path& linux_headers_base, const std::string& linux_release) {
  fs::path version_file_path = linux_headers_base / "include/generated/uapi/linux/version.h";

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

Status FindOrInstallLinuxHeaders() {
  std::error_code ec;

  // The following effectively runs `uname -r`.
  struct utsname buffer;
  if (uname(&buffer) != 0) {
    LOG(ERROR) << "Could not determine kernel version";
    return error::Internal("Could not determine kernel version (uname -r)");
  }
  std::string uname = buffer.release;
  LOG(INFO) << absl::Substitute("Detected kernel release (name -r): $0", uname);

  // TODO(oazizi): linux_headers_pl is tied to our packaged headers. Too brittle.
  fs::path linux_headers_pl = "/usr/src/linux-headers-4.14.104-pl";
  fs::path linux_headers_host = "/usr/src/linux-headers-" + uname;
  fs::path lib_modules_dir = "/lib/modules";
  lib_modules_dir = lib_modules_dir / uname;

  if (fs::exists(linux_headers_host)) {
    LOG(INFO) << absl::Substitute("Using linux headers found at $0", linux_headers_host.string());
    return Status::OK();
  }

  LOG(INFO) << "No host linux headers found. Looking to see if packaged copy exists.";
  if (!fs::exists(linux_headers_pl)) {
    return error::Internal("Could not find packaged headers to install. Search location: $0",
                           linux_headers_pl.string());
  }

  PL_RETURN_IF_ERROR(ModifyKernelVersion(linux_headers_pl, uname));

  fs::create_symlink(linux_headers_pl, linux_headers_host, ec);
  if (ec) {
    return error::Internal("Failed to create symlink $0 -> $1. Message: $2",
                           linux_headers_host.string(), linux_headers_pl.string(), ec.message());
  }

  fs::create_directories(lib_modules_dir, ec);
  if (ec) {
    return error::Internal("Failed to create directory $0. Message: $0", lib_modules_dir.string(),
                           ec.message());
  }

  lib_modules_dir = lib_modules_dir / "build";
  fs::create_symlink(linux_headers_host, lib_modules_dir.string(), ec);
  if (ec) {
    return error::Internal("Failed to create symlink $0 -> $1. Message: $0",
                           lib_modules_dir.string(), linux_headers_host.string(), ec.message());
  }
  LOG(INFO) << "Successfully installed packaged copy of headers.";
  return Status::OK();
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl

#endif
