#pragma once

#ifdef __linux__

#include <filesystem>
#include <string>
#include <vector>

#include "src/common/base/base.h"

namespace pl {
namespace fs {

// These are wrappers around std::filesystem functions to convert error codes to Status.
// More functions should be added as needed.

Status CreateSymlink(std::filesystem::path target, std::filesystem::path link);

Status CreateDirectories(std::filesystem::path dir);

pl::StatusOr<std::filesystem::path> ReadSymlink(std::filesystem::path symlink);

/**
 * Joins multiple paths together.
 *
 * Note that unlike std::filesystem's operator/, all arguments are treated as relative paths.
 * For example, assuming:
 *   std::filesystem::path a = '/path/to/a';
 *   std::filesystem::path b = '/path/to/b';
 * JoinPath({a, b}) returns /path/to/a/path/to/b.
 * In contrast, a/b (std::filesystem::path's operator/) would return /path/to/b.
 */
std::filesystem::path JoinPath(const std::vector<const std::filesystem::path*>& paths);

// Designed for use in test code only.
Status CreateSymlinkIfNotExists(std::filesystem::path target, std::filesystem::path link);

/**
 * Returns OK if the path exists. Returns error if does not exist, or failed to detect
 * (for example, because of lack of permission).
 */
Status Exists(std::filesystem::path path);

}  // namespace fs
}  // namespace pl

#endif
