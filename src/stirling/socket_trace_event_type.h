#pragma once

#include <string>

namespace pl {
namespace stirling {

// TODO(yzhao): This seems duplicate with MessageType in event_parser.h. Maybe merge the two and put
// the final type inside this header; or remove this header.
enum class HTTPEventType { kUnknown, kHTTPRequest, kHTTPResponse };

inline std::string HTTPEventTypeToString(HTTPEventType event_type) {
  std::string event_type_str;

  switch (event_type) {
    case HTTPEventType::kUnknown:
      event_type_str = "unknown";
      break;
    case HTTPEventType::kHTTPResponse:
      event_type_str = "http_response";
      break;
    case HTTPEventType::kHTTPRequest:
      event_type_str = "http_request";
      break;
    default:
      CHECK(false) << absl::StrFormat("Unrecognized event_type: %d", event_type);
  }

  return event_type_str;
}

}  // namespace stirling
}  // namespace pl
