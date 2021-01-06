#include "src/stirling/protocols/http/parse.h"

#include <picohttpparser.h>

#include <algorithm>
#include <string>
#include <utility>

namespace pl {
namespace stirling {
namespace protocols {
namespace http {

namespace {

HeadersMap GetHTTPHeadersMap(const phr_header* headers, size_t num_headers) {
  HeadersMap result;
  for (size_t i = 0; i < num_headers; i++) {
    std::string name(headers[i].name, headers[i].name_len);
    std::string value(headers[i].value, headers[i].value_len);
    result.insert({name, value});
  }
  return result;
}

}  // namespace

//=============================================================================
// Pico Wrapper
//=============================================================================

namespace pico_wrapper {

namespace {

// Mutates the input data.
ParseState ParseChunk(std::string_view* data, Message* result) {
  result->body.clear();
  phr_chunked_decoder chunk_decoder = {};
  auto buf = const_cast<char*>(data->data());
  size_t buf_size = data->size();
  ssize_t retval = phr_decode_chunked(&chunk_decoder, buf, &buf_size);
  if (retval == -1) {
    // Parse failed.
    return ParseState::kInvalid;
  } else if (retval >= 0) {
    // Complete message.
    result->body.append(buf, buf_size);
    // phr_decode_chunked rewrites the buffer in place, removing chunked-encoding headers.
    // So we cannot simply remove the prefix, but rather have to shorten the buffer too.
    // This is done via retval, which specifies how many unprocessed bytes are left.
    *data = std::string_view(buf + buf_size, retval);
    // Pico claims that the last \r\n are unparsed, manually remove them.
    while (!data->empty() && (data->front() == '\r' || data->front() == '\n')) {
      data->remove_prefix(1);
    }
    return ParseState::kSuccess;
  } else if (retval == -2) {
    // Incomplete message.
    return ParseState::kNeedsMoreData;
  }
  LOG(DFATAL) << "Unexpected retval from phr_decode_chunked()";
  return ParseState::kUnknown;
}

}  // namespace

ParseState ParseBody(std::string_view* buf, Message* result) {
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
  const auto content_length_iter = result->headers.find(kContentLength);
  if (content_length_iter != result->headers.end()) {
    size_t len;
    if (!absl::SimpleAtoi(content_length_iter->second, &len)) {
      LOG(ERROR) << absl::Substitute("Unable to parse Content-Length: $0",
                                     content_length_iter->second);
      return ParseState::kInvalid;
    }

    if (buf->size() < len) {
      return ParseState::kNeedsMoreData;
    }

    result->body = buf->substr(0, len);
    buf->remove_prefix(std::min(len, buf->size()));
    return ParseState::kSuccess;
  }

  // Case 2: Chunked transfer.
  const auto transfer_encoding_iter = result->headers.find(kTransferEncoding);
  if (transfer_encoding_iter != result->headers.end() &&
      transfer_encoding_iter->second == "chunked") {
    // TODO(yzhao): Change to set default value in appending record batch instead of data for
    // parsing.
    return ParseChunk(buf, result);
  }

  // Case 3: Message has content, but no Content-Length or Transfer-Encoding.

  // Case 3A: Requests where we can assume no body.
  // An HTTP request with no Content-Length and no Transfer-Encoding should not have a body when
  // no Content-Length or Transfer-Encoding is set:
  // "A user agent SHOULD NOT send a Content-Length header field when the request message does
  // not contain a payload body and the method semantics do not anticipate such a body."
  //
  // We apply this to all methods, since we have no better strategy in other cases.
  // TODO(oazizi): Revisit this strategy if we see problems.
  if (result->type == MessageType::kRequest) {
    result->body = "";
    return ParseState::kSuccess;
  }

  // Case 3B: Responses where we can assume no body.
  // The status codes below MUST not have a body, according to the spec,
  // so if no Content-Length or Transfer-Encoding are present,
  // assume they don't have a body.
  // See: https://tools.ietf.org/html/rfc2616#section-4.4
  // TODO(oazizi): Are there more responses where we can assume no body?
  if ((result->resp_status >= 100 && result->resp_status < 200) || result->resp_status == 204 ||
      result->resp_status == 304) {
    result->body = "";

    // Status 101 is an even more special case.
    if (result->resp_status == 101) {
      const auto upgrade_iter = result->headers.find(kUpgrade);
      if (upgrade_iter == result->headers.end()) {
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

    return ParseState::kSuccess;
  }

  // Case 3C: Response where we can't assume no body, but where no Content-Length or
  // Transfer-Encoding is provided. In these cases we should wait for close().
  // According to HTTP/1.1 standard:
  // https://www.w3.org/Protocols/HTTP/1.0/draft-ietf-http-spec.html#BodyLength
  // such messages are terminated by the close of the connection.
  // TODO(yzhao): For now we just accumulate messages, let probe_close() submit a message to
  // perf buffer, so that we can terminate such messages.
  if (!buf->empty()) {
    // Currently, we output the parsed message with a potentially partial body.
    // Only the body that is present at the time is emitted, since we don't
    // know if the data is actually complete or not without a length.

    // TODO(yzhao): This assignment overwrites the default value "-". We should move the setting of
    // default value outside of HTTP message parsing and into appending HTTP messages to record
    // batch.
    result->body = *buf;
    buf->remove_prefix(buf->size());
    LOG_FIRST_N(WARNING, 10)
        << "HTTP message with no Content-Length or Transfer-Encoding may produce "
           "incomplete message bodies.";
    // TODO(yzhao/oazizi): Revisit the implementation of this case.
    return ParseState::kSuccess;
  }

  LOG_FIRST_N(WARNING, 10) << "Could not figure out how to extract body" << std::endl;
  return ParseState::kInvalid;
}

ParseState ParseRequest(std::string_view* buf, Message* result) {
  // Fields populated by phr_parse_response.
  const char* method = nullptr;
  size_t method_len;
  const char* path = nullptr;
  size_t path_len;
  int minor_version = 0;
  static constexpr size_t kMaxNumHeaders = 50;
  struct phr_header headers[kMaxNumHeaders];

  // Set header number to maximum we can accept.
  // Pico will change it to the number of headers parsed for us.
  size_t num_headers = kMaxNumHeaders;

  const int retval =
      phr_parse_request(buf->data(), buf->size(), &method, &method_len, &path, &path_len,
                        &minor_version, headers, &num_headers, /*last_len*/ 0);
  if (retval >= 0) {
    buf->remove_prefix(retval);

    result->type = MessageType::kRequest;
    result->minor_version = minor_version;
    result->headers = GetHTTPHeadersMap(headers, num_headers);
    result->req_method = std::string(method, method_len);
    result->req_path = std::string(path, path_len);
    result->headers_byte_size = retval;

    return ParseBody(buf, result);
  }
  if (retval == -2) {
    return ParseState::kNeedsMoreData;
  }
  return ParseState::kInvalid;
}

ParseState ParseResponse(std::string_view* buf, Message* result) {
  // Fields populated by phr_parse_response.
  const char* msg = nullptr;
  size_t msg_len = 0;
  int status = 0;
  int minor_version = 0;
  static constexpr size_t kMaxNumHeaders = 50;
  struct phr_header headers[kMaxNumHeaders];

  // Set header number to maximum we can accept.
  // Pico will change it to the number of headers parsed for us.
  size_t num_headers = kMaxNumHeaders;
  const int retval = phr_parse_response(buf->data(), buf->size(), &minor_version, &status, &msg,
                                        &msg_len, headers, &num_headers, /*last_len*/ 0);
  if (retval >= 0) {
    buf->remove_prefix(retval);

    result->type = MessageType::kResponse;
    result->minor_version = minor_version;
    result->headers = GetHTTPHeadersMap(headers, num_headers);
    result->resp_status = status;
    result->resp_message = std::string(msg, msg_len);
    result->headers_byte_size = retval;

    return ParseBody(buf, result);
  }
  if (retval == -2) {
    return ParseState::kNeedsMoreData;
  }
  return ParseState::kInvalid;
}

}  // namespace pico_wrapper

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
ParseState ParseFrame(MessageType type, std::string_view* buf, Message* result) {
  switch (type) {
    case MessageType::kRequest:
      return pico_wrapper::ParseRequest(buf, result);
    case MessageType::kResponse:
      return pico_wrapper::ParseResponse(buf, result);
    default:
      return ParseState::kInvalid;
  }
}

// TODO(oazizi/yzhao): This function should use is_http_{response,request} inside
// bcc_bpf/socket_trace.c to check if a sequence of bytes are aligned on HTTP message boundary.
// ATM, they actually do not share the same logic. As a result, BPF events detected as HTTP traffic,
// can actually fail to find any valid boundary by this function. Unfortunately, BPF has many
// restrictions that likely make this a difficult or impossible goal.
size_t FindFrameBoundary(MessageType type, std::string_view buf, size_t start_pos) {
  // List of all HTTP request methods. All HTTP requests start with one of these.
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods
  static constexpr std::string_view kHTTPReqStartPatternArray[] = {
      "GET ", "HEAD ", "POST ", "PUT ", "DELETE ", "CONNECT ", "OPTIONS ", "TRACE ", "PATCH ",
  };

  // List of supported HTTP protocol versions. HTTP responses typically start with one of these.
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Messages
  static constexpr std::string_view kHTTPRespStartPatternArray[] = {"HTTP/1.1 ", "HTTP/1.0 "};

  static constexpr ArrayView<std::string_view> kHTTPReqStartPatterns =
      ArrayView<std::string_view>(kHTTPReqStartPatternArray);
  static constexpr ArrayView<std::string_view> kHTTPRespStartPatterns =
      ArrayView<std::string_view>(kHTTPRespStartPatternArray);

  static constexpr std::string_view kBoundaryMarker = "\r\n\r\n";

  // Choose the right set of patterns for request vs response.
  const ArrayView<std::string_view>* start_patterns = nullptr;
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

}  // namespace http

template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, http::Message* result) {
  return http::ParseFrame(type, buf, result);
}

template <>
size_t FindFrameBoundary<http::Message>(MessageType type, std::string_view buf, size_t start_pos) {
  return http::FindFrameBoundary(type, buf, start_pos);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
