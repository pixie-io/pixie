#pragma once

#include <picohttpparser.h>

#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/event_parser.h"

namespace pl {
namespace stirling {
namespace http {

// TODO(yzhao): HTTP/{1.x,2} headers are case insensitive. Our code does not work that way.
// We'll need to update all code paths to match that.
inline constexpr char kContentEncoding[] = "Content-Encoding";
inline constexpr char kContentLength[] = "Content-Length";
inline constexpr char kContentType[] = "Content-Type";
inline constexpr char kTransferEncoding[] = "Transfer-Encoding";

// TODO(yzhao): The repetitions of information among HTTPMessage + ConnectionTraceRecord,
// DataElementsIndexes, and kTables should be eliminated. It might make sense to use proto file
// to define data schema and generate kTables array during runtime, based on proto schema.

struct HTTPMessage {
  uint64_t timestamp_ns;
  MessageType type = MessageType::kUnknown;

  int http_minor_version = -1;
  std::map<std::string, std::string> http_headers = {};
  // -1 indicates this message does not have 'Content-Length' header.
  int content_length = -1;

  std::string http_req_method = "-";
  std::string http_req_path = "-";

  int http_resp_status = -1;
  std::string http_resp_message = "-";

  std::string http_msg_body = "-";
};

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
bool MatchesHTTPTHeaders(const std::map<std::string, std::string>& http_headers,
                         const HTTPHeaderFilter& filter);

struct PicoHTTPParserWrapper {
  ParseState Parse(MessageType type, std::string_view buf) {
    switch (type) {
      case MessageType::kRequest:
        return ParseRequest(buf);
      case MessageType::kResponse:
        return ParseResponse(buf);
      default:
        return ParseState::kInvalid;
    }
  }
  ParseState Write(MessageType type, HTTPMessage* result) {
    switch (type) {
      case MessageType::kRequest:
        return WriteRequest(result);
      case MessageType::kResponse:
        return WriteResponse(result);
      default:
        return ParseState::kUnknown;
    }
  }
  ParseState ParseRequest(std::string_view buf);
  ParseState WriteRequest(HTTPMessage* result);
  ParseState ParseResponse(std::string_view buf);
  ParseState WriteResponse(HTTPMessage* result);
  ParseState WriteBody(HTTPMessage* result);

  // For parsing HTTP requests.
  const char* method = nullptr;
  size_t method_len;
  const char* path = nullptr;
  size_t path_len;

  // For parsing HTTP responses.
  const char* msg = nullptr;
  size_t msg_len = 0;
  int status = 0;

  // For parsing HTTP requests/response (common).
  int minor_version = 0;
  static constexpr size_t kMaxNumHeaders = 50;
  struct phr_header headers[kMaxNumHeaders];

  std::map<std::string, std::string> header_map;
  std::string_view unparsed_data;
};

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
