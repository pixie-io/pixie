#pragma once

#include <sys/types.h>

#include <string>

#include "src/common/base/statusor.h"

namespace pl {

StatusOr<std::string> NameForUID(uid_t uid);

}  // namespace pl
