#pragma once

#include <map>
#include <string>

namespace pl {
namespace stirling {

enum class ChunkingStatus {
  UNSPECIFIED,
  CHUNKED,
  COMPLETE,
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
  ChunkingStatus chunking_status = ChunkingStatus::UNSPECIFIED;
};

/**
 * @brief Parses the message body assuming it's encoded with 'Transfer-Encoding: chunked'.
 * Writes a bool to indicate if the message body surpasses the end of the entire message.
 *
 * @param record The input and result.
 */
void ParseMessageBodyChunked(HTTPTraceRecord* record);

}  // namespace stirling
}  // namespace pl
