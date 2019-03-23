#include "src/common/base/error_strings.h"

namespace pl {
namespace error {

std::string CodeToString(pl::statuspb::Code code) {
  switch (code) {
    case pl::statuspb::OK:
      return "OK";
    case pl::statuspb::CANCELLED:
      return "Cancelled";
    case pl::statuspb::UNKNOWN:
      return "Unknown";
    case pl::statuspb::INVALID_ARGUMENT:
      return "Invalid Argument";
    case pl::statuspb::DEADLINE_EXCEEDED:
      return "Deadline Exceeded";
    case pl::statuspb::NOT_FOUND:
      return "Not Found";
    case pl::statuspb::ALREADY_EXISTS:
      return "Already Exists";
    case pl::statuspb::PERMISSION_DENIED:
      return "Permission Denied";
    case pl::statuspb::UNAUTHENTICATED:
      return "Unauthenticated";
    case pl::statuspb::INTERNAL:
      return "Internal";
    case pl::statuspb::UNIMPLEMENTED:
      return "Unimplemented";
    default: { return absl::Substitute("Unknown error_code($0)", static_cast<int>(code)); }
  }
}

}  // namespace error
}  // namespace pl
