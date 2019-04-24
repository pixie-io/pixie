#pragma once

#ifndef __linux__

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(HTTPTraceConnector);

}  // namespace stirling
}  // namespace pl

#else

#include <bcc/BPF.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "src/stirling/source_connector.h"

DECLARE_string(selected_content_type_substrs);

OBJ_STRVIEW(http_trace_bcc_script, _binary_src_stirling_bcc_bpf_http_trace_c);

namespace pl {
namespace stirling {

// The fields are exact corresponding to HTTPTraceConnector::kElements.
// TODO(yzhao): The repetitions of information among this, DataElementsIndexes, and kElements should
// be eliminated. It might make sense to use proto file to define data schema and generate kElements
// array during runtime, based on proto schema.
struct HTTPTraceRecord {
  uint64_t time_stamp_ns = 0;
  uint32_t tgid = 0;
  uint32_t pid = 0;
  int fd = -1;
  std::string event_type;
  std::string src_addr;
  int src_port = -1;
  std::string dst_addr;
  int dst_port = -1;
  int http_minor_version = -1;
  std::map<std::string, std::string> http_headers;
  std::string http_req_method;
  std::string http_req_path;
  int http_resp_status = -1;
  std::string http_resp_message;
};

class HTTPTraceConnector : public SourceConnector {
 public:
  static constexpr SourceType kSourceType = SourceType::kEBPF;
  static constexpr char kName[] = "bcc_http_trace";
  // TODO(yzhao): As of 2019-04-18, all HTTP trace data is written to one table. An option to
  // improve is to have different tables:
  // - A HTTP connection table with fields 1) id (fd+additional_data for uniqueness),
  //   2) {src,dst}_{addr,port}.
  // - A HTTP message table with fields 1) id (same as above), type (req or resp), header, payload.
  inline static const DataElements kElements = {
      DataElement("time_stamp_ns", types::DataType::TIME64NS),
      // tgid is the user space "pid".
      DataElement("tgid", types::DataType::INT64),
      DataElement("pid", types::DataType::INT64),
      DataElement("fd", types::DataType::INT64),
      DataElement("event_type", types::DataType::STRING),
      // TODO(PL-519): Eventually, use the appropriate data type to represent IP addresses, as will
      // be resolved in the Jira issue.
      DataElement("src_addr", types::DataType::STRING),
      DataElement("src_port", types::DataType::INT64),
      DataElement("dst_addr", types::DataType::STRING),
      DataElement("dst_port", types::DataType::INT64),
      DataElement("http_minor_version", types::DataType::INT64),
      DataElement("http_headers", types::DataType::STRING),
      DataElement("http_req_method", types::DataType::STRING),
      DataElement("http_req_path", types::DataType::STRING),
      DataElement("http_resp_status", types::DataType::INT64),
      DataElement("http_resp_message", types::DataType::STRING),
  };

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new HTTPTraceConnector(name));
  }
  static void HandleProbeOutput(void* cb_cookie, void* data, int /*data_size*/);
  static void HandleProbeLoss(void* /*cb_cookie*/, uint64_t);

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(types::ColumnWrapperRecordBatch* record_batch) override;

  void UpdateFdRecordMap(int fd, HTTPTraceRecord record);
  const HTTPTraceRecord& GetRecordForFd(int fd);

 private:
  explicit HTTPTraceConnector(const std::string& source_name)
      : SourceConnector(kSourceType, std::move(source_name), kElements) {}

  inline static const std::string_view kBCCScript = http_trace_bcc_script;

  ebpf::BPF bpf_;
  const int perf_buffer_page_num_ = 8;
  // A map from file descriptor to an IP:port pair. There is no race conditions as the caller is
  // single threaded.
  std::map<int, HTTPTraceRecord> fd_record_map_;
};

}  // namespace stirling
}  // namespace pl

#endif
