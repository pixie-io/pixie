#pragma once

#include <string>

#include "src/common/base/statusor.h"

namespace pl {

std::string FileContentsOrDie(const std::string& filename);
StatusOr<std::string> ReadFileToString(const std::string& filename);
Status WriteFileFromString(const std::string& filename, std::string_view contents);

}  // namespace pl
