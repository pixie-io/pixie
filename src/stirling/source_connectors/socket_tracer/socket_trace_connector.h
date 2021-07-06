/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>

#include "src/common/grpcutils/service_descriptor_database.h"
#include "src/common/system/socket_info.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/obj_tools/dwarf_tools.h"
#include "src/stirling/obj_tools/elf_tools.h"

#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/conn_stats.h"
#include "src/stirling/source_connectors/socket_tracer/conn_tracker.h"
#include "src/stirling/source_connectors/socket_tracer/conn_trackers_manager.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_bpf_tables.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_tables.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_manager.h"
#include "src/stirling/utils/proc_path_tools.h"
#include "src/stirling/utils/proc_tracker.h"

DECLARE_uint32(stirling_conn_stats_sampling_ratio);
DECLARE_bool(stirling_enable_periodic_bpf_map_cleanup);
DECLARE_string(perf_buffer_events_output_path);
DECLARE_bool(stirling_enable_http_tracing);
DECLARE_bool(stirling_enable_http2_tracing);
DECLARE_bool(stirling_enable_mysql_tracing);
DECLARE_bool(stirling_enable_cass_tracing);
DECLARE_bool(stirling_enable_dns_tracing);
DECLARE_bool(stirling_enable_redis_tracing);
DECLARE_bool(stirling_disable_self_tracing);
DECLARE_string(stirling_role_to_trace);

DECLARE_uint32(messages_expiration_duration_secs);
DECLARE_uint32(messages_size_limit_bytes);

namespace px {
namespace stirling {

class SocketTraceConnector : public SourceConnector, public bpf_tools::BCCWrapper {
 public:
  static constexpr std::string_view kName = "socket_tracer";
  static constexpr auto kTables = MakeArray(kConnStatsTable, kHTTPTable, kMySQLTable, kCQLTable,
                                            kPGSQLTable, kDNSTable, kRedisTable);

  static constexpr uint32_t kConnStatsTableNum = TableNum(kTables, kConnStatsTable);
  static constexpr uint32_t kHTTPTableNum = TableNum(kTables, kHTTPTable);
  static constexpr uint32_t kMySQLTableNum = TableNum(kTables, kMySQLTable);
  static constexpr uint32_t kCQLTableNum = TableNum(kTables, kCQLTable);
  static constexpr uint32_t kPGSQLTableNum = TableNum(kTables, kPGSQLTable);
  static constexpr uint32_t kDNSTableNum = TableNum(kTables, kDNSTable);
  static constexpr uint32_t kRedisTableNum = TableNum(kTables, kRedisTable);

  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{200};
  // TODO(yzhao): This is not used right now. Eventually use this to control data push frequency.
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SocketTraceConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void InitContextImpl(ConnectorContext* ctx) override;
  void TransferDataImpl(ConnectorContext* ctx, const std::vector<DataTable*>& data_tables) override;

  // Perform actions that are not specifically targeting a table.
  // For example, drain perf buffers, deploy new uprobes, and update socket info manager.
  // If these were performed on every TransferData(), they would occur too frequently,
  // because TransferData() gets called for every table in the connector.
  // That would then cause performance overheads.
  void UpdateCommonState(ConnectorContext* ctx);

  // Updates control map value for protocol, which specifies which role(s) to trace for the given
  // protocol's traffic.
  //
  // Role_mask a bit mask, and represents the EndpointRole roles that are allowed to transfer
  // data from inside BPF to user-space.
  Status UpdateBPFProtocolTraceRole(TrafficProtocol protocol, uint64_t role_mask);
  Status TestOnlySetTargetPID(int64_t pid);
  Status DisableSelfTracing();

  void DisablePIDTrace(int pid) override {
    SourceConnector::DisablePIDTrace(pid);
    pids_to_trace_disable_.insert(pid);
  }

  /**
   * Gets a pointer to the most recent ConnTracker for the given pid and fd.
   *
   * @return Pointer to the ConnTracker, or error::NotFound if it does not exist.
   */
  StatusOr<const ConnTracker*> GetConnTracker(uint32_t pid, int32_t fd) const {
    return conn_trackers_mgr_.GetConnTracker(pid, fd);
  }

 private:
  // ReadPerfBuffers poll callback functions (must be static).
  // These are used by the static variables below, and have to be placed here.
  static void HandleDataEvent(void* cb_cookie, void* data, int data_size);
  static void HandleDataEventLoss(void* cb_cookie, uint64_t lost);
  static void HandleControlEvent(void* cb_cookie, void* data, int data_size);
  static void HandleControlEventLoss(void* cb_cookie, uint64_t lost);
  static void HandleConnStatsEvent(void* cb_cookie, void* data, int data_size);
  static void HandleConnStatsEventLoss(void* cb_cookie, uint64_t lost);
  static void HandleMMapEvent(void* cb_cookie, void* data, int data_size);
  static void HandleMMapEventLoss(void* cb_cookie, uint64_t lost);
  static void HandleHTTP2HeaderEvent(void* cb_cookie, void* data, int data_size);
  static void HandleHTTP2HeaderEventLoss(void* cb_cookie, uint64_t lost);
  static void HandleHTTP2Data(void* cb_cookie, void* data, int data_size);
  static void HandleHTTP2DataLoss(void* cb_cookie, uint64_t lost);

  static constexpr auto kProbeSpecs = MakeArray<bpf_tools::KProbeSpec>({
      {"connect", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_connect"},
      {"connect", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_connect"},
      {"accept", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_accept"},
      {"accept", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_accept"},
      {"accept4", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_accept4"},
      {"accept4", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_accept4"},
      {"open", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_open"},
      {"creat", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_open"},
      {"openat", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_open"},
      {"write", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_write"},
      {"write", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_write"},
      {"writev", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_writev"},
      {"writev", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_writev"},
      {"send", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_send"},
      {"send", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_send"},
      {"sendto", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_sendto"},
      {"sendto", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_sendto"},
      {"sendmsg", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_sendmsg"},
      {"sendmsg", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_sendmsg"},
      {"sendmmsg", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_sendmmsg"},
      {"sendmmsg", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_sendmmsg"},
      {"read", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_read"},
      {"read", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_read"},
      {"readv", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_readv"},
      {"readv", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_readv"},
      {"recv", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_recv"},
      {"recv", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_recv"},
      {"recvfrom", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_recvfrom"},
      {"recvfrom", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_recvfrom"},
      {"recvmsg", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_recvmsg"},
      {"recvmsg", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_recvmsg"},
      {"recvmmsg", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_recvmmsg"},
      {"recvmmsg", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_recvmmsg"},
      {"close", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_close"},
      {"close", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_close"},
      {"mmap", bpf_tools::BPFProbeAttachType::kEntry, "syscall__probe_entry_mmap"},
      {"sock_alloc", bpf_tools::BPFProbeAttachType::kReturn, "probe_ret_sock_alloc",
       /*is_syscall*/ false},
  });

  // TODO(oazizi): Remove send and recv probes once we are confident that they don't trace anything.
  //               Note that send/recv are not in the syscall table
  //               (https://filippo.io/linux-syscall-table/), but are defined as SYSCALL_DEFINE4 in
  //               https://elixir.bootlin.com/linux/latest/source/net/socket.c.

  // Assume a moderate network bandwidth peak of 100MiB/s across socket connections for data.
  inline static constexpr int64_t kTargetDataBytesPerSec = 100 * 1024 * 1024;
  inline static constexpr int64_t kTargetDataBufferSize =
      kTargetDataBytesPerSec * kSamplingPeriod.count() / 1000;

  // Assume a 5MiB/s across socket connections for control events.
  inline static constexpr int64_t kTargetControlBytesPerSec = 5 * 1024 * 1024;
  inline static constexpr int64_t kTargetControlBufferSize =
      kTargetControlBytesPerSec * kSamplingPeriod.count() / 1000;

  inline static const auto kPerfBufferSpecs = MakeArray<bpf_tools::PerfBufferSpec>({
      // For data events. The order must be consistent with output tables.
      {"socket_data_events", HandleDataEvent, HandleDataEventLoss, kTargetDataBufferSize},
      // For non-data events. Must not mix with the above perf buffers for data events.
      {"socket_control_events", HandleControlEvent, HandleControlEventLoss,
       kTargetControlBufferSize},
      {"conn_stats_events", HandleConnStatsEvent, HandleConnStatsEventLoss,
       kTargetControlBufferSize},
      {"mmap_events", HandleMMapEvent, HandleMMapEventLoss, kTargetControlBufferSize},
      {"go_grpc_header_events", HandleHTTP2HeaderEvent, HandleHTTP2HeaderEventLoss,
       kTargetDataBufferSize / 10},
      {"go_grpc_data_events", HandleHTTP2Data, HandleHTTP2DataLoss, kTargetDataBufferSize},
  });

  // Most HTTP servers support 8K headers, so we truncate after that.
  // https://stackoverflow.com/questions/686217/maximum-on-http-header-values
  inline static constexpr size_t kMaxHTTPHeadersBytes = 8192;

  // Protobuf printer will limit strings to this length.
  inline static constexpr size_t kMaxPBStringLen = 64;

  explicit SocketTraceConnector(std::string_view source_name);

  // Use this version of the clock, instead of CurrentTimeNS(), when generating a timestamp
  // for comparison against BPF event timestamps. This is to make sure the clocks are generated
  // in the exact same way.
  uint64_t AdjustedSteadyClockNowNS() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() +
           ClockRealTimeOffset();
  }

  // Initialize protocol_transfer_specs_.
  void InitProtocolTransferSpecs();

  ConnTracker& GetOrCreateConnTracker(struct conn_id_t conn_id);

  // Events from BPF.
  void AcceptDataEvent(std::unique_ptr<SocketDataEvent> event);
  void AcceptControlEvent(socket_control_event_t event);
  void AcceptConnStatsEvent(conn_stats_event_t event);
  void AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event);
  void AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event);

  // Transfer of messages to the data table.
  void TransferStreams(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table);
  void TransferConnStats(ConnectorContext* ctx, DataTable* data_table);

  template <typename TProtocolTraits>
  void TransferStream(ConnectorContext* ctx, ConnTracker* tracker, DataTable* data_table);

  void set_iteration_time(std::chrono::time_point<std::chrono::steady_clock> time) {
    DCHECK(time >= iteration_time_);
    iteration_time_ = time;
  }
  std::chrono::time_point<std::chrono::steady_clock> iteration_time() { return iteration_time_; }

  void UpdateTrackerTraceLevel(ConnTracker* tracker);

  template <typename TRecordType>
  static void AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                            TRecordType record, DataTable* data_table);

  std::thread RunDeployUProbesThread(const absl::flat_hash_set<md::UPID>& pids);

  // Setups output file stream object writing to the input file path.
  void SetupOutput(const std::filesystem::path& file);

  // Writes data event to the specified output file.
  void WriteDataEvent(const SocketDataEvent& event);

  ConnTrackersManager conn_trackers_mgr_;

  ConnStats conn_stats_;

  absl::flat_hash_set<int> pids_to_trace_disable_;

  struct TransferSpec {
    // TODO(yzhao): Enabling protocol is essentially equivalent to subscribing to DataTable. They
    // could be unified.
    bool enabled = false;
    uint32_t table_num = 0;
    std::vector<EndpointRole> trace_roles;
    std::function<void(SocketTraceConnector&, ConnectorContext*, ConnTracker*, DataTable*)>
        transfer_fn = nullptr;
  };

  // This map controls how each protocol is processed and transferred.
  // The table num identifies which data the collected data is transferred.
  // The transfer_fn defines which function is called to process the data for transfer.
  std::vector<TransferSpec> protocol_transfer_specs_;

  // The time at which TransferDataImpl() begin. Used as a universal timestamp for the iteration,
  // to avoid too many calls to std::chrono::steady_clock::now().
  std::chrono::time_point<std::chrono::steady_clock> iteration_time_;

  // Keep track of when the last perf buffer drain event was triggered.
  // Perf buffer draining is not atomic nor synchronous, so we want the time before draining.
  // The time is used by DataTable to produce records in sorted order across iterations.
  //   Example: data_table->SetConsumeRecordsCutoffTime(perf_buffer_drain_time_);
  uint64_t perf_buffer_drain_time_ = 0;

  // If not a nullptr, writes the events received from perf buffers to this stream.
  std::unique_ptr<std::ofstream> perf_buffer_events_output_stream_;
  enum class OutputFormat {
    kTxt,
    kBin,
  };
  OutputFormat perf_buffer_events_output_format_ = OutputFormat::kTxt;

  // Portal to query for connections, by pid and inode.
  std::unique_ptr<system::SocketInfoManager> socket_info_mgr_;

  std::unique_ptr<system::ProcParser> proc_parser_;

  std::shared_ptr<ConnInfoMapManager> conn_info_map_mgr_;

  UProbeManager uprobe_mgr_;

  enum class StatKey {
    kLossSocketDataEvent,
    kLossSocketControlEvent,
    kLossConnStatsEvent,
    kLossMMapEvent,
    kLossGoGRPCHeaderEvent,
    kLossHTTP2Data,
  };

  utils::StatCounter<StatKey> stats_;

  FRIEND_TEST(SocketTraceConnectorTest, AppendNonContiguousEvents);
  FRIEND_TEST(SocketTraceConnectorTest, NoEvents);
  FRIEND_TEST(SocketTraceConnectorTest, SortedByResponseTime);
  FRIEND_TEST(SocketTraceConnectorTest, HTTPBasic);
  FRIEND_TEST(SocketTraceConnectorTest, HTTPContentType);
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
  FRIEND_TEST(SocketTraceConnectorTest, CQLQuery);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2ClientTest);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2ServerTest);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2PartialStream);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2ResponseOnly);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2SpanAcrossTransferData);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2SequentialStreams);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2ParallelStreams);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2StreamSandwich);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2StreamIDRace);
  FRIEND_TEST(SocketTraceConnectorTest, HTTP2OldStream);
};

}  // namespace stirling
}  // namespace px
