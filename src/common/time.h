#pragma once

#include <regex>
#include <string>

#include "absl/strings/str_format.h"
#include "src/common/error.h"
#include "src/common/statusor.h"

namespace pl {

inline StatusOr<int64_t> StringToTimeInt(const std::string& str_time) {
  std::regex rgx("([-]?[0-9]+)(ms|m|s|h|d)");
  std::smatch matches;
  if (std::regex_search(str_time, matches, rgx)) {
    // TODO(michelle): PL-403 - Fix potential buffer overflow here when matches < 2.
    auto amount = std::stoi(matches[1]);
    auto unit = matches[2];

    if (unit == "h") {
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(amount))
          .count();
    } else if (unit == "m") {
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::minutes(amount))
          .count();
    } else if (unit == "ms") {
      return std::chrono::milliseconds(amount).count();
    } else if (unit == "s") {
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(amount))
          .count();
    } else if (unit == "d") {
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(amount * 24))
          .count();
    } else {
      return error::InvalidArgument("Unsupported unit.");
    }
  } else {
    return error::InvalidArgument("Time string is in wrong format.");
  }
}

/**
 * Print a duration with a suffix of us, ms, s.
 *
 * This makes times smaller than 0.5ms print as us. Then print with ms till 1 second.
 * @param duration_ns
 * @return
 */
inline std::string PrettyDuration(double duration_ns) {
  if (duration_ns < 500000) {
    return absl::StrFormat("%.2f \u03BCs", duration_ns / 1E3);
  } else if (duration_ns < 1E9) {
    return absl::StrFormat("%.2f ms", duration_ns / 1E6);
  }
  return absl::StrFormat("%.2f s", duration_ns / 1E9);
}

}  // namespace pl
