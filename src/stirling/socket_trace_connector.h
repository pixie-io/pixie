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
#include "src/stirling/socket_connection.h"
#include "src/stirling/source_connector.h"

DECLARE_string(http_response_header_filters);

OBJ_STRVIEW(http_trace_bcc_script, _binary_src_stirling_bcc_bpf_socket_trace_c);

namespace pl {
namespace stirling {

struct HTTPStream {
  SocketConnection conn;
  // Key: sequence number.
  std::map<uint64_t, socket_data_event_t> events;
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

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) override;

  // TODO(oazizi): These are only public for the sake of tests. Should be private?
  void PollPerfBuffer(uint32_t table_num);
  void AcceptEvent(socket_data_event_t event);

  const std::map<uint64_t, HTTPStream>& TestOnlyHTTPStreams() const { return http_streams_; }
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

  static void HandleProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleProbeLoss(void* cb_cookie, uint64_t);

  // Helper functions used by HandleProbeOutput().
  static bool SelectForAppend(const HTTPTraceRecord& record);
  static void AppendToRecordBatch(HTTPTraceRecord record,
                                  types::ColumnWrapperRecordBatch* record_batch);
  static void ConsumeRecord(HTTPTraceRecord record, types::ColumnWrapperRecordBatch* record_batch);

  ebpf::BPF bpf_;

  std::map<uint64_t, HTTPStream> http_streams_;

  // Describes a kprobe that should be attached with the BPF::attach_kprobe().
  struct ProbeSpec {
    std::string kernel_fn_short_name;
    std::string trace_fn_name;
    int kernel_fn_offset;
    bpf_probe_attach_type attach_type;
  };

  struct PerfBufferSpec {
    // Name is same as the perf buffer inside bcc_bpf/socket_trace.c.
    std::string name;
    perf_reader_raw_cb probe_output_fn;
    perf_reader_lost_cb probe_loss_fn;
    uint32_t num_pages;
  };

  static inline const std::vector<ProbeSpec> kProbeSpecs = {
      {"accept4", "probe_entry_accept4", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"accept4", "probe_ret_accept4", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"write", "probe_entry_write", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"write", "probe_ret_write", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"send", "probe_entry_send", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"send", "probe_ret_send", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"sendto", "probe_entry_sendto", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"sendto", "probe_ret_sendto", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"close", "probe_close", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
  };

  static inline const std::vector<PerfBufferSpec> kPerfBufferSpecs = {
      {"socket_http_resp_events", &SocketTraceConnector::HandleProbeOutput,
       &SocketTraceConnector::HandleProbeLoss,
       /* num_pages */ 8}};
};

}  // namespace stirling
}  // namespace pl

#endif
