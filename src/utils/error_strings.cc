#include "src/utils/error_strings.h"

namespace pl {
namespace error {

std::string CodeToString(pl::error::Code code) {
  switch (code) {
    case pl::error::OK:
      return "OK";
    case pl::error::CANCELLED:
      return "Cancelled";
    case pl::error::UNKNOWN:
      return "Unknown";
    case pl::error::INVALID_ARGUMENT:
      return "Invalid Argument";
    case pl::error::DEADLINE_EXCEEDED:
      return "Deadline Exceeded";
    case pl::error::NOT_FOUND:
      return "Not Found";
    case pl::error::ALREADY_EXISTS:
      return "Already Exists";
    case pl::error::PERMISSION_DENIED:
      return "Permission Denied";
    case pl::error::UNAUTHENTICATED:
      return "Unauthenticated";
    case pl::error::INTERNAL:
      return "Internal";
    case pl::error::UNIMPLEMENTED:
      return "Unimplemented";
    default: { return absl::Substitute("Unknown error_code($0)", static_cast<int>(code)); }
  }
}

}  // namespace error
}  // namespace pl
