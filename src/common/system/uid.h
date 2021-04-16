#pragma once

#include <sys/types.h>

#include <map>
#include <string>
#include <vector>

#include "src/common/base/statusor.h"

namespace px {

StatusOr<std::string> NameForUID(uid_t uid);

/**
 * Returns a map from UID to username by parsing the content of /etc/passwd file or an equivalent
 * file at different path.
 */
std::map<uid_t, std::string> ParsePasswd(std::string_view passwd_content);

}  // namespace px
