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
  result.message.http_resp_body = std::string(event.msg, event.attr.msg_size);
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
    result->http_resp_body.append(buf, buf_size);
    *data = std::string_view(buf + buf_size, retval);
    // Pico claims that the last \r\n are unparsed, manually remove them.
    while (!data->empty() && (data->front() == '\r' || data->front() == '\n')) {
      data->remove_prefix(1);
    }
    return ParseState::kSuccess;
  } else if (retval == -2) {
    // Incomplete message.
    result->is_complete = false;
    result->http_resp_body.append(buf, buf_size);
    return ParseState::kNeedsMoreData;
  }
  return ParseState::kUnknown;
}

// Mutates the input data.
bool ParseChunk(std::string data, HTTPMessage* result) {
  std::string_view buf = data;
  ParseState s = ParseChunk(&buf, result);
  return s == ParseState::kSuccess || s == ParseState::kNeedsMoreData;
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
    } else {
      result->is_complete = false;
      result->http_resp_body.reserve(len);
      result->content_length = len;
      result->http_resp_body = unparsed_data;
    }
    unparsed_data.remove_prefix(std::min(static_cast<size_t>(len), unparsed_data.size()));
    return true;
  }

  const auto transfer_encoding_iter = result->http_headers.find(http_headers::kTransferEncoding);
  if (transfer_encoding_iter != result->http_headers.end() &&
      transfer_encoding_iter->second == "chunked") {
    // TODO(yzhao): Change to set default value in appending record batch instead of data for
    // parsing.
    result->http_resp_body.clear();
    result->is_chunked = true;
    ParseState s = ParseChunk(&unparsed_data, result);
    if (s != ParseState::kSuccess && s != ParseState::kNeedsMoreData) {
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
    // TODO(yzhao): This assignment overwrites the default value "-". We should move the setting of
    // default value outside of HTTP message parsing and into appending HTTP messages to record
    // batch.
    result->http_resp_body = unparsed_data;
    unparsed_data.remove_prefix(unparsed_data.size());
  }
  return true;
}

// HTTP messages are sequentially written to the file descriptor, and their sequence numbers are
// obtained accordingly. We rely on the consecutive sequence numbers to detect missing events and
// order the events correctly.
ParseState HTTPParser::ParseResponse(const socket_data_event_t& event) {
  const uint64_t seq_num = event.attr.seq_num;
  std::string_view buf(event.msg, event.attr.msg_size);
  if (absl::StartsWith(buf, "HTTP")) {
    HTTPMessage message;
    switch (pico_wrapper_.ParseResponse(buf)) {
      case ParseState::kSuccess:
        message.is_header_complete = true;
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
      case ParseState::kNeedsMoreData:
        msgs_incomplete_[seq_num] = std::move(message);
        return ParseState::kNeedsMoreData;
      case ParseState::kInvalid:
      case ParseState::kUnknown:
        return ParseState::kInvalid;
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
  } else if (message.is_chunked) {
    if (!ParseChunk(std::string(buf), &message)) {
      return ParseState::kInvalid;
    }
  } else {
    message.http_resp_body.append(buf);
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

std::pair<uint64_t, uint64_t> HTTPParser::ParseResponses() {
  std::string buf = Combine();
  std::vector<SeqHTTPMessage> seq_msgs;
  std::pair<ParseState, size_t> parse_result = Parse(buf, &seq_msgs);
  for (auto& msg : seq_msgs) {
    const uint64_t seq_num = GetSeqNum(msg.bytes_begin);
    DCHECK(seq_num >= seq_begin_ && seq_num < seq_end_) << absl::Substitute(
        "The sequence number must be in valid range of [$0, $1)", seq_begin_, seq_end_);
    msg.timestamp_ns = ts_nses_[seq_num - seq_begin_];
    if (msg.is_complete) {
      msgs_complete_.push_back(std::move(msg));
    } else {
      msgs_incomplete_.emplace(GetSeqNum(msg.bytes_end - 1), std::move(msg));
    }
  }
  parse_state_ = parse_result.first;
  // Remove all processed data afterwards. This has to be after processing the messages, as the
  // messages are needed to get the correct sequence number.
  // TODO(yzhao): Recover from parse failure. Maybe looking for "HTTP" and restart parsing.
  return RemovePrefix(parse_result.second);
}

std::vector<HTTPMessage> HTTPParser::ExtractHTTPMessages() { return std::move(msgs_complete_); }

void HTTPParser::Close() {
  for (auto& [last_seq_num, message] : msgs_incomplete_) {
    PL_UNUSED(last_seq_num);
    if (!message.is_chunked && message.content_length == -1 && message.is_header_complete) {
      message.is_complete = true;
      msgs_complete_.push_back(std::move(message));
    }
  }
  msgs_incomplete_.clear();
}

uint64_t HTTPParser::GetSeqNum(size_t pos) const {
  uint64_t curr_seq = seq_begin_;
  uint64_t size = 0;
  for (auto msg : msgs_) {
    size += msg.size();
    if (pos < size) {
      return curr_seq;
    }
    ++curr_seq;
  }
  return 0;
}

std::pair<uint64_t, uint64_t> HTTPParser::RemovePrefix(size_t size) {
  DCHECK(size <= msgs_size_) << "Prefix size is too long, maximal size: " << msgs_size_
                             << " got: " << size;
  const uint64_t begin = seq_begin_;
  msgs_size_ -= size;
  while (size > 0 && !msgs_.empty()) {
    const size_t l = std::min(size, msgs_.front().size());
    msgs_.front().remove_prefix(l);
    if (msgs_.front().empty()) {
      msgs_.erase(msgs_.begin());
      ts_nses_.erase(ts_nses_.begin());
      ++seq_begin_;
    }
    size -= l;
  }
  return std::make_pair(begin, seq_begin_);
}

bool HTTPParser::Append(uint64_t seq_num, uint64_t ts_ns, std::string_view msg) {
  // TODO(yzhao): Simplify this by resetting parser appropriately.
  if (seq_num >= seq_begin_ && seq_num < seq_end_) {
    // Skip duplicate events.
    return true;
  }
  if (seq_begin_ >= seq_end_) {
    seq_begin_ = seq_num;
    seq_end_ = seq_num + 1;
  } else if (seq_num != seq_end_) {
    return false;
  } else {
    ++seq_end_;
  }
  msgs_.push_back(msg);
  ts_nses_.push_back(ts_ns);
  msgs_size_ += msg.size();
  return true;
}

std::string HTTPParser::Combine() const {
  std::string result;
  result.reserve(msgs_size_);
  for (auto msg : msgs_) {
    result.append(msg);
  }
  return result;
}

std::pair<ParseState, size_t> Parse(std::string_view buf, std::vector<SeqHTTPMessage>* result) {
  PicoHTTPParserWrapper pico;
  const size_t buf_size = buf.size();
  while (!buf.empty()) {
    const ParseState s = pico.ParseResponse(buf);
    switch (s) {
      case ParseState::kSuccess:
      case ParseState::kUnknown:
        break;
      case ParseState::kNeedsMoreData:
        return {s, buf_size - buf.size()};
      case ParseState::kInvalid:
        // TODO(yzhao): If parse failed, just assume all are processed. This should be changed so
        // that we can restart parsing from a later position in the bytes stream.
        return {s, buf_size};
    }
    SeqHTTPMessage message;
    if (s == ParseState::kSuccess) {
      message.is_header_complete = true;
    }
    if (!pico.WriteResponse(&message)) {
      return {ParseState::kInvalid, buf_size - pico.unparsed_data.size()};
    }
    message.bytes_begin = buf_size - buf.size();
    message.bytes_end = buf_size - pico.unparsed_data.size();
    buf = pico.unparsed_data;
    result->push_back(std::move(message));
  }
  return {ParseState::kSuccess, buf_size - pico.unparsed_data.size()};
}

}  // namespace stirling
}  // namespace pl
