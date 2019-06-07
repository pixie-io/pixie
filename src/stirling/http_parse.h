#pragma once

#include <picohttpparser.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/socket_connection.h"
#include "src/stirling/socket_trace_event_type.h"

namespace pl {
namespace stirling {
namespace http_headers {

inline constexpr char kContentEncoding[] = "Content-Encoding";
inline constexpr char kContentLength[] = "Content-Length";
inline constexpr char kContentType[] = "Content-Type";
inline constexpr char kTransferEncoding[] = "Transfer-Encoding";

}  // namespace http_headers

// TODO(yzhao): The repetitions of information among HTTPMessage + ConnectionTraceRecord,
// DataElementsIndexes, and kTables should be eliminated. It might make sense to use proto file
// to define data schema and generate kTables array during runtime, based on proto schema.

struct HTTPMessage {
  bool is_complete = false;

  // Only meaningful is is_chunked is true.
  phr_chunked_decoder chunk_decoder = {};

  uint64_t timestamp_ns;
  SocketTraceEventType type = SocketTraceEventType::kUnknown;

  int http_minor_version = -1;
  std::map<std::string, std::string> http_headers = {};
  // -1 indicates this message does not have 'Content-Length' header.
  int content_length = -1;
  bool is_chunked = false;

  std::string http_req_method = "-";
  std::string http_req_path = "-";
  std::string http_req_body = "-";

  int http_resp_status = -1;
  std::string http_resp_message = "-";
  std::string http_resp_body = "-";
};

struct HTTPTraceRecord {
  SocketConnection conn;
  HTTPMessage message;
};

/**
 * @brief Parses the message body assuming it's encoded with 'Transfer-Encoding: chunked'.
 * Writes a bool to indicate if the message body surpasses the end of the entire message.
 *
 * @param record The input and result.
 */
void ParseMessageBodyChunked(HTTPTraceRecord* record);

void PreProcessHTTPRecord(HTTPTraceRecord* record);
void ParseEventAttr(const socket_data_event_t& event, HTTPTraceRecord* record);
// TODO(yzhao): Changes the functions that return bool to return Status.
bool ParseHTTPRequest(const socket_data_event_t& event, HTTPTraceRecord* record);
bool ParseHTTPResponse(const socket_data_event_t& event, HTTPTraceRecord* record);
struct IPEndpoint {
  std::string ip;
  int port;
};
StatusOr<IPEndpoint> ParseSockAddr(const socket_data_event_t& event);
bool ParseSockAddr(const socket_data_event_t& event, HTTPTraceRecord* record);
// TODO(oazizi): Enable to output all raw events on debug cases for particular protocols.
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

struct PicoHTTPParserWrapper {
  bool ParseResponse(std::string_view buf);
  bool WriteResponse(HTTPMessage* result);

  // For parsing HTTP responses.
  const char* msg = nullptr;
  size_t msg_len = 0;
  int minor_version = 0;
  int status = 0;
  static constexpr size_t kMaxNumHeaders = 50;
  struct phr_header headers[kMaxNumHeaders];

  // Needs explicit reset.
  size_t num_headers = kMaxNumHeaders;
  std::map<std::string, std::string> header_map;
  std::string_view unparsed_data;
};

/**
 * @brief Parses events traced from write/sendto syscalls,
 * and emits a complete HTTP message in the process.
 */
class HTTPParser {
 public:
  enum class ParseState {
    kUnknown,
    // The data is invalid.
    kInvalid,
    // The data has been combined with an incomplete HTTP message, but more data is needed.
    kNeedsMoreData,
    kSuccess,
  };

  /**
   * @brief Parses a possibly incomplete data chunk of a HTTP message, and combines it with any
   * previous partial messages.
   */
  ParseState ParseResponse(const socket_data_event_t& event);
  void Close();

  /**
   * @brief Extracts the current parsed HTTP messages, partial ones are not included.
   */
  std::vector<HTTPMessage> ExtractHTTPMessages();

 private:
  PicoHTTPParserWrapper pico_wrapper_;
  std::vector<HTTPMessage> msgs_complete_;
  // Map from an incomplete HTTPMessage's last sequence number to the partial message itself.
  std::map<uint64_t, HTTPMessage> msgs_incomplete_;
};

}  // namespace stirling
}  // namespace pl
