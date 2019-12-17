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
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "demos/applications/hipster_shop/reflection.h"
#include "src/common/grpcutils/service_descriptor_database.h"
#include "src/common/system/socket_info.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/common/socket_trace.h"
#include "src/stirling/connection_tracker.h"
#include "src/stirling/http_table.h"
#include "src/stirling/mysql_table.h"
#include "src/stirling/source_connector.h"

DECLARE_string(http_response_header_filters);
DECLARE_bool(stirling_enable_parsing_protobufs);
DECLARE_uint32(stirling_socket_trace_sampling_period_millis);
DECLARE_string(perf_buffer_events_output_path);
DECLARE_bool(stirling_enable_http_tracing);
DECLARE_bool(stirling_enable_grpc_kprobe_tracing);
DECLARE_bool(stirling_enable_grpc_uprobe_tracing);
DECLARE_bool(stirling_enable_mysql_tracing);
DECLARE_bool(stirling_disable_self_tracing);
DECLARE_bool(stirling_use_packaged_headers);

BCC_SRC_STRVIEW(http_trace_bcc_script, socket_trace);

namespace pl {
namespace stirling {

enum class HTTPContentType {
  kUnknown = 0,
  kJSON = 1,
  // We use gRPC instead of PB to be consistent with the wording used in gRPC.
  kGRPC = 2,
};

class SocketTraceConnector : public SourceConnector, public bpf_tools::BCCWrapper {
 public:
  inline static const std::string_view kBCCScript = http_trace_bcc_script;

  static constexpr std::string_view kHTTPPerfBufferNames[] = {
      "socket_control_events",
      "socket_data_events",
      "go_grpc_header_events",
      "go_grpc_data_events",
  };

  // Used in ReadPerfBuffer to drain the relevant perf buffers.
  static constexpr auto kHTTPPerfBuffers = ArrayView<std::string_view>(kHTTPPerfBufferNames);

  static constexpr std::string_view kMySQLPerfBufferNames[] = {
      "socket_control_events",
      "socket_data_events",
  };

  static constexpr auto kMySQLPerfBuffers = ArrayView<std::string_view>(kMySQLPerfBufferNames);

  static constexpr DataTableSchema kTablesArray[] = {kHTTPTable, kMySQLTable};
  static constexpr auto kTables = ArrayView<DataTableSchema>(kTablesArray);
  static constexpr uint32_t kHTTPTableNum = SourceConnector::TableNum(kTables, kHTTPTable);
  static constexpr uint32_t kMySQLTableNum = SourceConnector::TableNum(kTables, kMySQLTable);

  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  // Dim 0: DataTables; dim 1: perfBuffer Names
  static constexpr ArrayView<std::string_view> perfBufferNames[] = {kHTTPPerfBuffers,
                                                                    kMySQLPerfBuffers};
  // TODO(yzhao/oazizi): This is no longer necessary because different tables now pull data from the
  // same set of perf buffers. But we'd need to think about how to adapt the APIs with the table_num
  // argument.
  static constexpr auto kTablePerfBufferMap =
      ArrayView<ArrayView<std::string_view> >(perfBufferNames);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SocketTraceConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

  Status Configure(TrafficProtocol protocol, uint64_t config_mask);
  Status TestOnlySetTargetPID(int64_t pid);
  Status DisableSelfTracing();

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

  static void TestOnlySetHTTPResponseHeaderFilter(http::HTTPHeaderFilter filter) {
    http_response_header_filter_ = std::move(filter);
  }

  // This function causes the perf buffer to be read, and triggers callbacks per message.
  // TODO(oazizi): This function is only public for testing purposes. Make private?
  void ReadPerfBuffer(uint32_t table_num);

 private:
  // ReadPerfBuffer poll callback functions (must be static).
  // These are used by the static variables below, and have to be placed here.
  static void HandleDataEvent(void* cb_cookie, void* data, int data_size);
  static void HandleDataEventsLoss(void* cb_cookie, uint64_t lost);
  static void HandleControlEvent(void* cb_cookie, void* data, int data_size);
  static void HandleControlEventsLoss(void* cb_cookie, uint64_t lost);
  static void HandleHTTP2HeaderEvent(void* cb_cookie, void* data, int data_size);
  static void HandleHTTP2HeaderEventLoss(void* cb_cookie, uint64_t lost);
  static void HandleHTTP2Data(void* cb_cookie, void* data, int data_size);
  static void HandleHTTP2DataLoss(void* cb_cookie, uint64_t lost);

  static constexpr bpf_tools::KProbeSpec kProbeSpecsArray[] = {
      {"connect", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_connect"},
      {"connect", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_connect"},
      {"accept", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_accept"},
      {"accept", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_accept"},
      {"accept4", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_accept4"},
      {"accept4", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_accept4"},
      {"open", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_open"},
      {"creat", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_open"},
      {"openat", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_open"},
      {"write", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_write"},
      {"write", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_write"},
      {"writev", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_writev"},
      {"writev", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_writev"},
      {"send", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_send"},
      {"send", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_send"},
      {"sendto", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_sendto"},
      {"sendto", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_sendto"},
      {"sendmsg", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_sendmsg"},
      {"sendmsg", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_sendmsg"},
      {"read", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_read"},
      {"read", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_read"},
      {"readv", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_readv"},
      {"readv", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_readv"},
      {"recv", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_recv"},
      {"recv", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_recv"},
      {"recvfrom", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_recv"},
      {"recvfrom", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_recv"},
      {"recvmsg", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_recvmsg"},
      {"recvmsg", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_recvmsg"},
      {"close", bpf_probe_attach_type::BPF_PROBE_ENTRY, "syscall__probe_entry_close"},
      {"close", bpf_probe_attach_type::BPF_PROBE_RETURN, "syscall__probe_ret_close"},
  };
  static constexpr auto kProbeSpecs = ArrayView<bpf_tools::KProbeSpec>(kProbeSpecsArray);

  inline static constexpr bpf_tools::UProbeTmpl kUProbeTmplsArray[] = {
      {"google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders",
       elf_tools::SymbolMatchType::kSuffix, "probe_http2_client_operate_headers",
       bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"google.golang.org/grpc/internal/transport.(*loopyWriter).writeHeader",
       elf_tools::SymbolMatchType::kSuffix, "probe_loopy_writer_write_header",
       bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"golang.org/x/net/http2.(*Framer).WriteDataPadded", elf_tools::SymbolMatchType::kSuffix,
       "probe_framer_write_data", bpf_probe_attach_type::BPF_PROBE_ENTRY},
      {"golang.org/x/net/http2.(*Framer).checkFrameOrder", elf_tools::SymbolMatchType::kSuffix,
       "probe_framer_check_frame_order", bpf_probe_attach_type::BPF_PROBE_ENTRY},
  };
  static constexpr auto kUProbeTmpls = ArrayView<bpf_tools::UProbeTmpl>(kUProbeTmplsArray);

  // TODO(oazizi): Remove send and recv probes once we are confident that they don't trace anything.
  //               Note that send/recv are not in the syscall table
  //               (https://filippo.io/linux-syscall-table/), but are defined as SYSCALL_DEFINE4 in
  //               https://elixir.bootlin.com/linux/latest/source/net/socket.c.

  static constexpr bpf_tools::PerfBufferSpec kPerfBufferSpecsArray[] = {
      // For data events. The order must be consistent with output tables.
      {"socket_data_events", HandleDataEvent, HandleDataEventsLoss},
      // For non-data events. Must not mix with the above perf buffers for data events.
      {"socket_control_events", HandleControlEvent, HandleControlEventsLoss},
      {"go_grpc_header_events", HandleHTTP2HeaderEvent, HandleHTTP2HeaderEventLoss},
      {"go_grpc_data_events", HandleHTTP2Data, HandleHTTP2DataLoss},
  };
  static constexpr auto kPerfBufferSpecs =
      ArrayView<bpf_tools::PerfBufferSpec>(kPerfBufferSpecsArray);

  inline static http::HTTPHeaderFilter http_response_header_filter_;
  // TODO(yzhao): We will remove this once finalized the mechanism of lazy protobuf parse.
  inline static ::pl::grpc::ServiceDescriptorDatabase grpc_desc_db_{
      demos::hipster_shop::GetFileDescriptorSet()};

  explicit SocketTraceConnector(std::string_view source_name)
      : SourceConnector(
            source_name, kTables,
            std::chrono::milliseconds(FLAGS_stirling_socket_trace_sampling_period_millis),
            kDefaultPushPeriod),
        bpf_tools::BCCWrapper(kBCCScript) {
    // TODO(yzhao): Is there a better place/time to grab the flags?
    http_response_header_filter_ = http::ParseHTTPHeaderFilters(FLAGS_http_response_header_filters);
    proc_parser_ = std::make_unique<system::ProcParser>(system::Config::GetInstance());
    netlink_socket_prober_ = std::make_unique<system::NetlinkSocketProber>();

    protocol_transfer_specs_[kProtocolHTTP].enabled = FLAGS_stirling_enable_http_tracing;
    protocol_transfer_specs_[kProtocolHTTP2].enabled = FLAGS_stirling_enable_grpc_kprobe_tracing;
    protocol_transfer_specs_[kProtocolHTTP2Uprobe].enabled = true;
    protocol_transfer_specs_[kProtocolMySQL].enabled = FLAGS_stirling_enable_mysql_tracing;
  }

  // Events from BPF.
  // TODO(oazizi/yzhao): These all operate based on pass-by-value, which copies.
  //                     The Handle* functions should call make_unique() of new corresponding
  //                     objects, and these functions should take unique_ptrs.
  void AcceptDataEvent(std::unique_ptr<SocketDataEvent> event);
  void AcceptControlEvent(const socket_control_event_t& event);

  void AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event);
  void AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event);

  void UpdateActiveConnections();

  // Transfer of messages to the data table.
  void TransferStreams(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table);

  template <typename TEntryType>
  void TransferStream(ConnectorContext* ctx, ConnectionTracker* tracker, DataTable* data_table);

  template <typename TEntryType>
  static void AppendMessage(ConnectorContext* ctx, const ConnectionTracker& conn_tracker,
                            TEntryType record, DataTable* data_table);

  // HTTP-specific helper function.
  static bool SelectMessage(const http::Record& record);

  // TODO(oazizi/yzhao): Change to use std::unique_ptr.

  // Note that the inner map cannot be a vector, because there is no guaranteed order
  // in which events are read from perf buffers.
  // Inner map could be a priority_queue, but benchmarks showed better performance with a std::map.
  // Key is {PID, FD} for outer map (see GetStreamId()), and generation for inner map.
  std::unordered_map<uint64_t, std::map<uint64_t, ConnectionTracker> > connection_trackers_;

  struct TransferSpec {
    uint32_t table_num;
    std::function<void(SocketTraceConnector&, ConnectorContext*, ConnectionTracker*, DataTable*)>
        transfer_fn = nullptr;
    bool enabled = false;
  };

  // This map controls how each protocol is processed and transferred.
  // The table num identifies which data the collected data is transferred.
  // The transfer_fn defines which function is called to process the data for transfer.
  std::map<TrafficProtocol, TransferSpec> protocol_transfer_specs_ = {
      {kProtocolHTTP, {kHTTPTableNum, &SocketTraceConnector::TransferStream<http::Record>}},
      {kProtocolHTTP2, {kHTTPTableNum, &SocketTraceConnector::TransferStream<http2::Record>}},
      {kProtocolHTTP2Uprobe,
       {kHTTPTableNum, &SocketTraceConnector::TransferStream<http2::NewRecord>}},
      {kProtocolMySQL, {kMySQLTableNum, &SocketTraceConnector::TransferStream<mysql::Record>}},
      // Unknown protocols attached to HTTP table so that they run their cleanup functions,
      // but the use of nullptr transfer_fn means it won't actually transfer data to the HTTP table.
      {kProtocolUnknown, {kHTTPTableNum, nullptr}},
  };

  // If not a nullptr, writes the events received from perf buffers to this stream.
  std::unique_ptr<std::ofstream> perf_buffer_events_output_stream_;
  enum class OutputFormat {
    kTxt,
    kBin,
  };
  OutputFormat perf_buffer_events_output_format_ = OutputFormat::kTxt;

  std::unique_ptr<system::NetlinkSocketProber> netlink_socket_prober_;

  std::unique_ptr<std::map<int, system::SocketInfo> > socket_connections_;

  std::unique_ptr<system::ProcParser> proc_parser_;

  FRIEND_TEST(SocketTraceConnectorTest, AppendNonContiguousEvents);
  FRIEND_TEST(SocketTraceConnectorTest, NoEvents);
  FRIEND_TEST(SocketTraceConnectorTest, End2End);
  FRIEND_TEST(SocketTraceConnectorTest, UPIDCheck);
  FRIEND_TEST(SocketTraceConnectorTest, RequestResponseMatching);
  FRIEND_TEST(SocketTraceConnectorTest, MissingEventInStream);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupInOrder);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupOutOfOrder);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupMissingDataEvent);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupOldGenerations);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupInactiveDead);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupInactiveAlive);
  FRIEND_TEST(SocketTraceConnectorTest, ConnectionCleanupNoProtocol);
  FRIEND_TEST(SocketTraceConnectorTest, MySQLPrepareExecuteClose);
  FRIEND_TEST(SocketTraceConnectorTest, MySQLQuery);
  FRIEND_TEST(SocketTraceConnectorTest, MySQLMultipleCommands);
  FRIEND_TEST(SocketTraceConnectorTest, MySQLQueryWithLargeResultset);
  FRIEND_TEST(SocketTraceConnectorTest, MySQLMultiResultset);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2ClientTest);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2ServerTest);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2PartialStream);
};

}  // namespace stirling
}  // namespace pl

#endif
