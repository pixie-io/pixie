#include "src/stirling/http_parse.h"

#include <picohttpparser.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "src/common/zlib/zlib_wrapper.h"

namespace pl {
namespace stirling {
namespace http {

void PreProcessMessage(HTTPMessage* message) {
  auto content_encoding_iter = message->http_headers.find(kContentEncoding);
  // Replace body with decompressed version, if required.
  if (content_encoding_iter != message->http_headers.end() &&
      content_encoding_iter->second == "gzip") {
    std::string_view body_strview(message->http_msg_body);
    auto bodyOrErr = pl::zlib::StrInflate(body_strview);
    if (!bodyOrErr.ok()) {
      LOG(WARNING) << "Unable to gunzip HTTP body.";
      message->http_msg_body = "<Stirling failed to gunzip body>";
    } else {
      message->http_msg_body = bodyOrErr.ValueOrDie();
    }
  }
}

namespace {

std::map<std::string, std::string> GetHttpHeadersMap(const phr_header* headers,
                                                     size_t num_headers) {
  std::map<std::string, std::string> result;
  for (size_t i = 0; i < num_headers; i++) {
    std::string name(headers[i].name, headers[i].name_len);
    std::string value(headers[i].value, headers[i].value_len);
    result[name] = value;
  }
  return result;
}

}  // namespace

HTTPHeaderFilter ParseHTTPHeaderFilters(std::string_view filters) {
  HTTPHeaderFilter result;
  for (std::string_view header_filter : absl::StrSplit(filters, ",", absl::SkipEmpty())) {
    std::pair<std::string_view, std::string_view> header_substr =
        absl::StrSplit(header_filter, absl::MaxSplits(":", 1));
    if (absl::StartsWith(header_substr.first, "-")) {
      header_substr.first.remove_prefix(1);
      result.exclusions.emplace(header_substr);
    } else {
      result.inclusions.emplace(header_substr);
    }
  }
  return result;
}

bool MatchesHTTPTHeaders(const std::map<std::string, std::string>& http_headers,
                         const HTTPHeaderFilter& filter) {
  if (!filter.inclusions.empty()) {
    bool included = false;
    // cpplint lags behind C++17, and only consider '[]' as an operator, therefore insists that no
    // space is before '[]'. And clang-format, which seems is updated with C++17, insists to add a
    // space as it's necessary in this form.
    //
    // TODO(yzhao): Update cpplint to newer version.
    // NOLINTNEXTLINE: whitespace/braces
    for (auto [http_header, substr] : filter.inclusions) {
      auto http_header_iter = http_headers.find(std::string(http_header));
      if (http_header_iter != http_headers.end() &&
          absl::StrContains(http_header_iter->second, substr)) {
        included = true;
        break;
      }
    }
    if (!included) {
      return false;
    }
  }
  // For symmetry with the above if block and safety in case of copy-paste, we put exclusions search
  // also inside a if statement, which is not needed for correctness.
  if (!filter.exclusions.empty()) {
    bool excluded = false;
    // NOLINTNEXTLINE: whitespace/braces
    for (auto [http_header, substr] : filter.exclusions) {
      auto http_header_iter = http_headers.find(std::string(http_header));
      if (http_header_iter != http_headers.end() &&
          absl::StrContains(http_header_iter->second, substr)) {
        excluded = true;
        break;
      }
    }
    if (excluded) {
      return false;
    }
  }
  return true;
}

//=============================================================================
// PicoHTTPParserWrapper
//=============================================================================

// TODO(oazizi): Restructure is in progress. Complete it:
//  - Eliminate this class. Pull out functions as stand-alone.
//  - Member variables only required inside ParseRequest()/ParseResponse(),
//    and each only requires its own subset.
class PicoHTTPParserWrapper {
 public:
  /**
   * @brief Parses a raw input buffer for HTTP messages.
   * HTTP headers are parsed by pico. Body is extracted separately.
   *
   * @param type: request or response
   * @param buf: The source buffer to parse. The prefix of this buffer will be consumed to indicate
   * the point until which the parse has progressed.
   * @param result: A parsed HTTP message, if parse was successful (must consider return value).
   * @return parse state indicating how the parse progressed.
   */
  ParseState Parse(MessageType type, std::string_view* buf, HTTPMessage* result);

 private:
  ParseState ParseRequest(std::string_view* buf, HTTPMessage* result);
  ParseState ParseResponse(std::string_view* buf, HTTPMessage* result);
  ParseState ParseBody(std::string_view* buf, HTTPMessage* result);

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
};

ParseState PicoHTTPParserWrapper::Parse(MessageType type, std::string_view* buf,
                                        HTTPMessage* result) {
  switch (type) {
    case MessageType::kRequest:
      return ParseRequest(buf, result);
    case MessageType::kResponse:
      return ParseResponse(buf, result);
    default:
      return ParseState::kInvalid;
  }
}

ParseState PicoHTTPParserWrapper::ParseRequest(std::string_view* buf, HTTPMessage* result) {
  // Set header number to maximum we can accept.
  // Pico will change it to the number of headers parsed for us.
  size_t num_headers = kMaxNumHeaders;
  const int retval =
      phr_parse_request(buf->data(), buf->size(), &method, &method_len, &path, &path_len,
                        &minor_version, headers, &num_headers, /*last_len*/ 0);
  if (retval >= 0) {
    buf->remove_prefix(retval);
    header_map = GetHttpHeadersMap(headers, num_headers);

    result->type = MessageType::kRequest;
    result->http_minor_version = minor_version;
    result->http_headers = std::move(header_map);
    result->http_req_method = std::string(method, method_len);
    result->http_req_path = std::string(path, path_len);

    return ParseBody(buf, result);
  }
  if (retval == -2) {
    return ParseState::kNeedsMoreData;
  }
  return ParseState::kInvalid;
}

ParseState PicoHTTPParserWrapper::ParseResponse(std::string_view* buf, HTTPMessage* result) {
  // Set header number to maximum we can accept.
  // Pico will change it to the number of headers parsed for us.
  size_t num_headers = kMaxNumHeaders;
  const int retval = phr_parse_response(buf->data(), buf->size(), &minor_version, &status, &msg,
                                        &msg_len, headers, &num_headers, /*last_len*/ 0);
  if (retval >= 0) {
    buf->remove_prefix(retval);
    header_map = GetHttpHeadersMap(headers, num_headers);

    result->type = MessageType::kResponse;
    result->http_minor_version = minor_version;
    result->http_headers = std::move(header_map);
    result->http_resp_status = status;
    result->http_resp_message = std::string(msg, msg_len);

    return ParseBody(buf, result);
  }
  if (retval == -2) {
    return ParseState::kNeedsMoreData;
  }
  return ParseState::kInvalid;
}

namespace {

// Mutates the input data.
ParseState ParseChunk(std::string_view* data, HTTPMessage* result) {
  phr_chunked_decoder chunk_decoder = {};
  auto buf = const_cast<char*>(data->data());
  size_t buf_size = data->size();
  ssize_t retval = phr_decode_chunked(&chunk_decoder, buf, &buf_size);
  if (retval == -1) {
    // Parse failed.
    return ParseState::kInvalid;
  } else if (retval >= 0) {
    // Complete message.
    result->http_msg_body.append(buf, buf_size);
    *data = std::string_view(buf + buf_size, retval);
    // Pico claims that the last \r\n are unparsed, manually remove them.
    while (!data->empty() && (data->front() == '\r' || data->front() == '\n')) {
      data->remove_prefix(1);
    }
    return ParseState::kSuccess;
  } else if (retval == -2) {
    // Incomplete message.
    result->http_msg_body.append(buf, buf_size);
    return ParseState::kNeedsMoreData;
  }
  return ParseState::kUnknown;
}

}  // namespace

ParseState PicoHTTPParserWrapper::ParseBody(std::string_view* buf, HTTPMessage* result) {
  // Try to find boundary of message by looking at Content-Length and Transfer-Encoding.

  // From https://tools.ietf.org/html/rfc7230:
  //  A sender MUST NOT send a Content-Length header field in any message
  //  that contains a Transfer-Encoding header field.
  //
  //  A user agent SHOULD send a Content-Length in a request message when
  //  no Transfer-Encoding is sent and the request method defines a meaning
  //  for an enclosed payload body.  For example, a Content-Length header
  //  field is normally sent in a POST request even when the value is 0
  //  (indicating an empty payload body).  A user agent SHOULD NOT send a
  //  Content-Length header field when the request message does not contain
  //  a payload body and the method semantics do not anticipate such a
  //  body.

  // Case 1: Content-Length
  const auto content_length_iter = result->http_headers.find(kContentLength);
  if (content_length_iter != result->http_headers.end()) {
    const int len = std::stoi(content_length_iter->second);
    if (len < 0) {
      LOG(ERROR) << "HTTP message has a negative Content-Length: " << len;
      return ParseState::kInvalid;
    }

    if (buf->size() < static_cast<size_t>(len)) {
      return ParseState::kNeedsMoreData;
    }

    result->http_msg_body = buf->substr(0, len);
    buf->remove_prefix(std::min(static_cast<size_t>(len), buf->size()));
    return ParseState::kSuccess;
  }

  // Case 2: Chunked transfer.
  const auto transfer_encoding_iter = result->http_headers.find(kTransferEncoding);
  if (transfer_encoding_iter != result->http_headers.end() &&
      transfer_encoding_iter->second == "chunked") {
    // TODO(yzhao): Change to set default value in appending record batch instead of data for
    // parsing.
    result->http_msg_body.clear();
    return ParseChunk(buf, result);
  }

  // Case 3: Request with no body.
  // An HTTP GET with no Content-Length and no Transfer-Encoding should not have a body when no
  // Content-Length or Transfer-Encoding is set:
  // "A user agent SHOULD NOT send a Content-Length header field when the request message does
  // not contain a payload body and the method semantics do not anticipate such a body."
  if (result->http_req_method == "GET") {
    result->http_msg_body = "";
    return ParseState::kSuccess;
  }

  // Case 4: Response indicating a protocol switch.
  if (result->http_resp_status == 101) {
    result->http_msg_body = "";

    const auto upgrade_iter = result->http_headers.find(kUpgrade);
    if (upgrade_iter == result->http_headers.end()) {
      LOG(WARNING) << "Expected an Upgrade header with HTTP status 101";
      return ParseState::kEOS;
    }

    // Header 'Upgrade: h2c' indicates protocol switch is to HTTP/2.
    // See: https://http2.github.io/http2-spec/#discover-http
    if (upgrade_iter->second == "h2c") {
      LOG(WARNING) << "HTTP upgrades to HTTP2 are not yet supported";
      // TODO(oazizi/yzhao): Support upgrades to HTTP/2.
    }

    return ParseState::kEOS;
  }

  // TODO(oazizi): Add other special responses that don't have a body.

  // Case 5: Message has content, but no length or chunking provided, so wait for close().
  // For messages that do not have Content-Length and chunked Transfer-Encoding. According to
  // HTTP/1.1 standard: https://www.w3.org/Protocols/HTTP/1.0/draft-ietf-http-spec.html#BodyLength
  // such messages is terminated by the close of the connection.
  // TODO(yzhao): For now we just accumulate messages, let probe_close() submit a message to
  // perf buffer, so that we can terminate such messages.
  if (!buf->empty()) {
    // TODO(yzhao): This assignment overwrites the default value "-". We should move the setting of
    // default value outside of HTTP message parsing and into appending HTTP messages to record
    // batch.
    result->http_msg_body = *buf;
    buf->remove_prefix(buf->size());
    LOG(WARNING)
        << "HTTP message with no Content-Length or Transfer-Encoding is not fully supported.";
    // TODO(yzhao/oazizi): Revisit the implementation of this case.
    return ParseState::kInvalid;
  }

  LOG(WARNING) << "Could not figure out how to extract body" << std::endl;
  return ParseState::kInvalid;
}

}  // namespace http

template <>
ParseResult<size_t> Parse(MessageType type, std::string_view buf,
                          std::deque<http::HTTPMessage>* messages) {
  http::PicoHTTPParserWrapper pico;
  std::vector<size_t> start_position;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;
  size_t bytes_processed = 0;

  while (!buf.empty() && s != ParseState::kEOS) {
    http::HTTPMessage message;

    s = pico.Parse(type, &buf, &message);
    if (s != ParseState::kSuccess && s != ParseState::kEOS) {
      break;
    }

    start_position.push_back(bytes_processed);
    messages->push_back(std::move(message));
    bytes_processed = (buf_size - buf.size());
  }

  ParseResult<size_t> result{std::move(start_position), bytes_processed, s};
  return result;
}

template <>
size_t FindMessageBoundary<http::HTTPMessage>(MessageType type, std::string_view buf,
                                              size_t start_pos) {
  // List of all HTTP request methods. All HTTP requests start with one of these.
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods
  static constexpr ConstStrView kHTTPReqStartPatternArray[] = {
      "GET ", "HEAD ", "POST ", "PUT ", "DELETE ", "CONNECT ", "OPTIONS ", "TRACE ", "PATCH ",
  };

  // List of supported HTTP protocol versions. HTTP responses typically start with one of these.
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Messages
  static constexpr ConstStrView kHTTPRespStartPatternArray[] = {"HTTP/1.1 ", "HTTP/1.0 "};

  static constexpr ConstVectorView<ConstStrView> kHTTPReqStartPatterns =
      ConstVectorView<ConstStrView>(kHTTPReqStartPatternArray);
  static constexpr ConstVectorView<ConstStrView> kHTTPRespStartPatterns =
      ConstVectorView<ConstStrView>(kHTTPRespStartPatternArray);

  static constexpr std::string_view kBoundaryMarker = "\r\n\r\n";

  // Choose the right set of patterns for request vs response.
  const ConstVectorView<ConstStrView>* start_patterns = nullptr;
  switch (type) {
    case MessageType::kRequest:
      start_patterns = &kHTTPReqStartPatterns;
      break;
    case MessageType::kResponse:
      start_patterns = &kHTTPRespStartPatterns;
      break;
    case MessageType::kUnknown:
      return std::string::npos;
  }

  // Search for a boundary marker, preceded with a message start.
  // Example, using HTTP Response:
  //   leftover body (from previous message)
  //   HTTP/1.1 ...
  //   headers
  //   \r\n\r\n
  //   body
  // We first search forwards for \r\n\r\n, then we search backwards from there for HTTP/1.1.
  //
  // Note that we don't search forwards for HTTP/1.1 directly, because it could result in matches
  // inside the request/response body.
  while (true) {
    size_t marker_pos = buf.find(kBoundaryMarker, start_pos);

    if (marker_pos == std::string::npos) {
      return std::string::npos;
    }

    std::string_view buf_substr = buf.substr(start_pos, marker_pos - start_pos);

    size_t substr_pos = std::string::npos;
    for (auto& start_pattern : *start_patterns) {
      size_t current_substr_pos = buf_substr.rfind(start_pattern);
      if (current_substr_pos != std::string::npos) {
        // Found a match. Check if it is closer to the marker than our previous match.
        // We want to return the match that is closest to the marker, so we aren't
        // matching to something in a previous message's body.
        substr_pos = (substr_pos == std::string::npos) ? current_substr_pos
                                                       : std::max(substr_pos, current_substr_pos);
      }
    }

    if (substr_pos != std::string::npos) {
      return start_pos + substr_pos;
    }

    // Couldn't find a start position. Move to the marker, and search for another marker.
    start_pos = marker_pos + kBoundaryMarker.size();
  }
}

}  // namespace stirling
}  // namespace pl
