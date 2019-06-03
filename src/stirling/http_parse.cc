#include "src/stirling/http_parse.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <picohttpparser.h>

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
    std::string_view body_strview(record->message.http_resp_body);
    auto bodyOrErr = pl::zlib::StrInflate(body_strview);
    if (!bodyOrErr.ok()) {
      LOG(WARNING) << "Unable to gunzip HTTP body.";
      record->message.http_resp_body = "<Stirling failed to gunzip body>";
    } else {
      record->message.http_resp_body = bodyOrErr.ValueOrDie();
    }
  }
}

void ParseEventAttr(const socket_data_event_t& event, HTTPTraceRecord* record) {
  record->conn.tgid = event.attr.tgid;
  record->conn.fd = event.attr.fd;
  record->message.timestamp_ns = event.attr.timestamp_ns;
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

bool ParseHTTPRequest(const socket_data_event_t& event, HTTPTraceRecord* record) {
  // TODO(yzhao): Due to the BPF weirdness (see socket_trace.c), this calculation must be done here,
  // not in BPF. Investigate if we can fix it.
  const uint32_t msg_size = MsgSize(event);
  const char* method = nullptr;
  size_t method_len = 0;
  const char* path = nullptr;
  size_t path_len = 0;
  int minor_version = 0;
  size_t num_headers = 10;
  struct phr_header headers[num_headers];
  const int retval = phr_parse_request(event.msg, msg_size, &method, &method_len, &path, &path_len,
                                       &minor_version, headers, &num_headers, /*last_len*/ 0);

  if (retval > 0) {
    HTTPTraceRecord& result = *record;
    ParseEventAttr(event, &result);
    result.message.type = SocketTraceEventType::kHTTPRequest;
    result.message.http_minor_version = minor_version;
    result.message.http_headers = GetHttpHeadersMap(headers, num_headers);
    result.message.http_req_method = std::string(method, method_len);
    result.message.http_req_path = std::string(path, path_len);
    return true;
  }
  return false;
}

StatusOr<IPEndpoint> ParseSockAddr(const socket_data_event_t& event) {
  const auto* sa = reinterpret_cast<const struct sockaddr*>(&event.attr.conn_info.addr);

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

// Parses an IP:port pair from the event input into the provided record.
// Returns false if an unexpected sockaddr family is provided.
// Currently this function understands IPV4 and IPV6 sockaddr families.
bool ParseSockAddr(const socket_data_event_t& event, HTTPTraceRecord* record) {
  auto ip_endpoint_or = ParseSockAddr(event);
  if (ip_endpoint_or.ok()) {
    record->conn.dst_addr = std::move(ip_endpoint_or.ValueOrDie().ip);
    record->conn.dst_port = ip_endpoint_or.ValueOrDie().port;
    return true;
  }
  return false;
}

bool ParseRaw(const socket_data_event_t& event, HTTPTraceRecord* record) {
  HTTPTraceRecord& result = *record;
  ParseEventAttr(event, &result);
  result.message.type = SocketTraceEventType::kUnknown;
  result.message.http_resp_body = std::string(event.msg, MsgSize(event));
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

bool PicoHTTPParserWrapper::ParseResponse(std::string_view buf) {
  // Reset header number to the size of the buffer.
  num_headers = kMaxNumHeaders;
  const int retval = phr_parse_response(buf.data(), buf.size(), &minor_version, &status, &msg,
                                        &msg_len, headers, &num_headers, /*last_len*/ 0);
  if (retval >= 0) {
    unparsed_data = buf.substr(retval);
    header_map = GetHttpHeadersMap(headers, num_headers);
  }
  return retval >= 0;
}

namespace {

// Mutates the input data.
bool ParseChunk(std::string data, HTTPMessage* result) {
  char* buf = const_cast<char*>(data.data());
  size_t buf_size = data.size();
  ssize_t retval = phr_decode_chunked(&result->chunk_decoder, buf, &buf_size);
  if (retval == -1) {
    // Parse failed.
    return false;
  } else if (retval >= 0) {
    // Complete message.
    result->is_complete = true;
    result->http_resp_body.append(buf, buf_size);
    return true;
  } else if (retval == -2) {
    // Incomplete message.
    result->is_complete = false;
    result->http_resp_body.append(buf, buf_size);
    return true;
  }
  return false;
}

}  // namespace

bool PicoHTTPParserWrapper::WriteResponse(HTTPMessage* result) {
  result->type = SocketTraceEventType::kHTTPResponse;
  result->http_minor_version = minor_version;
  result->http_headers = std::move(header_map);
  result->http_resp_status = status;
  result->http_resp_message = std::string(msg, msg_len);

  const auto content_length_iter = result->http_headers.find(http_headers::kContentLength);
  if (content_length_iter != result->http_headers.end()) {
    const int len = std::stoi(content_length_iter->second);
    if (len < 0) {
      LOG(ERROR) << "HTTP message has a negative Content-Length: " << len;
      return false;
    }
    if (static_cast<size_t>(len) <= unparsed_data.size()) {
      result->is_complete = true;
      result->http_resp_body = unparsed_data.substr(0, len);
      if (static_cast<size_t>(len) < unparsed_data.size()) {
        LOG(WARNING) << "Have data left unparsed: " << unparsed_data.substr(len);
      }
    } else {
      result->is_complete = false;
      result->http_resp_body.reserve(len);
      result->content_length = len;
      result->http_resp_body = unparsed_data;
    }
    return true;
  }

  const auto transfer_encoding_iter = result->http_headers.find(http_headers::kTransferEncoding);
  if (transfer_encoding_iter != result->http_headers.end() &&
      transfer_encoding_iter->second == "chunked") {
    result->http_resp_body.clear();
    if (!ParseChunk(std::string(unparsed_data), result)) {
      return false;
    }
    return true;
  }

  // For messages that do not have Content-Length and chunked Transfer-Encoding. According to
  // HTTP/1.1 standard: https://www.w3.org/Protocols/HTTP/1.0/draft-ietf-http-spec.html#BodyLength
  // such messages is terminated by the close of the connection.
  // TODO(yzhao): For now we just accumulate messages, let probe_close() submit a message to
  // perf buffer, so that we can terminate such messages.
  if (!unparsed_data.empty()) {
    result->http_resp_body = unparsed_data;
  }
  return true;
}

// HTTP messages are sequentially written to the file descriptor, and their sequence numbers are
// obtained accordingly. We rely on the consecutive sequence numbers to detect missing events and
// order the events correctly.
HTTPParser::ParseState HTTPParser::ParseResponse(const socket_data_event_t& event) {
  const uint64_t seq_num = event.attr.conn_info.seq_num;
  std::string_view buf(event.msg, MsgSize(event));
  if (absl::StartsWith(buf, "HTTP")) {
    if (!pico_wrapper_.ParseResponse(buf)) {
      return ParseState::kInvalid;
    }
    HTTPMessage message;
    if (!pico_wrapper_.WriteResponse(&message)) {
      return ParseState::kInvalid;
    }
    // The message's time stamp is from its first event.
    message.timestamp_ns = event.attr.timestamp_ns;
    if (message.is_complete) {
      msgs_complete_.push_back(std::move(message));
      return ParseState::kSuccess;
    } else {
      msgs_incomplete_[seq_num] = std::move(message);
      return ParseState::kNeedsMoreData;
    }
  }
  if (seq_num == 0) {
    // This is the first event, and it does not start with a valid HTTP prefix, so we have no way to
    // produce a complete HTTP response from this.
    return ParseState::kInvalid;
  }
  const uint64_t prev_seq_num = seq_num - 1;
  auto iter = msgs_incomplete_.find(prev_seq_num);
  if (iter == msgs_incomplete_.end()) {
    // There is no previous unfinished HTTP message, maybe we just missed it.
    return ParseState::kUnknown;
  }

  HTTPMessage& message = iter->second;
  if (message.content_length != -1) {
    const int remaining_size = message.content_length - message.http_resp_body.size();
    if (remaining_size >= 0) {
      message.http_resp_body.append(buf.substr(0, remaining_size));
      message.is_complete =
          static_cast<size_t>(message.content_length) == message.http_resp_body.size();
    }
  } else {
    if (!ParseChunk(std::string(buf), &message)) {
      return ParseState::kInvalid;
    }
  }

  if (message.is_complete) {
    msgs_complete_.push_back(std::move(message));
    msgs_incomplete_.erase(iter);
    return ParseState::kSuccess;
  } else {
    msgs_incomplete_[seq_num] = std::move(message);
    msgs_incomplete_.erase(iter);
    return ParseState::kNeedsMoreData;
  }
}

std::vector<HTTPMessage> HTTPParser::ExtractHTTPMessages() { return std::move(msgs_complete_); }

}  // namespace stirling
}  // namespace pl
