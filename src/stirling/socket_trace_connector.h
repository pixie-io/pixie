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
#include <unordered_map>
#include <utility>
#include <vector>

#include "demos/applications/hipster_shop/reflection.h"
#include "src/common/grpcutils/service_descriptor_database.h"
#include "src/stirling/bcc_wrapper.h"
#include "src/stirling/connection_tracker.h"
#include "src/stirling/socket_trace.h"

DECLARE_string(http_response_header_filters);
DECLARE_bool(enable_parsing_protobufs);
DECLARE_uint32(stirling_socket_trace_sampling_period_millis);

OBJ_STRVIEW(http_trace_bcc_script, _binary_bcc_bpf_socket_trace_c_preprocessed);

namespace pl {
namespace stirling {

enum class HTTPContentType {
  kUnknown = 0,
  kJSON = 1,
  // We use gRPC instead of PB to be consistent with the wording used in gRPC.
  kGRPC = 2,
};

class SocketTraceConnector : public SourceConnector, public BCCWrapper {
 public:
  inline static const std::string_view kBCCScript = http_trace_bcc_script;

  // clang-format off
  static constexpr DataElement kHTTPElements[] = {
          {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
          {"pid", types::DataType::INT64, types::PatternType::GENERAL},
          // TODO(oazizi): Merge with pid, and use INT128, when available.
          {"pid_start_time", types::DataType::INT64, types::PatternType::GENERAL},
          // TODO(PL-519): Eventually, use uint128 to represent IP addresses, as will be resolved in
          // the Jira issue.
          {"remote_addr", types::DataType::STRING, types::PatternType::GENERAL},
          {"remote_port", types::DataType::INT64, types::PatternType::GENERAL},
          {"http_major_version", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
          {"http_minor_version", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
          {"http_content_type", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
          {"http_req_headers", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_req_method", types::DataType::STRING, types::PatternType::GENERAL_ENUM},
          {"http_req_path", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_req_body", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_resp_headers", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_resp_status", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
          {"http_resp_message", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_resp_body", types::DataType::STRING, types::PatternType::STRUCTURED},
          {"http_resp_latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE}
  };
  // clang-format on
  static constexpr auto kHTTPTable = DataTableSchema("http_events", kHTTPElements);

  static constexpr std::string_view kHTTPPerfBufferNames[] = {
      "socket_open_conns",
      "socket_http_events",
      "socket_close_conns",
  };

  // Used in ReadPerfBuffer to drain the relevant perf buffers.
  static constexpr auto kHTTPPerfBuffers = ConstVectorView<std::string_view>(kHTTPPerfBufferNames);

  // clang-format off
  static constexpr DataElement kMySQLElements[] = {
          {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
          {"pid", types::DataType::INT64, types::PatternType::GENERAL},
          {"pid_start_time", types::DataType::INT64, types::PatternType::GENERAL},
          {"fd", types::DataType::INT64, types::PatternType::GENERAL},
          {"remote_addr", types::DataType::STRING, types::PatternType::GENERAL},
          {"remote_port", types::DataType::INT64, types::PatternType::GENERAL},
          {"body", types::DataType::STRING, types::PatternType::STRUCTURED},
  };
  // clang-format on
  static constexpr auto kMySQLTable = DataTableSchema("mysql_events", kMySQLElements);

  static constexpr std::string_view kMySQLPerfBufferNames[] = {
      "socket_open_conns",
      "socket_mysql_events",
      "socket_close_conns",
  };

  static constexpr auto kMySQLPerfBuffers =
      ConstVectorView<std::string_view>(kMySQLPerfBufferNames);

  static constexpr DataTableSchema kTablesArray[] = {kHTTPTable, kMySQLTable};
  static constexpr auto kTables = ConstVectorView<DataTableSchema>(kTablesArray);
  static constexpr uint32_t kHTTPTableNum = SourceConnector::TableNum(kTables, kHTTPTable);
  static constexpr uint32_t kMySQLTableNum = SourceConnector::TableNum(kTables, kMySQLTable);

  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  // Dim 0: DataTables; dim 1: perfBuffer Names
  static constexpr ConstVectorView<std::string_view> perfBufferNames[] = {kHTTPPerfBuffers,
                                                                          kMySQLPerfBuffers};
  static constexpr auto kTablePerfBufferMap =
      ConstVectorView<ConstVectorView<std::string_view> >(perfBufferNames);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SocketTraceConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

  Status Configure(TrafficProtocol protocol, uint64_t config_mask);
  Status TestOnlySetTargetPID(int64_t pid);

  /**
   * @brief Number of active ConnectionTrackers.
   *
   * Note: Multiple ConnectionTrackers on same TGID+FD are counted as 1.
   */
  size_t NumActiveConnections() const { return connection_trackers_.size(); }

  /**
   * @brief Gets a pointer to a ConnectionTracker by conn_id.
   *
   * @param connid The connection to get.
   * @return Pointer to the ConnectionTracker, or nullptr if it does not exist.
   */
  const ConnectionTracker* GetConnectionTracker(struct conn_id_t connid) const;

  void TestOnlyConfigure(uint32_t protocol, uint64_t config_mask) {
    config_mask_[protocol] = config_mask;
  }
  static void TestOnlySetHTTPResponseHeaderFilter(HTTPHeaderFilter filter) {
    http_response_header_filter_ = std::move(filter);
  }

  // This function causes the perf buffer to be read, and triggers callbacks per message.
  // TODO(oazizi): This function is only public for testing purposes. Make private?
  void ReadPerfBuffer(uint32_t table_num);

 private:
  // ReadPerfBuffer poll callback functions (must be static).
  // These are used by the static variables below, and have to be placed here.
  static void HandleHTTPProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleHTTPProbeLoss(void* cb_cookie, uint64_t lost);
  static void HandleMySQLProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleMySQLProbeLoss(void* cb_cookie, uint64_t lost);
  static void HandleCloseProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleCloseProbeLoss(void* cb_cookie, uint64_t lost);
  static void HandleOpenProbeOutput(void* cb_cookie, void* data, int data_size);
  static void HandleOpenProbeLoss(void* cb_cookie, uint64_t lost);

  static constexpr ProbeSpec kProbeSpecsArray[] = {
      {"connect", "syscall__probe_entry_connect", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"connect", "syscall__probe_ret_connect", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"accept", "syscall__probe_entry_accept", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"accept", "syscall__probe_ret_accept", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"accept4", "syscall__probe_entry_accept4", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"accept4", "syscall__probe_ret_accept4", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"write", "syscall__probe_entry_write", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"write", "syscall__probe_ret_write", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"writev", "syscall__probe_entry_writev", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"writev", "syscall__probe_ret_writev", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"send", "syscall__probe_entry_send", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"send", "syscall__probe_ret_send", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"sendto", "syscall__probe_entry_sendto", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"sendto", "syscall__probe_ret_sendto", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"sendmsg", "syscall__probe_entry_sendmsg", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"sendmsg", "syscall__probe_ret_sendmsg", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"read", "syscall__probe_entry_read", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"read", "syscall__probe_ret_read", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"readv", "syscall__probe_entry_readv", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"readv", "syscall__probe_ret_readv", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"recv", "syscall__probe_entry_recv", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"recv", "syscall__probe_ret_recv", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"recvfrom", "syscall__probe_entry_recv", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"recvfrom", "syscall__probe_ret_recv", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"recvmsg", "syscall__probe_entry_recvmsg", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"recvmsg", "syscall__probe_ret_recvmsg", bpf_probe_attach_type::BPF_PROBE_RETURN},
      {"close", "syscall__probe_close", bpf_probe_attach_type::BPF_PROBE_ENTRY},
  };
  static constexpr auto kProbeSpecs = ConstVectorView<ProbeSpec>(kProbeSpecsArray);

  // TODO(oazizi): Remove send and recv probes once we are confident that they don't trace anything.
  //               Note that send/recv are not in the syscall table
  //               (https://filippo.io/linux-syscall-table/), but are defined as SYSCALL_DEFINE4 in
  //               https://elixir.bootlin.com/linux/latest/source/net/socket.c.

  static constexpr PerfBufferSpec kPerfBufferSpecsArray[] = {
      // For data events. The order must be consistent with output tables.
      {"socket_http_events", &SocketTraceConnector::HandleHTTPProbeOutput,
       &SocketTraceConnector::HandleHTTPProbeLoss},
      {"socket_mysql_events", &SocketTraceConnector::HandleMySQLProbeOutput,
       &SocketTraceConnector::HandleMySQLProbeLoss},

      // For non-data events. Must not mix with the above perf buffers for data events.
      {"socket_open_conns", &SocketTraceConnector::HandleOpenProbeOutput,
       &SocketTraceConnector::HandleOpenProbeLoss},
      {"socket_close_conns", &SocketTraceConnector::HandleCloseProbeOutput,
       &SocketTraceConnector::HandleCloseProbeLoss},
  };
  static constexpr auto kPerfBufferSpecs = ConstVectorView<PerfBufferSpec>(kPerfBufferSpecsArray);

  inline static HTTPHeaderFilter http_response_header_filter_;
  // TODO(yzhao): We will remove this once finalized the mechanism of lazy protobuf parse.
  inline static ::pl::grpc::ServiceDescriptorDatabase grpc_desc_db_{
      demos::hipster_shop::GetFileDescriptorSet()};
  inline static const size_t kCPUCount = ebpf::BPFTable::get_possible_cpu_count();

  explicit SocketTraceConnector(std::string_view source_name)
      : SourceConnector(
            source_name, kTables,
            std::chrono::milliseconds(FLAGS_stirling_socket_trace_sampling_period_millis),
            kDefaultPushPeriod),
        BCCWrapper(kBCCScript) {
    // TODO(yzhao): Is there a better place/time to grab the flags?
    http_response_header_filter_ = ParseHTTPHeaderFilters(FLAGS_http_response_header_filters);
    config_mask_.resize(kNumProtocols);
  }

  // Events from BPF.
  // TODO(oazizi/yzhao): These all operate based on pass-by-value, which copies.
  //                     The Handle* functions should call make_unique() of new corresponding
  //                     objects, and these functions should take unique_ptrs.
  void AcceptDataEvent(std::unique_ptr<SocketDataEvent> event);
  void AcceptOpenConnEvent(conn_info_t conn_info);
  void AcceptCloseConnEvent(conn_info_t conn_info);

  // Transfer of messages to the data table.
  template <class TMessageType>
  void TransferStreams(TrafficProtocol protocol, DataTable* data_table);

  template <class TMessageType>
  static void AppendMessage(TraceRecord<TMessageType> record, DataTable* data_table);

  // HTTP-specific helper function.
  static bool SelectMessage(const TraceRecord<HTTPMessage>& record);

  // Transfer of a MySQL Event to the MySQL Table.
  // TODO(oazizi/yzhao): Change to use std::unique_ptr.
  void TransferMySQLEvent(SocketDataEvent event, DataTable* data_table);

  // Note that the inner map cannot be a vector, because there is no guaranteed order
  // in which events are read from perf buffers.
  // Inner map could be a priority_queue, but benchmarks showed better performance with a std::map.
  // Key is {PID, FD} for outer map (see GetStreamId()), and generation for inner map.
  std::unordered_map<uint64_t, std::map<uint64_t, ConnectionTracker> > connection_trackers_;

  // For MySQL tracing only. Will go away when MySQL uses streams.
  DataTable* data_table_ = nullptr;

  std::vector<uint64_t> config_mask_;

  FRIEND_TEST(SocketTraceConnectorTest, AppendNonContiguousEvents);
  FRIEND_TEST(SocketTraceConnectorTest, NoEvents);
  FRIEND_TEST(SocketTraceConnectorTest, End2end);
  FRIEND_TEST(SocketTraceConnectorTest, RequestResponseMatching);
  FRIEND_TEST(SocketTraceConnectorTest, MissingEventInStream);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupInOrder);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupOutOfOrder);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupMissingDataEvent);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupOldGenerations);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupInactiveDead);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupInactiveAlive);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupNoProtocol);
};

}  // namespace stirling
}  // namespace pl

#endif
