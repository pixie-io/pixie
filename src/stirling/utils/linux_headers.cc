#ifdef __linux__

#include <sys/utsname.h>
#include <fstream>
#include <sstream>

#include "absl/strings/numbers.h"
#include "src/common/base/file.h"
#include "src/stirling/utils/fs_wrapper.h"
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
  // The following effectively runs `uname -r`.
  struct utsname buffer;
  if (uname(&buffer) != 0) {
    LOG(ERROR) << "Could not determine kernel version";
    return error::Internal("Could not determine kernel version (uname -r)");
  }
  std::string uname = buffer.release;
  LOG(INFO) << absl::Substitute("Detected kernel release (name -r): $0", uname);

  // /lib/modules/<uname>/build is where BCC looks for Linux headers.
  fs::path lib_modules_dir = "/lib/modules/" + uname;
  fs::path lib_modules_build_dir = lib_modules_dir / "build";

  // Case 1: Linux headers are already accessible (we must be running directly on host).
  if (fs::exists(lib_modules_build_dir)) {
    LOG(INFO) << absl::Substitute("Using linux headers found at $0",
                                  lib_modules_build_dir.string());
    return Status::OK();
  }

  // Case 2: Linux headers found in /host (we must be running in a container).
  fs::path mounted_usr_src_headers = "/host/usr/src/linux-headers-" + uname;
  LOG(INFO) << absl::Substitute("Looking for host mounted headers at $0",
                                mounted_usr_src_headers.string());
  if (fs::exists(mounted_usr_src_headers)) {
    PL_RETURN_IF_ERROR(CreateDirectories(lib_modules_dir));
    PL_RETURN_IF_ERROR(CreateSymlink(mounted_usr_src_headers, lib_modules_build_dir));
    LOG(INFO) << absl::Substitute("Using linked linux headers found at $0",
                                  mounted_usr_src_headers.string());
    return Status::OK();
  }

  // Case 3: No Linux headers found (we may or may not be running in container).
  // Try to install packaged headers (will only work if we're in a container image with packaged
  // headers).
  // TODO(oazizi): /usr/src/linux-headers-4.14.104-pl is tied to our container build. Too brittle.
  fs::path packaged_headers = "/usr/src/linux-headers-4.14.104-pl";
  LOG(INFO) << absl::Substitute("Looking for packed headers at $0", packaged_headers.string());
  if (fs::exists(packaged_headers)) {
    PL_RETURN_IF_ERROR(ModifyKernelVersion(packaged_headers, uname));
    PL_RETURN_IF_ERROR(CreateDirectories(lib_modules_dir));
    PL_RETURN_IF_ERROR(CreateSymlink(packaged_headers, lib_modules_build_dir));
    LOG(INFO) << "Successfully installed packaged copy of headers.";
    return Status::OK();
  }

  return error::Internal("Could not find packaged headers to install. Search location: $0",
                         packaged_headers.string());
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl

#endif
