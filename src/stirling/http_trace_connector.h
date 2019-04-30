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
#include <vector>

#include "src/stirling/bcc_bpf/http_trace.h"
#include "src/stirling/http_parse.h"
#include "src/stirling/source_connector.h"

DECLARE_string(selected_content_type_substrs);

OBJ_STRVIEW(http_trace_bcc_script, _binary_src_stirling_bcc_bpf_http_trace_c);

namespace pl {
namespace stirling {

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
      DataElement("time_", types::DataType::TIME64NS),
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
      DataElement("http_resp_body", types::DataType::STRING),
      DataElement("http_resp_latency_ns", types::DataType::INT64),
  };

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{5000};
  static types::ColumnWrapperRecordBatch* g_record_batch_;

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new HTTPTraceConnector(name));
  }
  static void HandleProbeOutput(void* cb_cookie, void* data, int /*data_size*/,
                                types::ColumnWrapperRecordBatch* record_batch);
  static void HandleProbeLoss(void* /*cb_cookie*/, uint64_t);

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(types::ColumnWrapperRecordBatch* record_batch) override;

  void UpdateFdRecordMap(uint64_t tgid, uint64_t fd, HTTPTraceRecord record);
  const HTTPTraceRecord& GetRecordForFd(uint64_t tgid, uint64_t fd);

 private:
  explicit HTTPTraceConnector(const std::string& source_name)
      : SourceConnector(kSourceType, std::move(source_name), kElements) {
    // TODO(oazizi): Is there a better place/time to grab the flags?
    filter_substrs_ = absl::StrSplit(FLAGS_selected_content_type_substrs, ",", absl::SkipEmpty());
  }

  inline static const std::string_view kBCCScript = http_trace_bcc_script;

  FRIEND_TEST(HandleProbeOutputTest, FilterMessages);
  inline static std::vector<std::string_view> filter_substrs_;

  // Helper functions used by HandleProbeOutput().
  static bool SelectForAppend(const HTTPTraceRecord& record);
  static void PreprocessRecord(HTTPTraceRecord* record);
  static void AppendToRecordBatch(HTTPTraceRecord record,
                                  types::ColumnWrapperRecordBatch* record_batch);
  static void ConsumeRecord(HTTPTraceRecord record, types::ColumnWrapperRecordBatch* record_batch);

  // Parse functions used by HandleProbeOutput().
  static void ParseEventAttr(const syscall_write_event_t& event, HTTPTraceRecord* record);
  static bool ParseHTTPRequest(const syscall_write_event_t& event, HTTPTraceRecord* record,
                               uint64_t msg_size);
  static bool ParseHTTPResponse(const syscall_write_event_t& event, HTTPTraceRecord* record,
                                uint64_t msg_size);
  static bool ParseSockAddr(const syscall_write_event_t& event, HTTPTraceRecord* record);
  static bool ParseRaw(const syscall_write_event_t& event, HTTPTraceRecord* record,
                       uint64_t msg_size);

  ebpf::BPF bpf_;
  const int perf_buffer_page_num_ = 8;
  // A map from file descriptor to an IP:port pair. There is no race conditions as the caller is
  // single threaded.
  std::map<uint64_t, HTTPTraceRecord> fd_record_map_;
};

}  // namespace stirling
}  // namespace pl

#endif
