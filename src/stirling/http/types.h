#pragma once

#include <chrono>
#include <string>

#include "src/common/base/utils.h"
#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/common/event_parser.h"  // For FrameBase
#include "src/stirling/utils/req_resp_pair.h"

namespace pl {
namespace stirling {
namespace http {

//-----------------------------------------------------------------------------
// HTTP Message
//-----------------------------------------------------------------------------

// HTTP1.x headers can have multiple values for the same name, and field names are case-insensitive:
// https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
using HeadersMap = std::multimap<std::string, std::string, CaseInsensitiveLess>;

inline constexpr char kContentEncoding[] = "Content-Encoding";
inline constexpr char kContentLength[] = "Content-Length";
inline constexpr char kContentType[] = "Content-Type";
inline constexpr char kTransferEncoding[] = "Transfer-Encoding";
inline constexpr char kUpgrade[] = "Upgrade";

struct Message : public stirling::FrameBase {
  MessageType type = MessageType::kUnknown;

  int http_minor_version = -1;
  HeadersMap http_headers = {};

  std::string http_req_method = "-";
  std::string http_req_path = "-";

  int http_resp_status = -1;
  std::string http_resp_message = "-";

  std::string http_msg_body = "-";

  // TODO(yzhao): We should enforce that Message size does not change after certain point,
  // so that we can cache this value.
  size_t ByteSize() const override {
    size_t headers_size = 0;
    for (const auto& [name, val] : http_headers) {
      headers_size += name.size();
      headers_size += val.size();
    }
    return sizeof(Message) + headers_size + http_req_method.size() + http_req_path.size() +
           http_resp_message.size() + http_msg_body.size();
  }
};

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

/**
 *  Record is the primary output of the http stitcher.
 */
using Record = ReqRespPair<Message, Message>;

}  // namespace http
}  // namespace stirling
}  // namespace pl
