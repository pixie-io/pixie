#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "src/common/base/base.h"

namespace pl {
namespace system {

/**
 * This function dumps out general information about the system that could be useful
 * for debugging in different environments.
 */
void LogSystemInfo();

}  // namespace system
}  // namespace pl
