#pragma once

#include <map>
#include <string>

#include "src/stirling/bcc_bpf/http_trace.h"

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

// The fields corresponding exactly to HTTPTraceConnector::kElements.
// TODO(yzhao): The repetitions of information among this, DataElementsIndexes, and kElements should
// be eliminated. It might make sense to use proto file to define data schema and generate kElements
// array during runtime, based on proto schema.
struct HTTPTraceRecord {
  uint64_t time_stamp_ns = 0;
  uint32_t tgid = 0;
  uint32_t pid = 0;
  int fd = -1;
  std::string event_type = "-";
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
void ParseEventAttr(const syscall_write_event_t& event, HTTPTraceRecord* record);
bool ParseHTTPRequest(const syscall_write_event_t& event, HTTPTraceRecord* record,
                      uint64_t msg_size);
bool ParseHTTPResponse(const syscall_write_event_t& event, HTTPTraceRecord* record,
                       uint64_t msg_size);
bool ParseSockAddr(const syscall_write_event_t& event, HTTPTraceRecord* record);
bool ParseRaw(const syscall_write_event_t& event, HTTPTraceRecord* record, uint64_t msg_size);

}  // namespace stirling
}  // namespace pl
