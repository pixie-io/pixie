#pragma once

#include <map>
#include <string>
#include "src/common/base/base.h"

#include "src/stirling/bcc_bpf/socket_trace.h"

namespace pl {
namespace stirling {
namespace http_header_keys {

inline constexpr char kContentEncoding[] = "Content-Encoding";
inline constexpr char kContentType[] = "Content-Type";
inline constexpr char kTransferEncoding[] = "Transfer-Encoding";

}  // namespace http_header_keys

enum class ChunkingStatus {
  kUnknown,
  kChunked,
  kComplete,
};

enum class HTTPTraceEventType { kUnknown, kHTTPRequest, kHTTPResponse };

inline std::string EventTypeToString(HTTPTraceEventType event_type) {
  std::string event_type_str;

  switch (event_type) {
    case HTTPTraceEventType::kUnknown:
      event_type_str = "unknown";
      break;
    case HTTPTraceEventType::kHTTPResponse:
      event_type_str = "http_response";
      break;
    case HTTPTraceEventType::kHTTPRequest:
      event_type_str = "http_request";
      break;
    default:
      CHECK(false) << absl::StrFormat("Unrecognized event_type: %d", event_type);
  }

  return event_type_str;
}

// The fields corresponding exactly to HTTPTraceConnector::kElements.
// TODO(yzhao): The repetitions of information among this, DataElementsIndexes, and kElements should
// be eliminated. It might make sense to use proto file to define data schema and generate kElements
// array during runtime, based on proto schema.
struct HTTPTraceRecord {
  uint64_t time_stamp_ns = 0;
  uint32_t tgid = 0;
  uint32_t pid = 0;
  int fd = -1;
  HTTPTraceEventType event_type = HTTPTraceEventType::kUnknown;
  uint64_t http_start_time_stamp_ns = 0;
  std::string src_addr = "-";
  int src_port = -1;
  std::string dst_addr = "-";
  int dst_port = -1;
  int http_minor_version = -1;
  std::map<std::string, std::string> http_headers;
  std::string http_req_method = "-";
  std::string http_req_path = "-";
  int http_resp_status = -1;
  std::string http_resp_message = "-";
  std::string http_resp_body = "-";
  // If true, http_resp_body is an chunked message, therefore incomplete. But it's not
  ChunkingStatus chunking_status = ChunkingStatus::kUnknown;
};

/**
 * @brief Parses the message body assuming it's encoded with 'Transfer-Encoding: chunked'.
 * Writes a bool to indicate if the message body surpasses the end of the entire message.
 *
 * @param record The input and result.
 */
void ParseMessageBodyChunked(HTTPTraceRecord* record);

void PreProcessRecord(HTTPTraceRecord* record);
void ParseEventAttr(const socket_data_event_t& event, HTTPTraceRecord* record);
bool ParseHTTPRequest(const socket_data_event_t& event, HTTPTraceRecord* record);
bool ParseHTTPResponse(const socket_data_event_t& event, HTTPTraceRecord* record);
bool ParseSockAddr(const socket_data_event_t& event, HTTPTraceRecord* record);
bool ParseRaw(const socket_data_event_t& event, HTTPTraceRecord* record);

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

}  // namespace stirling
}  // namespace pl
