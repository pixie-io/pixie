#pragma once

#ifdef __linux__

#include <filesystem>
#include <string>
#include <vector>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace utils {

/**
 * Parses a the Linux version code from a uname release string (e.g. 'uname -r').
 *
 * @param linux_release the uname string (see uname() from <sys/utsname.h>).
 * @return Linux version code, as used in the linux source code (see version.h).
 */
StatusOr<uint32_t> VersionStringToCode(const std::string& linux_release);

/**
 * Returns the kernel version from /proc/version_signature.
 * This is required for Ubuntu, which does not report the correct minor version through uname.
 * Other distributions do not have /proc/version signature.
 *
 * @return Kernel release version (e.g. 4.15.4)
 */
StatusOr<std::string> GetProcVersionSignature();

/**
 * Extracts the linux version code integer.
 * It first searches /proc/version_signature (for Ubuntu distros).
 * If /proc/version_signature does not exist, it uses uname.
 *
 * Note that the version number reported by uname on Ubuntu distros does not include the minor
 * version, and thus cannot be used to create a version code.
 *
 * @return The version code as used in the linux source code, or error if it could not be
 * determined.
 */
StatusOr<uint32_t> LinuxVersionCode();

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
Status ModifyKernelVersion(const std::filesystem::path& linux_headers_base);

enum class LinuxHeaderStrategy {
  // Search for linux Linux headers are already accessible (must be running directly on host).
  kSearchLocalHeaders,

  // Search for Linux headers under /host (must be running in our own container).
  kLinkHostHeaders,

  // Try to install packaged headers (only works if in a container image with packaged headers).
  // Useful in case no Linux headers are found.
  kInstallPackagedHeaders
};

inline const std::vector<LinuxHeaderStrategy> kDefaultHeaderSearchOrder = {
    utils::LinuxHeaderStrategy::kSearchLocalHeaders, utils::LinuxHeaderStrategy::kLinkHostHeaders,
    utils::LinuxHeaderStrategy::kInstallPackagedHeaders};

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
 * @param attempt_order Provides the ordered list of strategies to use to find the Linux headers.
 * See LinuxHeaderStrategy enum.
 *
 * @return Status error if no headers (either host headers or installed packaged headers) are
 * available in the end state.
 */
Status FindOrInstallLinuxHeaders(const std::vector<LinuxHeaderStrategy>& attempt_order);

}  // namespace utils
}  // namespace stirling
}  // namespace pl

#endif
