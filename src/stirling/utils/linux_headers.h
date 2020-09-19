#pragma once

#ifdef __linux__

#include <filesystem>
#include <string>
#include <vector>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace utils {

struct KernelVersion {
  uint16_t version = 0;
  uint8_t major_rev = 0;
  uint8_t minor_rev = 0;

  uint32_t code() { return (version << 16) | (major_rev << 8) | (minor_rev); }
};

uint64_t KernelHeadersDistance(KernelVersion a, KernelVersion b);

/**
 * Parses a the Linux version code from a string.
 *
 * @param linux_release the kernel version string (e.g. 4.15.8-foobar).
 * @return Linux version as {version, major, minor}.
 */
StatusOr<KernelVersion> ParseKernelVersionString(const std::string& linux_release);

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
 * It first searches /proc/version_signature (for Ubuntu distros).
 * If /proc/version_signature does not exist, it uses uname.
 *
 * Note that the version number reported by uname on Ubuntu distros does not include the minor
 * version, and thus cannot be used to get an accurate version.
 *
 * @return The kernel version, or error if it could not be determined.
 */
StatusOr<KernelVersion> GetKernelVersion();

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
StatusOr<std::filesystem::path> FindOrInstallLinuxHeaders(
    const std::vector<LinuxHeaderStrategy>& attempt_order);

}  // namespace utils
}  // namespace stirling
}  // namespace pl

#endif
