#pragma once

#include <regex>
#include <string>

#include "src/common/error.h"
#include "src/common/statusor.h"

namespace pl {

StatusOr<int64_t> StringToTimeInt(const std::string& str_time) {
  std::regex rgx("([-]?[0-9]+)(ms|m|s|h|d)");
  std::smatch matches;
  if (std::regex_search(str_time, matches, rgx)) {
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

}  // namespace pl
