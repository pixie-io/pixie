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
#include "src/common/metrics/metrics.h"
#include "src/common/system/kernel_version.h"
#include "src/common/system/socket_info.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/elf_reader.h"

#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/grpc_c.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/conn_stats.h"
#include "src/stirling/source_connectors/socket_tracer/conn_tracker.h"
#include "src/stirling/source_connectors/socket_tracer/conn_trackers_manager.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_bpf_tables.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_tables.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_manager.h"
#include "src/stirling/utils/linux_headers.h"
#include "src/stirling/utils/proc_path_tools.h"
#include "src/stirling/utils/proc_tracker.h"

DECLARE_uint32(stirling_conn_stats_sampling_ratio);
DECLARE_bool(stirling_enable_periodic_bpf_map_cleanup);
DECLARE_int32(test_only_socket_trace_target_pid);
DECLARE_string(socket_trace_data_events_output_path);
DECLARE_int32(stirling_enable_http_tracing);
DECLARE_int32(stirling_enable_http2_tracing);
DECLARE_int32(stirling_enable_mysql_tracing);
DECLARE_int32(stirling_enable_cass_tracing);
DECLARE_int32(stirling_enable_dns_tracing);
DECLARE_int32(stirling_enable_redis_tracing);
DECLARE_int32(stirling_enable_nats_tracing);
DECLARE_int32(stirling_enable_kafka_tracing);
DECLARE_int32(stirling_enable_mux_tracing);
DECLARE_int32(stirling_enable_amqp_tracing);
DECLARE_int32(stirling_enable_mongodb_tracing);
DECLARE_bool(stirling_disable_self_tracing);
DECLARE_string(stirling_role_to_trace);

DECLARE_uint32(stirling_socket_tracer_target_data_bw_percpu);
DECLARE_uint32(stirling_socket_tracer_target_control_bw_percpu);

DECLARE_uint32(messages_expiry_duration_secs);
DECLARE_uint32(messages_size_limit_bytes);
DECLARE_uint32(datastream_buffer_expiry_duration_secs);
DECLARE_uint32(datastream_buffer_retention_size);

DECLARE_uint64(max_body_bytes);

namespace px {
namespace stirling {

using px::stirling::bpf_tools::WrappedBCCArrayTable;

// Whether the protocol traced is turned on, off, or on but only for newer kernels.
enum TraceMode : int32_t {
  Off = 0,
  On = 1,
  OnForNewerKernel = 2,
};

class SocketTraceConnector : public BCCSourceConnector {
 public:
  static constexpr std::string_view kName = "socket_tracer";
  static constexpr auto kTables =
      MakeArray(kConnStatsTable, kHTTPTable, kMySQLTable, kCQLTable, kPGSQLTable, kDNSTable,
                kRedisTable, kNATSTable, kKafkaTable, kMuxTable, kAMQPTable, kMongoDBTable);

  static constexpr uint32_t kConnStatsTableNum = TableNum(kTables, kConnStatsTable);
  static constexpr uint32_t kHTTPTableNum = TableNum(kTables, kHTTPTable);
  static constexpr uint32_t kMySQLTableNum = TableNum(kTables, kMySQLTable);
  static constexpr uint32_t kCQLTableNum = TableNum(kTables, kCQLTable);
  static constexpr uint32_t kPGSQLTableNum = TableNum(kTables, kPGSQLTable);
  static constexpr uint32_t kDNSTableNum = TableNum(kTables, kDNSTable);
  static constexpr uint32_t kRedisTableNum = TableNum(kTables, kRedisTable);
  static constexpr uint32_t kNATSTableNum = TableNum(kTables, kNATSTable);
  static constexpr uint32_t kKafkaTableNum = TableNum(kTables, kKafkaTable);
  static constexpr uint32_t kMuxTableNum = TableNum(kTables, kMuxTable);
  static constexpr uint32_t kAMQPTableNum = TableNum(kTables, kAMQPTable);
  static constexpr uint32_t kMongoDBTableNum = TableNum(kTables, kMongoDBTable);

  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{200};
  // TODO(yzhao): This is not used right now. Eventually use this to control data push frequency.
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};

  static std::unique_ptr<SocketTraceConnector> Create(std::string_view name) {
    return std::unique_ptr<SocketTraceConnector>(new SocketTraceConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void InitContextImpl(ConnectorContext* ctx) override;
  void TransferDataImpl(ConnectorContext* ctx) override;

  void CheckTracerState();

  // Perform actions that are not specifically targeting a table.
  // For example, drain perf buffers, deploy new uprobes, and update socket info manager.
  // If these were performed on every TransferData(), they would occur too frequently,
  // because TransferData() gets called for every table in the connector.
  // That would then cause performance overheads.
  void UpdateCommonState(ConnectorContext* ctx);

  // Updates control map value for protocol, which specifies which role(s) to trace for the given
  // protocol's traffic.
  //
  // Role_mask a bit mask, and represents the endpoint_role_t roles that are allowed to transfer
  // data from inside BPF to user-space.
  Status UpdateBPFProtocolTraceRole(traffic_protocol_t protocol, uint64_t role_mask);

  // Instructs Stirling to log detailed debug information about the traced events from the PID
  // specified by --test_only_socket_trace_target_pid.
  Status TestOnlySetTargetPID();
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

  void test_only_set_now_fn(std::function<std::chrono::steady_clock::time_point()> now_fn) {
    now_fn_ = now_fn;
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
  static void HandleHTTP2Event(void* cb_cookie, void* data, int data_size);
  static void HandleHTTP2EventLoss(void* cb_cookie, uint64_t lost);
  static void HandleGrpcCEvent(void* cb_cookie, void* data, int data_size);
  static void HandleGrpcCDataLoss(void* cb_cookie, uint64_t lost);
  static void HandleGrpcCHeaderEvent(void* cb_cookie, void* data, int data_size);
  static void HandleGrpcCHeaderDataLoss(void* cb_cookie, uint64_t lost);
  static void HandleGrpcCCloseEvent(void* cb_cookie, void* data, int data_size);
  static void HandleGrpcCCloseDataLoss(void* cb_cookie, uint64_t lost);

  explicit SocketTraceConnector(std::string_view source_name);

  auto InitPerfBufferSpecs();
  Status InitBPF();
  void InitPerfBufferSpec();
  void InitProtocolTransferSpecs();

  ConnTracker& GetOrCreateConnTracker(struct conn_id_t conn_id);

  // Events from BPF.
  void AcceptDataEvent(std::unique_ptr<SocketDataEvent> event);
  void AcceptControlEvent(socket_control_event_t event);
  void AcceptConnStatsEvent(conn_stats_event_t event);
  void AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event);
  void AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event);
  void AcceptGrpcCEventData(std::unique_ptr<struct grpc_c_event_data_t> event);
  void AcceptGrpcCHeaderEventData(std::unique_ptr<struct grpc_c_header_event_data_t> event);
  void AcceptGrpcCEvent(struct conn_id_t connection_id, uint32_t stream_id, uint64_t timestamp,
                        bool outgoing, uint64_t position_in_stream,
                        std::vector<struct grpc_c_data_slice_t> slices);
  void AcceptGrpcCCloseEvent(std::unique_ptr<struct grpc_c_stream_closed_data> event);
  void InitiateHeaderEventDataGoStyle(
      struct conn_id_t conn_id, uint32_t stream_id, uint64_t timestamp, bool end_stream,
      bool outgoing,
      /* OUT */ struct go_grpc_http2_header_event_t* header_event_data_go_style);

  template <typename TProtocolTraits>
  void TransferStream(ConnectorContext* ctx, ConnTracker* tracker, DataTable* data_table);
  void TransferConnStats(ConnectorContext* ctx, DataTable* data_table);

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

  std::unique_ptr<WrappedBCCArrayTable<int>> openssl_trace_state_;
  std::unique_ptr<WrappedBCCMap<uint32_t, struct openssl_trace_state_debug_t>>
      openssl_trace_state_debug_;
  prometheus::Family<prometheus::Counter>& openssl_trace_mismatched_fds_counter_family_;
  prometheus::Family<prometheus::Counter>& openssl_trace_tls_source_counter_family_;

  absl::flat_hash_set<int> pids_to_trace_disable_;

  std::function<std::chrono::steady_clock::time_point()> now_fn_ = std::chrono::steady_clock::now;

  struct TransferSpec {
    // TODO(yzhao): Enabling protocol is essentially equivalent to subscribing to DataTable. They
    // could be unified.
    int32_t trace_mode = TraceMode::Off;
    uint32_t table_num = 0;
    std::vector<endpoint_role_t> trace_roles;
    std::function<void(SocketTraceConnector&, ConnectorContext*, ConnTracker*, DataTable*)>
        transfer_fn = nullptr;
    bool enabled = false;
  };

  void EnableIfNeeded(TransferSpec* spec) {
    constexpr uint32_t kLinux5p2VersionCode = 328192;
    spec->enabled = (spec->trace_mode == TraceMode::On) ||
                    (spec->trace_mode == TraceMode::OnForNewerKernel &&
                     system::GetCachedKernelVersion().code() >= kLinux5p2VersionCode);
  }

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
    kLossHTTP2Event,
    kLossGrpcCEvent,
    kLossGrpcCHeaderEvent,
    kLossGrpcCCloseEvent,

    kPollSocketDataEventCount,
    kPollSocketDataEventAttrSize,
    kPollSocketDataEventDataSize,
    kPollSocketDataEventSize,
  };

  utils::StatCounter<StatKey> stats_;

  friend class SocketTraceConnectorFriend;
  friend class SocketTraceBPFTest;
};

}  // namespace stirling
}  // namespace px
