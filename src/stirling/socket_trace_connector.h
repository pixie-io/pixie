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

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/event_parser.h"
#include "src/stirling/http_parse.h"
#include "src/stirling/socket_connection.h"
#include "src/stirling/source_connector.h"

DECLARE_string(http_response_header_filters);

OBJ_STRVIEW(http_trace_bcc_script, _binary_bcc_bpf_socket_trace_c_preprocessed);

namespace pl {
namespace stirling {

enum class HTTPContentType {
  kUnknown = 0,
  kJSON = 1,
  // We use gRPC instead of PB to be consistent with the wording used in gRPC.
  kGRPC = 2,
};

struct DataStream {
  // Raw data events from BPF.
  // TODO(oazizi): Convert this to vector.
  std::map<uint64_t, socket_data_event_t> events;

  // To support partially processed events,
  // the stream may start at an offset in the first raw data event.
  uint64_t offset = 0;

  // Vector of parsed HTTP messages.
  // Once parsed, the raw data events should be discarded.
  std::deque<HTTPMessage> messages;
};

struct ConnectionTracker {
  SocketConnection conn;

  // TODO(oazizi): Will this be covered by conn?
  TrafficProtocol protocol;

  // The data collected by the stream, one per direction.
  DataStream send_data;
  DataStream recv_data;

  // TODO(oazizi): Add a bool to say whether the stream has been touched since last transfer (to
  // avoid useless computation).
  // TODO(oazizi): Could also record a timestamp, so we could destroy old EventStreams completely.
};

struct HTTPStream : public ConnectionTracker {
  HTTPStream() { protocol = kProtocolHTTP; }
};

struct HTTP2Stream : public ConnectionTracker {
  HTTP2Stream() { protocol = kProtocolHTTP2; }
  // TODO(yzhao): Add HTTP2Parser, or gRPC parser.
};

struct HTTPTraceRecord {
  const SocketConnection& conn;
  HTTPMessage req_message;
  HTTPMessage resp_message;
};

class SocketTraceConnector : public SourceConnector {
 public:
  inline static const std::string_view kBCCScript = http_trace_bcc_script;

  static constexpr SourceType kSourceType = SourceType::kEBPF;

  // clang-format off
  static constexpr DataElement kHTTPElements[] = {
          {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
          // tgid is the user space "pid".
          {"tgid", types::DataType::INT64, types::PatternType::GENERAL},
          // TODO(yzhao): Remove 'fd'.
          {"fd", types::DataType::INT64, types::PatternType::GENERAL},
          {"event_type", types::DataType::STRING, types::PatternType::GENERAL_ENUM},
          // TODO(PL-519): Eventually, use the appropriate data type to
          // represent IP addresses, as will be resolved in the Jira issue.
          {"remote_addr", types::DataType::STRING, types::PatternType::GENERAL},
          {"remote_port", types::DataType::INT64, types::PatternType::GENERAL},
          {"http_major_version", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
          {"http_minor_version", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
          {"http_headers", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_content_type", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
          {"http_req_method", types::DataType::STRING, types::PatternType::GENERAL_ENUM},
          {"http_req_path", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_resp_status", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
          {"http_resp_message", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_resp_body", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_resp_latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE}
  };

  static constexpr ConstStrView kHTTPPerfBufferNames[] = {
    "socket_open_conns",
    "socket_http_events",
    "socket_close_conns",
  };

  // Used in ReadPerfBuffer to drain the relevant perf buffers.
  static constexpr auto kHTTPPerfBuffers = ConstVectorView<ConstStrView>(kHTTPPerfBufferNames);

  // clang-format on
  static constexpr auto kHTTPTable = DataTableSchema("http_events", kHTTPElements);

  // clang-format off
  static constexpr DataElement kMySQLElements[] = {
          {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
          {"tgid", types::DataType::INT64, types::PatternType::GENERAL},
          {"fd", types::DataType::INT64, types::PatternType::GENERAL},
          {"bpf_event", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
          {"remote_addr", types::DataType::STRING, types::PatternType::GENERAL},
          {"remote_port", types::DataType::INT64, types::PatternType::GENERAL},
          {"body", types::DataType::STRING, types::PatternType::STRUCTURED},
  };

  static constexpr ConstStrView kMySQLPerfBufferNames[] = {
    "socket_open_conns",
    "socket_mysql_events",
    "socket_close_conns",
  };

  static constexpr auto kMySQLPerfBuffers = ConstVectorView<ConstStrView>(kMySQLPerfBufferNames);

  // clang-format on
  static constexpr auto kMySQLTable = DataTableSchema("mysql_events", kMySQLElements);

  static constexpr DataTableSchema kTablesArray[] = {kHTTPTable, kMySQLTable};
  static constexpr auto kTables = ConstVectorView<DataTableSchema>(kTablesArray);
  static constexpr uint32_t kHTTPTableNum = SourceConnector::TableNum(kTables, kHTTPTable);
  static constexpr uint32_t kMySQLTableNum = SourceConnector::TableNum(kTables, kMySQLTable);

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SocketTraceConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) override;

  Status Configure(uint32_t protocol, uint64_t config_mask);

  const std::map<uint64_t, HTTPStream>& TestOnlyHTTPStreams() const { return http_streams_; }
  const std::map<uint64_t, HTTP2Stream>& TestOnlyHTTP2Streams() const { return http2_streams_; }
  void TestOnlyConfigure(uint32_t protocol, uint64_t config_mask) {
    config_mask_[protocol] = config_mask;
  }
  static void TestOnlySetHTTPResponseHeaderFilter(HTTPHeaderFilter filter) {
    http_response_header_filter_ = std::move(filter);
  }

  // This function causes the perf buffer to be read, and triggers callbacks per message.
  // TODO(oazizi): This function is only public for testing purposes. Make private?
  void ReadPerfBuffer(uint32_t table_num);

  // Dim 0: DataTables; dim 1: perfBuffer Names
  static constexpr ConstVectorView<ConstStrView> perfBufferNames[] = {kHTTPPerfBuffers,
                                                                      kMySQLPerfBuffers};
  static constexpr auto kTablePerfBufferMap =
      ConstVectorView<ConstVectorView<ConstStrView> >(perfBufferNames);

 private:
  explicit SocketTraceConnector(std::string_view source_name)
      : SourceConnector(kSourceType, source_name, kTables, kDefaultSamplingPeriod,
                        kDefaultPushPeriod) {
    // TODO(yzhao): Is there a better place/time to grab the flags?
    http_response_header_filter_ = ParseHTTPHeaderFilters(FLAGS_http_response_header_filters);
    config_mask_.resize(kNumProtocols);
  }

  // ReadPerfBuffer poll callback functions (must be static).
  static void HandleHTTPProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleMySQLProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleHTTP2ProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleProbeLoss(void* cb_cookie, uint64_t lost);
  static void HandleCloseProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleOpenProbeOutput(void* cb_cookie, void* data, int data_size);

  // Data related events.
  // Places the event into a stream buffer to deal with reorderings.
  void AcceptEvent(socket_data_event_t event);

  // Connection related events.
  template <typename StreamType>
  auto RegisterStream(const conn_info_t& conn_info, std::map<uint64_t, StreamType>* streams,
                      uint64_t stream_id);
  template <typename StreamType>
  void AppendToStream(socket_data_event_t event, std::map<uint64_t, StreamType>* streams);
  void OpenConn(const conn_info_t& conn_info);
  void CloseConn(const conn_info_t& conn_info);
  conn_info_t* GetConn(const socket_data_event_t& event);

  // Transfers the data from stream buffers (from AcceptEvent()) to record_batch.
  void TransferStreamData(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch);

  // Transfer of an HTTP Response Event to the HTTP Response Table in the table store.
  void TransferHTTPStreams(types::ColumnWrapperRecordBatch* record_batch);

  // Takes a data stream, and parses as many HTTP messages of the specified type as it can.
  // Input data: data->events.
  // Output messages: data->messages.
  void ParseEventStream(TrafficMessageType type, DataStream* data);

  static void ConsumeHTTPMessage(HTTPTraceRecord record,
                                 types::ColumnWrapperRecordBatch* record_batch);
  static bool SelectHTTPMessage(const HTTPTraceRecord& record);
  static void AppendHTTPMessage(HTTPTraceRecord record,
                                types::ColumnWrapperRecordBatch* record_batch);

  // Transfer of a MySQL Event to the MySQL Table.
  void TransferMySQLEvent(const socket_data_event_t& event,
                          types::ColumnWrapperRecordBatch* record_batch);

  inline static HTTPHeaderFilter http_response_header_filter_;

  ebpf::BPF bpf_;

  std::map<uint64_t, HTTPStream> http_streams_;
  std::map<uint64_t, HTTP2Stream> http2_streams_;
  std::map<uint64_t, conn_info_t> connections_;
  std::map<uint64_t, StatusOr<IPEndpoint> > ip_endpoints_;

  // For MySQL tracing only. Will go away when MySQL uses streams.
  types::ColumnWrapperRecordBatch* record_batch_;

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

  static constexpr int kOffsetZero = 0;
  static inline const std::vector<ProbeSpec> kProbeSpecs = {
      {"connect", "probe_entry_connect", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"connect", "probe_ret_connect", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"accept", "probe_entry_accept", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"accept", "probe_ret_accept", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"accept4", "probe_entry_accept4", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"accept4", "probe_ret_accept4", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"write", "probe_entry_write", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"write", "probe_ret_write", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"send", "probe_entry_send", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"send", "probe_ret_send", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"sendto", "probe_entry_sendto", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"sendto", "probe_ret_sendto", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"read", "probe_entry_read", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"read", "probe_ret_read", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"recv", "probe_entry_recv", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"recv", "probe_ret_recv", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"recvfrom", "probe_entry_recv", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"recvfrom", "probe_ret_recv", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"close", "probe_close", kOffsetZero, bpf_probe_attach_type::BPF_PROBE_ENTRY},
  };
  // TODO(oazizi): Remove send and recv probes once we are confident that they don't trace anything.
  //               Note that send/recv are not in the syscall table
  //               (https://filippo.io/linux-syscall-table/), but are defined as SYSCALL_DEFINE4 in
  //               https://elixir.bootlin.com/linux/latest/source/net/socket.c.

  static constexpr uint32_t kDefaultPageCount = 8;
  static inline const std::vector<PerfBufferSpec> kPerfBufferSpecs = {
      // For data events. The order must be consistent with output tables.
      {"socket_http_events", &SocketTraceConnector::HandleHTTPProbeOutput,
       &SocketTraceConnector::HandleProbeLoss, kDefaultPageCount},
      {"socket_mysql_events", &SocketTraceConnector::HandleMySQLProbeOutput,
       &SocketTraceConnector::HandleProbeLoss, kDefaultPageCount},

      // For non-data events. Must not mix with the above perf buffers for data events.
      {"socket_open_conns", &SocketTraceConnector::HandleOpenProbeOutput,
       &SocketTraceConnector::HandleProbeLoss, kDefaultPageCount},
      {"socket_close_conns", &SocketTraceConnector::HandleCloseProbeOutput,
       &SocketTraceConnector::HandleProbeLoss, kDefaultPageCount},
  };

  std::vector<uint64_t> config_mask_;

  FRIEND_TEST(SocketTraceConnectorTest, AppendNonContiguousEvents);
  FRIEND_TEST(SocketTraceConnectorTest, NoEvents);
  FRIEND_TEST(SocketTraceConnectorTest, FilterMessages);
  FRIEND_TEST(SocketTraceConnectorTest, RequestResponseMatching);
};

}  // namespace stirling
}  // namespace pl

#endif
