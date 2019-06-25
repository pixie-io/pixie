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

void PreProcessHTTPRecord(HTTPTraceRecord* record) {
  auto content_encoding_iter = record->message.http_headers.find(http_headers::kContentEncoding);
  // Replace body with decompressed version, if required.
  if (content_encoding_iter != record->message.http_headers.end() &&
      content_encoding_iter->second == "gzip") {
    std::string_view body_strview(record->message.http_msg_body);
    auto bodyOrErr = pl::zlib::StrInflate(body_strview);
    if (!bodyOrErr.ok()) {
      LOG(WARNING) << "Unable to gunzip HTTP body.";
      record->message.http_msg_body = "<Stirling failed to gunzip body>";
    } else {
      record->message.http_msg_body = bodyOrErr.ValueOrDie();
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
  int port = -1;

  switch (sa->sa_family) {
    case AF_INET: {
      const auto* sa_in = reinterpret_cast<const struct sockaddr_in*>(sa);
      port = sa_in->sin_port;
      if (inet_ntop(AF_INET, &sa_in->sin_addr, addr, INET_ADDRSTRLEN) == nullptr) {
        return error::InvalidArgument("Could not parse sockaddr (AF_INET)");
      }
    } break;
    case AF_INET6: {
      const auto* sa_in6 = reinterpret_cast<const struct sockaddr_in6*>(sa);
      port = sa_in6->sin6_port;
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

bool ParseRaw(const socket_data_event_t& event, const conn_info_t& conn_info,
              HTTPTraceRecord* record) {
  HTTPTraceRecord& result = *record;
  record->conn.tgid = event.attr.tgid;
  record->conn.fd = conn_info.fd;
  record->message.timestamp_ns = event.attr.timestamp_ns;
  result.message.type = SocketTraceEventType::kUnknown;
  result.message.http_msg_body = std::string(event.msg, event.attr.msg_size);
  // Rest of the fields remain at default values.
  return true;
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
  // Reset header number to the size of the buffer.
  num_headers = kMaxNumHeaders;
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
  // Reset header number to the size of the buffer.
  num_headers = kMaxNumHeaders;
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
  char* buf = const_cast<char*>(data->data());
  size_t buf_size = data->size();
  ssize_t retval = phr_decode_chunked(&result->chunk_decoder, buf, &buf_size);
  if (retval == -1) {
    // Parse failed.
    return ParseState::kInvalid;
  } else if (retval >= 0) {
    // Complete message.
    result->is_complete = true;
    result->http_msg_body.append(buf, buf_size);
    *data = std::string_view(buf + buf_size, retval);
    // Pico claims that the last \r\n are unparsed, manually remove them.
    while (!data->empty() && (data->front() == '\r' || data->front() == '\n')) {
      data->remove_prefix(1);
    }
    return ParseState::kSuccess;
  } else if (retval == -2) {
    // Incomplete message.
    result->is_complete = false;
    result->http_msg_body.append(buf, buf_size);
    return ParseState::kNeedsMoreData;
  }
  return ParseState::kUnknown;
}

}  // namespace

bool PicoHTTPParserWrapper::WriteRequest(HTTPMessage* result) {
  result->type = SocketTraceEventType::kHTTPRequest;
  result->http_minor_version = minor_version;
  result->http_headers = std::move(header_map);
  result->http_req_method = std::string(method, method_len);
  result->http_req_path = std::string(path, path_len);

  return WriteBody(result);
}

bool PicoHTTPParserWrapper::WriteResponse(HTTPMessage* result) {
  result->type = SocketTraceEventType::kHTTPResponse;
  result->http_minor_version = minor_version;
  result->http_headers = std::move(header_map);
  result->http_resp_status = status;
  result->http_resp_message = std::string(msg, msg_len);

  return WriteBody(result);
}

bool PicoHTTPParserWrapper::WriteBody(HTTPMessage* result) {
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

  const auto content_length_iter = result->http_headers.find(http_headers::kContentLength);
  if (content_length_iter != result->http_headers.end()) {
    const int len = std::stoi(content_length_iter->second);
    if (len < 0) {
      LOG(ERROR) << "HTTP message has a negative Content-Length: " << len;
      return false;
    }
    if (static_cast<size_t>(len) <= unparsed_data.size()) {
      result->is_complete = true;
      result->http_msg_body = unparsed_data.substr(0, len);
    } else {
      result->is_complete = false;
      result->http_msg_body.reserve(len);
      result->content_length = len;
      result->http_msg_body = unparsed_data;
    }
    unparsed_data.remove_prefix(std::min(static_cast<size_t>(len), unparsed_data.size()));
    return true;
  }

  const auto transfer_encoding_iter = result->http_headers.find(http_headers::kTransferEncoding);
  if (transfer_encoding_iter != result->http_headers.end() &&
      transfer_encoding_iter->second == "chunked") {
    // TODO(yzhao): Change to set default value in appending record batch instead of data for
    // parsing.
    result->http_msg_body.clear();
    result->is_chunked = true;
    ParseState s = ParseChunk(&unparsed_data, result);
    if (s != ParseState::kSuccess && s != ParseState::kNeedsMoreData) {
      return false;
    }
    return true;
  }

  // An HTTP GET with no Content-Length and no Transfer-Encoding should not have a body
  // when no Content-Length or Transfer-Encoding is set:
  // "A user agent SHOULD NOT send a Content-Length header field when the request
  // message does not contain a payload body and the method semantics do not anticipate such a
  // body."
  if (result->http_req_method == "GET") {
    result->is_complete = true;
    result->http_msg_body = "";
    return true;
  }

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
  }
  return true;
}

namespace {

// Utility to convert positions from a position within a set of combined buffers,
// to the position within a set of matching content in disjoint buffers.
// @param msgs The original set of disjoint buffers.
// @param pos The position within the combined buffer.
// @return Position within disjoint buffers, as buffer number and offset within the buffer.
BufferPosition ConvertPosition(std::vector<std::string_view> msgs, size_t pos) {
  size_t curr_seq = 0;
  size_t size = 0;
  for (auto msg : msgs) {
    size += msg.size();
    if (pos < size) {
      return {curr_seq, pos - (size - msg.size())};
    }
    ++curr_seq;
  }
  return {curr_seq, 0};
}

}  // namespace

HTTPParseResult<BufferPosition> HTTPParser::ParseMessages(TrafficMessageType type) {
  std::string buf = Combine();

  HTTPParseResult<size_t> result = Parse(type, buf);
  std::vector<BufferPosition> positions;

  // Match timestamps with the parsed messages.
  size_t i = 0;
  for (auto& msg : result.messages) {
    // TODO(oazizi): ConvertPosition is inefficient, because it starts searching from scratch
    // everytime.
    //               Could do better if ConvertPosition took a starting seq and size.
    BufferPosition position = ConvertPosition(msgs_, result.start_positions[i]);
    positions.push_back(position);
    DCHECK(position.seq_num < msgs_.size())
        << absl::Substitute("The sequence number must be in valid range of [0, $0)", msgs_.size());
    msg.timestamp_ns = ts_nses_[position.seq_num];
    CHECK(msg.is_complete || i == (result.messages.size() - 1)) << "Incomplete message!";
    ++i;
  }

  BufferPosition end_position = ConvertPosition(msgs_, result.end_position);

  msgs_.clear();
  ts_nses_.clear();
  msgs_size_ = 0;

  return {std::move(result.messages), std::move(positions), end_position, result.state};
}

void HTTPParser::Append(std::string_view msg, uint64_t ts_ns) {
  msgs_.push_back(msg);
  ts_nses_.push_back(ts_ns);
  msgs_size_ += msg.size();
}

std::string HTTPParser::Combine() const {
  std::string result;
  result.reserve(msgs_size_);
  for (auto msg : msgs_) {
    result.append(msg);
  }
  return result;
}

HTTPParseResult<size_t> Parse(TrafficMessageType type, std::string_view buf) {
  PicoHTTPParserWrapper pico;
  std::vector<HTTPMessage> messages;
  std::vector<size_t> start_position;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;
  size_t bytes_processed = 0;
  bool abort = false;

  while (!buf.empty() && !abort) {
    s = pico.Parse(type, buf);

    switch (s) {
      case ParseState::kSuccess: {
        HTTPMessage message;
        message.is_header_complete = true;
        if (!pico.Write(type, &message)) {
          s = ParseState::kInvalid;
          abort = true;
          break;
        }
        if (!message.is_complete) {
          s = ParseState::kNeedsMoreData;
          abort = true;
          break;
        }
        start_position.push_back(bytes_processed);
        messages.push_back(std::move(message));
        buf = pico.unparsed_data;
        bytes_processed = (buf_size - buf.size());
      } break;
      case ParseState::kNeedsMoreData:
        abort = true;
        break;
      case ParseState::kInvalid:
        abort = true;
        break;
      default:
        LOG(ERROR) << absl::StrFormat("Unexpected pico parse state %d", s);
        abort = true;
        break;
    }
  }

  HTTPParseResult<size_t> result{std::move(messages), std::move(start_position), bytes_processed,
                                 s};
  return result;
}

}  // namespace stirling
}  // namespace pl
