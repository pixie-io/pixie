#pragma once

#include <string>

namespace pl {
namespace stirling {

enum class SocketTraceEventType { kUnknown, kHTTPRequest, kHTTPResponse };

inline std::string EventTypeToString(SocketTraceEventType event_type) {
  std::string event_type_str;

  switch (event_type) {
    case SocketTraceEventType::kUnknown:
      event_type_str = "unknown";
      break;
    case SocketTraceEventType::kHTTPResponse:
      event_type_str = "http_response";
      break;
    case SocketTraceEventType::kHTTPRequest:
      event_type_str = "http_request";
      break;
    default:
      CHECK(false) << absl::StrFormat("Unrecognized event_type: %d", event_type);
  }

  return event_type_str;
}

}  // namespace stirling
}  // namespace pl
