#pragma once

#include <chrono>
#include <map>
#include <string>

#include "src/common/base/utils.h"

namespace pl {
namespace stirling {
namespace http {

// HTTP1.x headers can have multiple values for the same name, and field names are case-insensitive:
// https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
using HTTPHeadersMap = std::multimap<std::string, std::string, CaseInsensitiveLess>;

struct HTTPMessage {
  uint64_t timestamp_ns;
  std::chrono::time_point<std::chrono::steady_clock> creation_timestamp;
  MessageType type = MessageType::kUnknown;

  int http_minor_version = -1;
  HTTPHeadersMap http_headers = {};

  std::string http_req_method = "-";
  std::string http_req_path = "-";

  int http_resp_status = -1;
  std::string http_resp_message = "-";

  std::string http_msg_body = "-";

  // TODO(yzhao): We should enforce that HTTPMessage size does not change after certain point,
  // so that we can cache this value.
  size_t ByteSize() const {
    size_t headers_size = 0;
    for (const auto& [name, val] : http_headers) {
      headers_size += name.size();
      headers_size += val.size();
    }
    return sizeof(HTTPMessage) + headers_size + http_req_method.size() + http_req_path.size() +
           http_resp_message.size() + http_msg_body.size();
  }
};

}  // namespace http
}  // namespace stirling
}  // namespace pl
