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

  // clang-format off
  static constexpr DataElement kHTTPElements[] = {
                       DataElement("time_", types::DataType::TIME64NS),
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
                       DataElement("http_resp_latency_ns", types::DataType::INT64)
  };
  // clang-format on
  static constexpr auto kHTTPTable = DataTableSchema("socket_trace", kHTTPElements);

  // clang-format off
  static constexpr DataElement kMySQLElements[] = {
           DataElement("time_", types::DataType::TIME64NS),
           DataElement("tgid", types::DataType::INT64), DataElement("fd", types::DataType::INT64),
           DataElement("bpf_event", types::DataType::INT64),
           DataElement("src_addr", types::DataType::STRING),
           DataElement("src_port", types::DataType::INT64),
           DataElement("dst_addr", types::DataType::STRING),
           DataElement("dst_port", types::DataType::INT64),
           DataElement("body", types::DataType::STRING),
  };
  // clang-format on
  static constexpr auto kMySQLTable = DataTableSchema("mysql_trace", kMySQLElements);

  static constexpr DataTableSchema kTablesArray[] = {kHTTPTable, kMySQLTable};
  static constexpr auto kTables = ConstVectorView<DataTableSchema>(kTablesArray);

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{5000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SocketTraceConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) override;

  const std::map<uint64_t, HTTPStream>& TestOnlyHTTPStreams() const { return http_streams_; }
  static void TestOnlySetHTTPResponseHeaderFilter(HTTPHeaderFilter filter) {
    http_response_header_filter_ = std::move(filter);
  }

  // This function causes the perf buffer to be read, and triggers callbacks per message.
  // TODO(oazizi): This function is only public for testing purposes. Make private?
  void ReadPerfBuffer(uint32_t table_num);

 private:
  explicit SocketTraceConnector(std::string_view source_name)
      : SourceConnector(kSourceType, source_name, kTables, kDefaultSamplingPeriod,
                        kDefaultPushPeriod) {
    // TODO(yzhao): Is there a better place/time to grab the flags?
    http_response_header_filter_ = ParseHTTPHeaderFilters(FLAGS_http_response_header_filters);
  }

  // ReadPerfBuffer poll callback functions (must be static).
  static void HandleHTTPResponseProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleMySQLProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleProbeLoss(void* cb_cookie, uint64_t lost);

  // Places the event into a stream buffer to deal with reorderings.
  void AcceptEvent(socket_data_event_t event);

  // Transfers the data from stream buffers (from AcceptEvent()) to record_batch.
  void TransferStreamData(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch);

  // Transfer of an HTTP Response Event to the HTTP Response Table in the table store.
  void TransferHTTPResponseStreams(types::ColumnWrapperRecordBatch* record_batch);
  static void ConsumeHTTPResponse(HTTPTraceRecord record,
                                  types::ColumnWrapperRecordBatch* record_batch);
  static bool SelectHTTPResponse(const HTTPTraceRecord& record);
  static void AppendHTTPResponse(HTTPTraceRecord record,
                                 types::ColumnWrapperRecordBatch* record_batch);

  // Transfer of a MySQL Event to the MySQL Table.
  void TransferMySQLEvent(const socket_data_event_t& event,
                          types::ColumnWrapperRecordBatch* record_batch);

  FRIEND_TEST(HandleProbeOutputTest, FilterMessages);
  inline static HTTPHeaderFilter http_response_header_filter_;

  ebpf::BPF bpf_;

  std::map<uint64_t, HTTPStream> http_streams_;

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

  static inline const std::vector<ProbeSpec> kProbeSpecs = {
      {"accept4", "probe_entry_accept4", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"accept4", "probe_ret_accept4", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"write", "probe_entry_write", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"write", "probe_ret_write", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"send", "probe_entry_send", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"send", "probe_ret_send", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"sendto", "probe_entry_sendto", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"sendto", "probe_ret_sendto", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      // TODO(oazizi): Enable probing on reads, when tested.
      //    {"read", "probe_entry_read", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      //    {"read", "probe_ret_read", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"recv", "probe_entry_recv", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"recv", "probe_ret_recv", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"recvfrom", "probe_entry_recv", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"recvfrom", "probe_ret_recv", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"close", "probe_close", 0, bpf_probe_attach_type::BPF_PROBE_ENTRY},
  };
  // TODO(oazizi): Remove send and recv probes once we are confident that they don't trace anything.
  //               Note that send/recv are not in the syscall table
  //               (https://filippo.io/linux-syscall-table/), but are defined as SYSCALL_DEFINE4 in
  //               https://elixir.bootlin.com/linux/latest/source/net/socket.c.

  // Indexed by table_num from kTables (one-to-one mapping).
  static inline const std::vector<PerfBufferSpec> kPerfBufferSpecs = {
      {"socket_http_resp_events", &SocketTraceConnector::HandleHTTPResponseProbeOutput,
       &SocketTraceConnector::HandleProbeLoss,
       /* num_pages */ 8},
      {"socket_mysql_events", &SocketTraceConnector::HandleMySQLProbeOutput,
       &SocketTraceConnector::HandleProbeLoss,
       /* num_pages */ 8},
  };
};

}  // namespace stirling
}  // namespace pl

#endif
