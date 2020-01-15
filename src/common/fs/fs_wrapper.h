#pragma once

#ifdef __linux__

#include <filesystem>

#include "src/common/base/base.h"

namespace pl {
namespace fs {

// These are wrappers around std::filesystem functions to convert error codes to Status.
// More functions should be added as needed.

Status CreateSymlink(std::filesystem::path target, std::filesystem::path link);

Status CreateDirectories(std::filesystem::path dir);

pl::StatusOr<std::filesystem::path> ReadSymlink(std::filesystem::path symlink);

}  // namespace fs
}  // namespace pl

#endif
