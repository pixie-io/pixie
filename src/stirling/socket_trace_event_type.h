#pragma once

#include <string>

namespace pl {
namespace stirling {

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

enum class MySQLEventType {
  kMySQLUnknown,
  kMySQLComStmtPrepare,
  kMySQLComStmtExecute,
  kMySQLComQuery
};

}  // namespace stirling
}  // namespace pl
