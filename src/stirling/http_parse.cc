#include "src/stirling/http_parse.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <picohttpparser.h>
#include <algorithm>
#include <utility>

#include "src/common/zlib/zlib_wrapper.h"

namespace pl {
namespace stirling {

void ParseMessageBodyChunked(HTTPTraceRecord* record) {
  if (record->http_resp_body.empty()) {
    return;
  }
  phr_chunked_decoder decoder = {};
  char* buf = const_cast<char*>(record->http_resp_body.data());
  size_t buf_size = record->http_resp_body.size();
  ssize_t retval = phr_decode_chunked(&decoder, buf, &buf_size);
  if (retval != -1) {
    // As long as the parse succeeded (-1 indicate parse failure), buf_size is the decoded data
    // size (even if it's incomplete).
    record->http_resp_body.resize(buf_size);
  }
  if (retval >= 0) {
    record->chunking_status = ChunkingStatus::kComplete;
  } else {
    record->chunking_status = ChunkingStatus::kChunked;
  }
}

void PreProcessRecord(HTTPTraceRecord* record) {
  auto content_encoding_iter = record->http_headers.find(http_header_keys::kContentEncoding);
  // Replace body with decompressed version, if required.
  if (content_encoding_iter != record->http_headers.end() &&
      content_encoding_iter->second == "gzip") {
    std::string_view body_strview(record->http_resp_body.c_str(), record->http_resp_body.size());
    auto bodyOrErr = pl::zlib::StrInflate(body_strview);
    if (!bodyOrErr.ok()) {
      LOG(WARNING) << "Unable to gunzip HTTP body.";
      record->http_resp_body = "<Stirling failed to gunzip body>";
    } else {
      record->http_resp_body = bodyOrErr.ValueOrDie();
    }
  }
}

void ParseEventAttr(const socket_data_event_t& event, HTTPTraceRecord* record) {
  record->time_stamp_ns = event.attr.time_stamp_ns;
  record->tgid = event.attr.tgid;
  record->pid = event.attr.pid;
  record->fd = event.attr.fd;
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
  const uint64_t msg_size = std::min(event.attr.msg_bytes, event.attr.msg_buf_size);
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
    result.event_type = HTTPTraceEventType::kHTTPRequest;
    result.http_minor_version = minor_version;
    result.http_headers = GetHttpHeadersMap(headers, num_headers);
    result.http_req_method = std::string(method, method_len);
    result.http_req_path = std::string(path, path_len);
    return true;
  }
  return false;
}

// TODO(PL-519): Now we discard anything of the response that are not http headers. This is because
// we cannot associate a write() call with the http response. The future work is to keep a list of
// captured data from write() and associate them with the same http response. The rough idea looks
// like as follows:
// time         event type
// t0           write() http response #1 header + body
// t1           write() http response #1 body
// t2           write() http response #1 body
// t3           write() http response #2 header + body
// t4           write() http response #2 body
// ...
//
// We then can squash events at t0, t1, t2 together and concatenate their bodies as the full http
// message. This works in http 1.1 because the responses and requests are not interleaved.
bool ParseHTTPResponse(const socket_data_event_t& event, HTTPTraceRecord* record) {
  const uint64_t msg_size = std::min(event.attr.msg_bytes, event.attr.msg_buf_size);
  const char* msg = nullptr;
  size_t msg_len = 0;
  int minor_version = 0;
  int status = 0;
  size_t num_headers = 10;
  struct phr_header headers[num_headers];
  const int bytes_processed = phr_parse_response(event.msg, msg_size, &minor_version, &status, &msg,
                                                 &msg_len, headers, &num_headers, /*last_len*/ 0);

  if (bytes_processed > 0) {
    HTTPTraceRecord& result = *record;
    ParseEventAttr(event, &result);
    result.event_type = HTTPTraceEventType::kHTTPResponse;
    result.http_minor_version = minor_version;
    result.http_headers = GetHttpHeadersMap(headers, num_headers);
    result.http_resp_status = status;
    result.http_resp_message = std::string(msg, msg_len);
    result.http_resp_body = std::string(event.msg + bytes_processed, msg_size - bytes_processed);
    return true;
  }
  return false;
}

// Parses an IP:port pair from the event input into the provided record.
// Returns false if an unexpected sockaddr family is provided.
// Currently this function understands IPV4 and IPV6 sockaddr families.
bool ParseSockAddr(const socket_data_event_t& event, HTTPTraceRecord* record) {
  const auto* sa = reinterpret_cast<const struct sockaddr*>(&event.attr.accept_info.addr);
  char s[INET6_ADDRSTRLEN] = "";
  const auto* sa_in = reinterpret_cast<const struct sockaddr_in*>(sa);
  const auto* sa_in6 = reinterpret_cast<const struct sockaddr_in6*>(sa);
  std::string ip;
  int port = -1;
  switch (sa->sa_family) {
    case AF_INET:
      port = sa_in->sin_port;
      if (inet_ntop(AF_INET, &sa_in->sin_addr, s, INET_ADDRSTRLEN) != nullptr) {
        ip.assign(s);
      }
      break;
    case AF_INET6:
      port = sa_in6->sin6_port;
      if (inet_ntop(AF_INET6, &sa_in6->sin6_addr, s, INET6_ADDRSTRLEN) != nullptr) {
        ip.assign(s);
      }
      break;
    default:
      LOG(WARNING) << "Ignoring unhandled sockaddr family: " << sa->sa_family;
      return false;
  }
  if (!ip.empty()) {
    record->dst_addr = std::move(ip);
    record->dst_port = port;
  }
  return true;
}

bool ParseRaw(const socket_data_event_t& event, HTTPTraceRecord* record) {
  const uint64_t msg_size = std::min(event.attr.msg_bytes, event.attr.msg_buf_size);
  HTTPTraceRecord& result = *record;
  ParseEventAttr(event, &result);
  result.event_type = HTTPTraceEventType::kUnknown;
  result.http_resp_body = std::string(event.msg, msg_size);
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
    for (auto [http_header, substr] : filter.inclusions) {  // NOLINT(whitespace/braces)
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
    for (auto [http_header, substr] : filter.exclusions) {  // NOLINT(whitespace/braces)
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

}  // namespace stirling
}  // namespace pl
