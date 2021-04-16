#pragma once

#include <string>

#include "src/common/base/statusor.h"

namespace px {

/**
 * Executes the provided command on the system.
 *
 * @param cmd Command to execute.
 * @return Output (stdout) as a string.
 */
StatusOr<std::string> Exec(std::string cmd);

}  // namespace px
