#pragma once

#ifdef __linux__

#include <experimental/filesystem>

#include "src/common/base/base.h"
#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace utils {

// These are wrappers around std::filesystem functions to convert error codes to Status.
// More functions should be added as needed.

Status CreateSymlink(std::experimental::filesystem::path target,
                     std::experimental::filesystem::path link);
Status CreateDirectories(std::experimental::filesystem::path dir);
pl::StatusOr<std::experimental::filesystem::path> ReadSymlink(std::experimental::filesystem::path symlink);

}  // namespace utils
}  // namespace stirling
}  // namespace pl

#endif
