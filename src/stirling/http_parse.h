#pragma once

#include <picohttpparser.h>

#include <chrono>
#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/socket_trace.h"
#include "src/stirling/event_parser.h"
#include "src/stirling/utils/req_resp_pair.h"

namespace pl {
namespace stirling {
namespace http {

inline constexpr char kContentEncoding[] = "Content-Encoding";
inline constexpr char kContentLength[] = "Content-Length";
inline constexpr char kContentType[] = "Content-Type";
inline constexpr char kTransferEncoding[] = "Transfer-Encoding";
inline constexpr char kUpgrade[] = "Upgrade";

// TODO(yzhao): The repetitions of information among HTTPMessage + ConnectionTraceRecord,
// DataElementsIndexes, and kTables should be eliminated. It might make sense to use proto file
// to define data schema and generate kTables array during runtime, based on proto schema.

using HTTPHeadersMap = std::map<std::string, std::string, CaseInsensitiveLess>;

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

using Record = ReqRespPair<HTTPMessage, HTTPMessage>;

void PreProcessMessage(HTTPMessage* message);

// For each HTTP message, inclusions are applied first; then exclusions, which can overturn the
// selection done by the former. An empty inclusions results into any HTTP message being selected,
// and an empty exclusions results into any HTTP message not being excluded.
struct HTTPHeaderFilter {
  std::multimap<std::string_view, std::string_view> inclusions;
  std::multimap<std::string_view, std::string_view> exclusions;
};

/**
 * @brief Parses a string the describes filters on HTTP headers. The exact format is described in
 * --http_response_header_filters's definition. Note that std::multimap<> is used to allow
 * conjunctive selection on the value of a header.
 *
 * @param filters The string that encodes the filters.
 */
HTTPHeaderFilter ParseHTTPHeaderFilters(std::string_view filters);

/**
 * @brief Returns true if the header matches any of the filters. A filter is considered match
 * if the http header's value contains one of the filter's values as substring.
 *
 * @param http_headers The HTTP headers of a HTTP message. The key is the header names,
 * and the value is the value of the header.
 * @param filters The filter on HTTP headers. The key is the header names, and the value is a
 * substring that the header value should contain.
 */
bool MatchesHTTPTHeaders(const HTTPHeadersMap& http_headers, const HTTPHeaderFilter& filter);

}  // namespace http

/**
 * @brief Parses the input string as a sequence of HTTP responses, writes the messages in result.
 *
 * @return ParseState To indicate the final state of the parsing. The second return value is the
 * bytes count of the parsed data.
 */
template <>
ParseResult<size_t> Parse(MessageType type, std::string_view buf,
                          std::deque<http::HTTPMessage>* messages);

template <>
size_t FindMessageBoundary<http::HTTPMessage>(MessageType type, std::string_view buf,
                                              size_t start_pos);

}  // namespace stirling
}  // namespace pl
