#include "src/stirling/http_parse.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <picohttpparser.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "src/common/zlib/zlib_wrapper.h"

namespace pl {
namespace stirling {

void PreProcessMessage(HTTPMessage* message) {
  auto content_encoding_iter = message->http_headers.find(http_headers::kContentEncoding);
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

// Parses an IP:port pair from conn_info.
// Returns an error if an unexpected sockaddr family is provided.
// Currently this function understands IPV4 and IPV6 sockaddr families.
StatusOr<IPEndpoint> ParseSockAddr(const conn_info_t& conn_info) {
  const auto* sa = reinterpret_cast<const struct sockaddr*>(&conn_info.addr);

  char addr[INET6_ADDRSTRLEN] = "";
  int port;

  switch (sa->sa_family) {
    case AF_INET: {
      const auto* sa_in = reinterpret_cast<const struct sockaddr_in*>(sa);
      port = ntohs(sa_in->sin_port);
      if (inet_ntop(AF_INET, &sa_in->sin_addr, addr, INET_ADDRSTRLEN) == nullptr) {
        return error::InvalidArgument("Could not parse sockaddr (AF_INET)");
      }
    } break;
    case AF_INET6: {
      const auto* sa_in6 = reinterpret_cast<const struct sockaddr_in6*>(sa);
      port = ntohs(sa_in6->sin6_port);
      if (inet_ntop(AF_INET6, &sa_in6->sin6_addr, addr, INET6_ADDRSTRLEN) == nullptr) {
        return error::InvalidArgument("Could not parse sockaddr (AF_INET6)");
      }
    } break;
    default:
      return error::InvalidArgument(
          absl::StrCat("Ignoring unhandled sockaddr family: ", sa->sa_family));
  }

  return IPEndpoint{std::string(addr), port};
}

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

ParseState PicoHTTPParserWrapper::ParseRequest(std::string_view buf) {
  // Set header number to maximum we can accept.
  // Pico will change it to the number of headers parsed for us.
  size_t num_headers = kMaxNumHeaders;
  const int retval =
      phr_parse_request(buf.data(), buf.size(), &method, &method_len, &path, &path_len,
                        &minor_version, headers, &num_headers, /*last_len*/ 0);
  if (retval >= 0) {
    unparsed_data = buf.substr(retval);
    header_map = GetHttpHeadersMap(headers, num_headers);
    return ParseState::kSuccess;
  }
  if (retval == -2) {
    return ParseState::kNeedsMoreData;
  }
  return ParseState::kInvalid;
}

ParseState PicoHTTPParserWrapper::ParseResponse(std::string_view buf) {
  // Set header number to maximum we can accept.
  // Pico will change it to the number of headers parsed for us.
  size_t num_headers = kMaxNumHeaders;
  const int retval = phr_parse_response(buf.data(), buf.size(), &minor_version, &status, &msg,
                                        &msg_len, headers, &num_headers, /*last_len*/ 0);
  if (retval >= 0) {
    unparsed_data = buf.substr(retval);
    header_map = GetHttpHeadersMap(headers, num_headers);
    return ParseState::kSuccess;
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

ParseState PicoHTTPParserWrapper::WriteRequest(HTTPMessage* result) {
  result->type = HTTPEventType::kHTTPRequest;
  result->http_minor_version = minor_version;
  result->http_headers = std::move(header_map);
  result->http_req_method = std::string(method, method_len);
  result->http_req_path = std::string(path, path_len);

  return WriteBody(result);
}

ParseState PicoHTTPParserWrapper::WriteResponse(HTTPMessage* result) {
  result->type = HTTPEventType::kHTTPResponse;
  result->http_minor_version = minor_version;
  result->http_headers = std::move(header_map);
  result->http_resp_status = status;
  result->http_resp_message = std::string(msg, msg_len);

  return WriteBody(result);
}

ParseState PicoHTTPParserWrapper::WriteBody(HTTPMessage* result) {
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
  const auto content_length_iter = result->http_headers.find(http_headers::kContentLength);
  if (content_length_iter != result->http_headers.end()) {
    const int len = std::stoi(content_length_iter->second);
    if (len < 0) {
      LOG(ERROR) << "HTTP message has a negative Content-Length: " << len;
      return ParseState::kInvalid;
    }

    if (unparsed_data.size() < static_cast<size_t>(len)) {
      // TODO(oazizi): Returning false, so why set fields?
      result->http_msg_body.reserve(len);
      result->content_length = len;
      result->http_msg_body = unparsed_data;
      return ParseState::kNeedsMoreData;
    }

    result->http_msg_body = unparsed_data.substr(0, len);
    unparsed_data.remove_prefix(std::min(static_cast<size_t>(len), unparsed_data.size()));
    return ParseState::kSuccess;
  }

  // Case 2: Chunked transfer.
  const auto transfer_encoding_iter = result->http_headers.find(http_headers::kTransferEncoding);
  if (transfer_encoding_iter != result->http_headers.end() &&
      transfer_encoding_iter->second == "chunked") {
    // TODO(yzhao): Change to set default value in appending record batch instead of data for
    // parsing.
    result->http_msg_body.clear();
    return ParseChunk(&unparsed_data, result);
  }

  // Case 3: Request with no body.
  // An HTTP GET with no Content-Length and no Transfer-Encoding should not have a body
  // when no Content-Length or Transfer-Encoding is set:
  // "A user agent SHOULD NOT send a Content-Length header field when the request
  // message does not contain a payload body and the method semantics do not anticipate such a
  // body."
  if (result->http_req_method == "GET") {
    result->http_msg_body = "";
    return ParseState::kSuccess;
  }

  // Case 4: Message has content, but no length or chunking provided, so wait for close().
  // For messages that do not have Content-Length and chunked Transfer-Encoding. According to
  // HTTP/1.1 standard: https://www.w3.org/Protocols/HTTP/1.0/draft-ietf-http-spec.html#BodyLength
  // such messages is terminated by the close of the connection.
  // TODO(yzhao): For now we just accumulate messages, let probe_close() submit a message to
  // perf buffer, so that we can terminate such messages.
  if (!unparsed_data.empty()) {
    // TODO(yzhao): This assignment overwrites the default value "-". We should move the setting of
    // default value outside of HTTP message parsing and into appending HTTP messages to record
    // batch.
    result->http_msg_body = unparsed_data;
    unparsed_data.remove_prefix(unparsed_data.size());
    LOG(WARNING)
        << "HTTP message with no Content-Length or Transfer-Encoding is not fully supported.";
    // TODO(yzhao/oazizi): Revisit the implementation of this case.
    return ParseState::kInvalid;
  }

  LOG(WARNING) << "Could not figure out how to extract body" << std::endl;
  return ParseState::kInvalid;
}

ParseResult<size_t> Parse(MessageType type, std::string_view buf,
                          std::deque<HTTPMessage>* messages) {
  PicoHTTPParserWrapper pico;
  std::vector<size_t> start_position;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;
  size_t bytes_processed = 0;

  while (!buf.empty()) {
    s = pico.Parse(type, buf);
    if (s != ParseState::kSuccess) {
      break;
    }

    HTTPMessage message;
    s = pico.Write(type, &message);
    if (s != ParseState::kSuccess) {
      break;
    }

    start_position.push_back(bytes_processed);
    messages->push_back(std::move(message));
    buf = pico.unparsed_data;
    bytes_processed = (buf_size - buf.size());
  }

  ParseResult<size_t> result{std::move(start_position), bytes_processed, s};
  return result;
}

}  // namespace stirling
}  // namespace pl
