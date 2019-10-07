#pragma once

#include <string>

#include "src/common/base/statusor.h"

namespace pl {
bool FileExists(const std::string& filename);
std::string FileContentsOrDie(const std::string& filename);
StatusOr<std::string> ReadFileToString(const std::string& filename);
Status WriteFileFromString(const std::string& filename, std::string_view contents);
}  // namespace pl
