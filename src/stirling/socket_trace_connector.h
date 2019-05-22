#pragma once

#ifndef __linux__

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(SocketTraceConnector);

}  // namespace stirling
}  // namespace pl

#else

#include <bcc/BPF.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/http_parse.h"
#include "src/stirling/source_connector.h"

DECLARE_string(http_response_header_filters);

OBJ_STRVIEW(http_trace_bcc_script, _binary_src_stirling_bcc_bpf_socket_trace_c);

namespace pl {
namespace stirling {

// TODO(yzhao): NEXT DIFF: Move parser field out, and create a new struct HTTPStream to include
// Stream and HTTPParser.
struct Stream {
  // The time stamp when this connection is created.
  uint64_t time_stamp_ns;
  uint32_t tgid;
  uint32_t fd;
  std::string remote_ip;
  int remote_port;
  // Key: sequence number.
  std::map<uint64_t, socket_data_event_t> data;
  HTTPParser parser;
};

class SocketTraceConnector : public SourceConnector {
 public:
  inline static const std::string_view kBCCScript = http_trace_bcc_script;

  static constexpr SourceType kSourceType = SourceType::kEBPF;
  static constexpr char kName[] = "bcc_http_trace";
  // TODO(yzhao): As of 2019-04-18, all HTTP trace data is written to one table. An option to
  // improve is to have different tables:
  // - A HTTP connection table with fields 1) id (fd+additional_data for uniqueness),
  //   2) {src,dst}_{addr,port}.
  // - A HTTP message table with fields 1) id (same as above), type (req or resp), header, payload.
  inline static const std::vector<DataTableSchema> kElements = {
      DataTableSchema(kName, {DataElement("time_", types::DataType::TIME64NS),
                              // tgid is the user space "pid".
                              DataElement("tgid", types::DataType::INT64),
                              // TODO(yzhao): Remove 'fd'.
                              DataElement("fd", types::DataType::INT64),
                              DataElement("event_type", types::DataType::STRING),
                              // TODO(PL-519): Eventually, use the appropriate data type to
                              // represent IP addresses, as will be resolved in the Jira issue.
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
                              DataElement("http_resp_latency_ns", types::DataType::INT64)})};

  // The order here must be identical to SocketTraceConnector::kElements, and it must start from 0.
  // TODO(yzhao): We probably could have some form of template construct to offload part of the
  // schema bookkeeping outside of kElements. Today we have a few major issues:
  // - When changing field order, we need to update 2 data structures: kElements,
  // DataElementsIndexes. Investigate if it's possible to use only one data structure.
  // - When runtime check failed, the error information does not show the field index.
  // Investigate if it's possible to enforce the check during compilation time.
  enum DataElementsIndexes {
    kTimeStampNs = 0,
    kTgid,
    kFd,
    kEventType,
    kSrcAddr,
    kSrcPort,
    kDstAddr,
    kDstPort,
    kHTTPMinorVersion,
    kHTTPHeaders,
    kHTTPReqMethod,
    kHTTPReqPath,
    kHTTPRespStatus,
    kHTTPRespMessage,
    kHTTPRespBody,
    kHTTPRespLatencyNs,
  };

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{5000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new SocketTraceConnector(name));
  }
  static void HandleProbeOutput(void* cb_cookie, void* data, int /*data_size*/);
  static void HandleProbeLoss(void* /*cb_cookie*/, uint64_t);

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) override;

  void PollPerfBuffer(uint32_t table_num);
  void AcceptEvent(socket_data_event_t event);

  const std::map<uint64_t, Stream>& TestOnlyStreams() const { return streams_; }
  static void TestOnlySetHTTPResponseHeaderFilter(HTTPHeaderFilter filter) {
    http_response_header_filter_ = std::move(filter);
  }

 private:
  explicit SocketTraceConnector(const std::string& source_name)
      : SourceConnector(kSourceType, std::move(source_name), kElements, kDefaultSamplingPeriod,
                        kDefaultPushPeriod) {
    // TODO(oazizi): Is there a better place/time to grab the flags?
    http_response_header_filter_ = ParseHTTPHeaderFilters(FLAGS_http_response_header_filters);
  }

  inline static HTTPHeaderFilter http_response_header_filter_;

  // Helper functions used by HandleProbeOutput().
  static bool SelectForAppend(const HTTPTraceRecord& record);
  static void AppendToRecordBatch(HTTPTraceRecord record,
                                  types::ColumnWrapperRecordBatch* record_batch);
  static void ConsumeRecord(HTTPTraceRecord record, types::ColumnWrapperRecordBatch* record_batch);

  ebpf::BPF bpf_;

  std::map<uint64_t, Stream> streams_;
};

}  // namespace stirling
}  // namespace pl

#endif
