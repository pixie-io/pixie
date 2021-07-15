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

#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"

#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <utility>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/match.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/common/json/json.h"
#include "src/common/system/socket_info.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/go_grpc_types.hpp"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/conn_stats.h"
#include "src/stirling/source_connectors/socket_tracer/proto/sock_event.pb.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/grpc.h"
#include "src/stirling/utils/proc_path_tools.h"

// 50 X less often than the normal sampling frequency. Based on the conn_stats_table.h's
// sampling period of 5 seconds, and other tables' 100 milliseconds.
DEFINE_uint32(
    stirling_conn_stats_sampling_ratio, 50,
    "Ratio of how frequently conn_stats_table is populated relative to the base sampling period");
// The default frequency logs every minute, since each iteration has a cycle period of 200ms.
DEFINE_uint32(
    stirling_socket_tracer_stats_logging_ratio,
    std::chrono::minutes(10) / px::stirling::SocketTraceConnector::kSamplingPeriod,
    "Ratio of how frequently conn_stats_table is populated relative to the base sampling period");

DEFINE_bool(stirling_enable_periodic_bpf_map_cleanup, true,
            "Disable periodic BPF map cleanup (for testing)");

DEFINE_int32(test_only_socket_trace_target_pid, kTraceAllTGIDs, "The process to trace.");
// TODO(yzhao): If we ever need to write all events from different perf buffers, then we need either
// write to different files for individual perf buffers, or create a protobuf message with an oneof
// field to include all supported message types.
DEFINE_string(perf_buffer_events_output_path, "",
              "If not empty, specifies the path & format to a file to which the socket tracer "
              "writes data events. If the filename ends with '.bin', the events are serialized in "
              "binary format; otherwise, text format.");

// PROTOCOL_LIST: Requires update on new protocols.
DEFINE_bool(stirling_enable_http_tracing, true,
            "If true, stirling will trace and process HTTP messages");
DEFINE_bool(stirling_enable_http2_tracing, true,
            "If true, stirling will trace and process gRPC RPCs.");
DEFINE_bool(stirling_enable_mysql_tracing, true,
            "If true, stirling will trace and process MySQL messages.");
DEFINE_bool(stirling_enable_pgsql_tracing, true,
            "If true, stirling will trace and process PostgreSQL messages.");
DEFINE_bool(stirling_enable_cass_tracing, true,
            "If true, stirling will trace and process Cassandra messages.");
DEFINE_bool(stirling_enable_dns_tracing, true,
            "If true, stirling will trace and process DNS messages.");
DEFINE_bool(stirling_enable_redis_tracing, true,
            "If true, stirling will trace and process Redis messages.");

DEFINE_bool(stirling_disable_self_tracing, true,
            "If true, stirling will not trace and process syscalls made by itself.");

DEFINE_uint32(messages_expiration_duration_secs, 10 * 60,
              "The duration for which a cached message to be erased.");
DEFINE_uint32(messages_size_limit_bytes, 1024 * 1024,
              "The limit of the size of the parsed messages, not the BPF events, "
              "for each direction, of each connection tracker. "
              "All cached messages are erased if this limit is breached.");

BPF_SRC_STRVIEW(socket_trace_bcc_script, socket_trace);

namespace px {
namespace stirling {

using ::px::stirling::protocols::kMaxBodyBytes;
using ::px::utils::ToJSONString;

SocketTraceConnector::SocketTraceConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables), conn_stats_(&conn_trackers_mgr_), uprobe_mgr_(this) {
  proc_parser_ = std::make_unique<system::ProcParser>(system::Config::GetInstance());
  InitProtocolTransferSpecs();
}

void SocketTraceConnector::InitProtocolTransferSpecs() {
#define TRANSFER_STREAM_PROTOCOL(protocol_name) \
  &SocketTraceConnector::TransferStream<protocols::protocol_name::ProtocolTraits>

  // PROTOCOL_LIST: Requires update on new protocols.

  // We popluate transfer_specs_by_protocol so that we guarantee the protocol_transfer_specs_
  // is stuffed in the *correct* order.
  // Also, this will fail fast (when we stuff the vector) if we forget a protocol.
  absl::flat_hash_map<TrafficProtocol, TransferSpec> transfer_specs_by_protocol = {
      {kProtocolHTTP, TransferSpec{FLAGS_stirling_enable_http_tracing,
                                   kHTTPTableNum,
                                   {kRoleClient, kRoleServer},
                                   TRANSFER_STREAM_PROTOCOL(http)}},
      {kProtocolHTTP2, TransferSpec{FLAGS_stirling_enable_http2_tracing,
                                    kHTTPTableNum,
                                    {kRoleClient, kRoleServer},
                                    TRANSFER_STREAM_PROTOCOL(http2)}},
      {kProtocolCQL, TransferSpec{FLAGS_stirling_enable_cass_tracing,
                                  kCQLTableNum,
                                  {kRoleClient, kRoleServer},
                                  TRANSFER_STREAM_PROTOCOL(cass)}},
      {kProtocolMySQL, TransferSpec{FLAGS_stirling_enable_mysql_tracing,
                                    kMySQLTableNum,
                                    {kRoleClient, kRoleServer},
                                    TRANSFER_STREAM_PROTOCOL(mysql)}},
      {kProtocolPGSQL, TransferSpec{FLAGS_stirling_enable_pgsql_tracing,
                                    kPGSQLTableNum,
                                    {kRoleClient, kRoleServer},
                                    TRANSFER_STREAM_PROTOCOL(pgsql)}},
      {kProtocolDNS, TransferSpec{FLAGS_stirling_enable_dns_tracing,
                                  kDNSTableNum,
                                  {kRoleClient, kRoleServer},
                                  TRANSFER_STREAM_PROTOCOL(dns)}},
      {kProtocolRedis, TransferSpec{FLAGS_stirling_enable_redis_tracing,
                                    kRedisTableNum,
                                    // Cannot infer endpoint role from Redis messages, so have to
                                    // allow such traffic transferred to user-space; and rely on
                                    // SocketInfo to infer the role.
                                    {kRoleUnknown, kRoleClient, kRoleServer},
                                    TRANSFER_STREAM_PROTOCOL(redis)}},
      // TODO(chengruizhe): Add Mongo table. nullptr should be replaced by the transfer_fn for
      // mongo in the future.
      {kProtocolMongo,
       TransferSpec{false, kHTTPTableNum, {kRoleUnknown, kRoleClient, kRoleServer}, nullptr}},
      // TODO(chengruizhe): Add Kafka table. nullptr should be replaced by the transfer_fn for
      // kafka in the future.
      {kProtocolKafka,
       TransferSpec{false, kHTTPTableNum, {kRoleUnknown, kRoleClient, kRoleServer}, nullptr}},
      {kProtocolUnknown, TransferSpec{false /*enabled*/,
                                      // Unknown protocols attached to HTTP table so that they run
                                      // their cleanup functions, but the use of nullptr transfer_fn
                                      // means it won't actually transfer data to the HTTP table.
                                      kHTTPTableNum,
                                      {kRoleUnknown, kRoleClient, kRoleServer},
                                      nullptr /*transfer_fn*/}}};
#undef TRANSFER_STREAM_PROTOCOL

  for (uint64_t i = 0; i < kNumProtocols; ++i) {
    // First, we double check that we have a transfer spec for the protocol in question.
    // Next, we stuff the vector of transfer specs,
    // by indexing into the transfer_specs_by_protocol map.
    DCHECK(transfer_specs_by_protocol.contains(TrafficProtocol(i))) << absl::Substitute(
        "Protocol $0 is not mapped in transfer_specs_by_protocol.", TrafficProtocol(i));
    protocol_transfer_specs_.push_back(transfer_specs_by_protocol[TrafficProtocol(i)]);
  }
}

Status SocketTraceConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);

  constexpr uint64_t kNanosPerSecond = 1000 * 1000 * 1000;
  if (kNanosPerSecond % sysconfig_.KernelTicksPerSecond() != 0) {
    return error::Internal(
        "SC_CLK_TCK aka USER_HZ must be 100, otherwise our BPF code may not generate proper "
        "timestamps in a way that matches how /proc/stat does it");
  }

  PL_RETURN_IF_ERROR(InitBPFProgram(socket_trace_bcc_script));
  PL_RETURN_IF_ERROR(AttachKProbes(kProbeSpecs));
  LOG(INFO) << absl::Substitute("Number of kprobes deployed = $0", kProbeSpecs.size());
  LOG(INFO) << "Probes successfully deployed.";

  PL_RETURN_IF_ERROR(OpenPerfBuffers(kPerfBufferSpecs, this));
  LOG(INFO) << absl::Substitute("Number of perf buffers opened = $0", kPerfBufferSpecs.size());

  // Set trace role to BPF probes.
  for (const auto& p : TrafficProtocolEnumValues()) {
    if (protocol_transfer_specs_[p].enabled) {
      uint64_t role_mask = 0;
      for (auto role : protocol_transfer_specs_[p].trace_roles) {
        role_mask |= role;
      }
      PL_RETURN_IF_ERROR(UpdateBPFProtocolTraceRole(p, role_mask));
    }
  }

  PL_RETURN_IF_ERROR(TestOnlySetTargetPID(FLAGS_test_only_socket_trace_target_pid));
  if (FLAGS_stirling_disable_self_tracing) {
    PL_RETURN_IF_ERROR(DisableSelfTracing());
  }
  if (!FLAGS_perf_buffer_events_output_path.empty()) {
    SetupOutput(FLAGS_perf_buffer_events_output_path);
  }

  StatusOr<std::unique_ptr<system::SocketInfoManager>> s =
      system::SocketInfoManager::Create(system::Config::GetInstance().proc_path(),
                                        system::kTCPEstablishedState | system::kTCPListeningState);
  if (!s.ok()) {
    LOG(WARNING) << absl::Substitute("Failed to set up socket prober manager. Message: $0",
                                     s.msg());
  } else {
    socket_info_mgr_ = s.ConsumeValueOrDie();
  }

  conn_info_map_mgr_ = std::make_shared<ConnInfoMapManager>(this);
  ConnTracker::SetConnInfoMapManager(conn_info_map_mgr_);

  uprobe_mgr_.Init(protocol_transfer_specs_[kProtocolHTTP2].enabled,
                   FLAGS_stirling_disable_self_tracing);

  return Status::OK();
}

void SocketTraceConnector::InitContextImpl(ConnectorContext* ctx) {
  std::thread thread = RunDeployUProbesThread(ctx->GetUPIDs());

  // On the first context, we want to make sure all uprobes deploy before returning.
  if (thread.joinable()) {
    thread.join();
  }
}

Status SocketTraceConnector::StopImpl() {
  if (perf_buffer_events_output_stream_ != nullptr) {
    perf_buffer_events_output_stream_->close();
  }

  // Wait for all threads to finish.
  while (uprobe_mgr_.ThreadsRunning()) {
  }

  // Must call Close() after attach_uprobes_thread_ has joined,
  // otherwise the two threads will cause concurrent accesses to BCC,
  // that will cause races and undefined behavior.
  Close();
  return Status::OK();
}

std::thread SocketTraceConnector::RunDeployUProbesThread(
    const absl::flat_hash_set<md::UPID>& pids) {
  // The check that state is not uninitialized is required for socket_trace_connector_test,
  // which would otherwise try to deploy uprobes (for which it does not have permissions).
  // Also, we check that there is no other previous thread still running.
  // If an old thread is still running, then we don't deploy this one,
  // we can let the next one cover the deployment of new uprobes.
  // TODO(oazizi): Consider changing the model to a independently running thread that wakes up
  //               periodically. If doing so, be careful of impact on tests, since the uprobe
  //               deployment will become asynchronous to TransferData(), and this may
  //               lead to non-determinism.
  if (state() != State::kUninitialized && !uprobe_mgr_.ThreadsRunning()) {
    return uprobe_mgr_.RunDeployUProbesThread(pids);
  }
  return {};
}

namespace {

std::string DumpContext(ConnectorContext* ctx) {
  std::vector<std::string> upids;
  for (const auto& upid : ctx->GetUPIDs()) {
    upids.push_back(upid.String());
  }
  return absl::Substitute("List of UPIDs (total=$0):\n$1", upids.size(),
                          absl::StrJoin(upids, "\n"));
}

template <typename TBPFTableKey, typename TBPFTableVal>
std::string BPFMapInfo(bpf_tools::BCCWrapper* bcc, std::string_view name) {
  auto map = bcc->GetHashTable<TBPFTableKey, TBPFTableVal>(name.data());
  size_t map_size = map.get_table_offline().size();
  if (1.0 * map_size / map.capacity() > 0.9) {
    LOG(WARNING) << absl::Substitute("BPF Table $0 is nearly at capacity [size=$0 capacity=$1]",
                                     map_size, map.capacity());
  }
  return absl::Substitute("\nBPFTable=$0 occupancy=$1 capacity=$2", name, map_size, map.capacity());
}

std::string BPFMapsInfo(bpf_tools::BCCWrapper* bcc) {
  std::string out;

  out += BPFMapInfo<uint32_t, struct go_common_symaddrs_t>(bcc, "go_common_symaddrs_map");
  out += BPFMapInfo<uint32_t, struct openssl_symaddrs_t>(bcc, "openssl_symaddrs_map");
  out += BPFMapInfo<uint64_t, struct data_args_t>(bcc, "active_ssl_read_args_map");
  out += BPFMapInfo<uint64_t, struct data_args_t>(bcc, "active_ssl_write_args_map");
  out += BPFMapInfo<uint32_t, struct go_tls_symaddrs_t>(bcc, "go_tls_symaddrs_map");
  out += BPFMapInfo<uint32_t, struct go_http2_symaddrs_t>(bcc, "http2_symaddrs_map");
  out += BPFMapInfo<void*, struct go_grpc_http2_header_event_t::header_attr_t>(
      bcc, "active_write_headers_frame_map");
  out += BPFMapInfo<uint64_t, struct conn_info_t>(bcc, "conn_info_map");
  out += BPFMapInfo<uint64_t, uint64_t>(bcc, "conn_disabled_map");
  out += BPFMapInfo<uint64_t, bool>(bcc, "open_file_map");
  out += BPFMapInfo<uint64_t, struct accept_args_t>(bcc, "active_accept_args_map");
  out += BPFMapInfo<uint64_t, struct connect_args_t>(bcc, "active_connect_args_map");
  out += BPFMapInfo<uint64_t, struct data_args_t>(bcc, "active_write_args_map");
  out += BPFMapInfo<uint64_t, struct data_args_t>(bcc, "active_read_args_map");
  out += BPFMapInfo<uint64_t, struct close_args_t>(bcc, "active_close_args_map");

  return out;
}

}  // namespace

void SocketTraceConnector::UpdateCommonState(ConnectorContext* ctx) {
  // Since events may be pushed into the perf buffer while reading it,
  // we establish a cutoff time before draining the perf buffer.
  // Note: We use AdjustedSteadyClockNowNS() instead of CurrentTimeNS()
  // to maintain consistency with how BPF generates timestamps on its events.
  perf_buffer_drain_time_ = AdjustedSteadyClockNowNS();

  // This drains all perf buffers, and causes Handle() callback functions to get called.
  // Note that it drains *all* perf buffers, not just those that are required for this table,
  // so raw data will be pushed to connection trackers more aggressively.
  // No data is lost, but this is a side-effect of sorts that affects timing of transfers.
  // It may be worth noting during debug.
  PollPerfBuffers();

  // Set-up current state for connection inference purposes.
  if (socket_info_mgr_ != nullptr) {
    socket_info_mgr_->Flush();
  }

  // Deploy uprobes on newly discovered PIDs.
  std::thread thread = RunDeployUProbesThread(ctx->GetUPIDs());
  // Let it run in the background.
  if (thread.joinable()) {
    thread.detach();
  }

  conn_trackers_mgr_.CleanupTrackers();

  // Periodically check for leaking conn_info_map entries.
  // TODO(oazizi): Track down and plug the leaks, then zap this function.
  constexpr auto kCleanupBPFMapLeaksPeriod = std::chrono::minutes(5);
  constexpr int kCleanupBPFMapLeaksSamplingRatio = kCleanupBPFMapLeaksPeriod / kSamplingPeriod;
  if (FLAGS_stirling_enable_periodic_bpf_map_cleanup &&
      sampling_freq_mgr_.count() % kCleanupBPFMapLeaksSamplingRatio == 0) {
    if (conn_info_map_mgr_ != nullptr) {
      conn_info_map_mgr_->CleanupBPFMapLeaks(&conn_trackers_mgr_);
    }
  }
}

void SocketTraceConnector::UpdateTrackerTraceLevel(ConnTracker* tracker) {
  if (pids_to_trace_.contains(tracker->conn_id().upid.pid)) {
    tracker->SetDebugTrace(2);
  }
  if (pids_to_trace_disable_.contains(tracker->conn_id().upid.pid)) {
    tracker->SetDebugTrace(0);
  }
}

void SocketTraceConnector::TransferDataImpl(ConnectorContext* ctx,
                                            const std::vector<DataTable*>& data_tables) {
  set_iteration_time(std::chrono::steady_clock::now());

  UpdateCommonState(ctx);

  DataTable* conn_stats_table = data_tables[kConnStatsTableNum];
  if (conn_stats_table != nullptr &&
      sampling_freq_mgr_.count() % FLAGS_stirling_conn_stats_sampling_ratio == 0) {
    TransferConnStats(ctx, conn_stats_table);
  }

  if (sampling_freq_mgr_.count() % FLAGS_stirling_socket_tracer_stats_logging_ratio == 0) {
    conn_trackers_mgr_.ComputeProtocolStats();
    LOG(INFO) << "ConnTracker statistics: " << conn_trackers_mgr_.StatsString();
    LOG(INFO) << "SocketTracer statistics: " << stats_.Print();
  }

  constexpr auto kDebugDumpPeriod = std::chrono::minutes(1);
  if (sampling_freq_mgr_.count() % (kDebugDumpPeriod / kSamplingPeriod)) {
    if (debug_level_ >= 1) {
      LOG(INFO) << "Context: " << DumpContext(ctx);
      LOG(INFO) << "BPF map info: " << BPFMapsInfo(static_cast<BCCWrapper*>(this));
    }
  }

  std::vector<CIDRBlock> cluster_cidrs = ctx->GetClusterCIDRs();

  for (size_t i = 0; i < data_tables.size(); ++i) {
    DataTable* data_table = data_tables[i];

    // Ensure records are within the time window, in order to ensure the order between record
    // batches. Exception: conn_stats table does not need cutoff time, because its timestamps
    // are assigned artificially.
    if (i != kConnStatsTableNum && data_table != nullptr) {
      data_table->SetConsumeRecordsCutoffTime(perf_buffer_drain_time_);
    }
  }

  for (const auto& conn_tracker : conn_trackers_mgr_.active_trackers()) {
    const auto& transfer_spec = protocol_transfer_specs_[conn_tracker->protocol()];
    DataTable* data_table = data_tables[transfer_spec.table_num];

    UpdateTrackerTraceLevel(conn_tracker);

    conn_tracker->IterationPreTick(iteration_time_, cluster_cidrs, proc_parser_.get(),
                                   socket_info_mgr_.get());
    if (transfer_spec.enabled && transfer_spec.transfer_fn && data_table != nullptr) {
      transfer_spec.transfer_fn(*this, ctx, conn_tracker, data_table);
    }
    conn_tracker->IterationPostTick();
  }

  // Once we've cleared all the debug trace levels for this pid, we can remove it from the list.
  pids_to_trace_disable_.clear();
}

template <typename TValueType>
Status UpdatePerCPUArrayValue(int idx, TValueType val, ebpf::BPFPercpuArrayTable<TValueType>* arr) {
  std::vector<TValueType> values(bpf_tools::BCCWrapper::kCPUCount, val);
  auto update_res = arr->update_value(idx, values);
  if (!update_res.ok()) {
    return error::Internal(absl::Substitute("Failed to set value on index: $0, error message: $1",
                                            idx, update_res.msg()));
  }
  return Status::OK();
}

Status SocketTraceConnector::UpdateBPFProtocolTraceRole(TrafficProtocol protocol,
                                                        uint64_t role_mask) {
  auto control_map_handle = GetPerCPUArrayTable<uint64_t>(kControlMapName);
  return UpdatePerCPUArrayValue(static_cast<int>(protocol), role_mask, &control_map_handle);
}

Status SocketTraceConnector::TestOnlySetTargetPID(int64_t pid) {
  auto control_map_handle = GetPerCPUArrayTable<int64_t>(kControlValuesArrayName);
  return UpdatePerCPUArrayValue(kTargetTGIDIndex, pid, &control_map_handle);
}

Status SocketTraceConnector::DisableSelfTracing() {
  auto control_map_handle = GetPerCPUArrayTable<int64_t>(kControlValuesArrayName);
  int64_t self_pid = getpid();
  return UpdatePerCPUArrayValue(kStirlingTGIDIndex, self_pid, &control_map_handle);
}

//-----------------------------------------------------------------------------
// Perf Buffer Polling and Callback functions.
//-----------------------------------------------------------------------------

void SocketTraceConnector::HandleDataEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto data_event_ptr = std::make_unique<SocketDataEvent>(data);
  connector->AcceptDataEvent(std::move(data_event_ptr));
}

void SocketTraceConnector::HandleDataEventLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossSocketDataEvent,
                                                                  lost);
}

void SocketTraceConnector::HandleControlEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  connector->AcceptControlEvent(*static_cast<const socket_control_event_t*>(data));
}

void SocketTraceConnector::HandleControlEventLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossSocketControlEvent,
                                                                  lost);
}

void SocketTraceConnector::HandleConnStatsEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  connector->AcceptConnStatsEvent(*static_cast<const conn_stats_event_t*>(data));
}

void SocketTraceConnector::HandleConnStatsEventLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossConnStatsEvent,
                                                                  lost);
}

void SocketTraceConnector::HandleMMapEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  connector->uprobe_mgr_.NotifyMMapEvent(*static_cast<upid_t*>(data));
}

void SocketTraceConnector::HandleMMapEventLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossMMapEvent, lost);
}

void SocketTraceConnector::HandleHTTP2HeaderEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);

  auto event = std::make_unique<HTTP2HeaderEvent>(data);

  VLOG(3) << absl::Substitute(
      "t=$0 pid=$1 type=$2 fd=$3 tsid=$4 stream_id=$5 end_stream=$6 name=$7 value=$8",
      event->attr.timestamp_ns, event->attr.conn_id.upid.pid,
      magic_enum::enum_name(event->attr.type), event->attr.conn_id.fd, event->attr.conn_id.tsid,
      event->attr.stream_id, event->attr.end_stream, event->name, event->value);
  connector->AcceptHTTP2Header(std::move(event));
}

void SocketTraceConnector::HandleHTTP2HeaderEventLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossGoGRPCHeaderEvent,
                                                                  lost);
}

void SocketTraceConnector::HandleHTTP2Data(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  // Directly access data through a go_grpc_data_event_t pointer results in mis-aligned access.
  // go_grpc_data_event_t is 8-bytes aligned, data is 4-bytes.
  auto event = std::make_unique<HTTP2DataEvent>(data);

  VLOG(3) << absl::Substitute(
      "t=$0 pid=$1 type=$2 fd=$3 tsid=$4 stream_id=$5 end_stream=$6 data=$7",
      event->attr.timestamp_ns, event->attr.conn_id.upid.pid,
      magic_enum::enum_name(event->attr.type), event->attr.conn_id.fd, event->attr.conn_id.tsid,
      event->attr.stream_id, event->attr.end_stream, event->payload);
  connector->AcceptHTTP2Data(std::move(event));
}

void SocketTraceConnector::HandleHTTP2DataLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossHTTP2Data, lost);
}

//-----------------------------------------------------------------------------
// Connection Tracker Events
//-----------------------------------------------------------------------------

ConnTracker& SocketTraceConnector::GetOrCreateConnTracker(struct conn_id_t conn_id) {
  ConnTracker& tracker = conn_trackers_mgr_.GetOrCreateConnTracker(conn_id);
  tracker.set_current_time(iteration_time_);
  UpdateTrackerTraceLevel(&tracker);
  return tracker;
}

void SocketTraceConnector::AcceptDataEvent(std::unique_ptr<SocketDataEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();

  if (perf_buffer_events_output_stream_ != nullptr) {
    WriteDataEvent(*event);
  }

  ConnTracker& tracker = GetOrCreateConnTracker(event->attr.conn_id);
  tracker.AddDataEvent(std::move(event));
}

void SocketTraceConnector::AcceptControlEvent(socket_control_event_t event) {
  // timestamp_ns is a common field of open and close fields.
  event.timestamp_ns += ClockRealTimeOffset();

  ConnTracker& tracker = GetOrCreateConnTracker(event.conn_id);
  tracker.AddControlEvent(event);
}

void SocketTraceConnector::AcceptConnStatsEvent(conn_stats_event_t event) {
  event.timestamp_ns += ClockRealTimeOffset();

  ConnTracker& tracker = conn_trackers_mgr_.GetOrCreateConnTracker(event.conn_id);
  tracker.AddConnStats(event);
}

void SocketTraceConnector::AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();

  ConnTracker& tracker = GetOrCreateConnTracker(event->attr.conn_id);
  tracker.AddHTTP2Header(std::move(event));
}

void SocketTraceConnector::AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();

  ConnTracker& tracker = GetOrCreateConnTracker(event->attr.conn_id);
  tracker.AddHTTP2Data(std::move(event));
}

//-----------------------------------------------------------------------------
// Append-Related Functions
//-----------------------------------------------------------------------------

namespace {

int64_t CalculateLatency(int64_t req_timestamp_ns, int64_t resp_timestamp_ns) {
  int64_t latency_ns = 0;
  if (req_timestamp_ns > 0 && resp_timestamp_ns > 0) {
    latency_ns = resp_timestamp_ns - req_timestamp_ns;
    LOG_IF(WARNING, latency_ns < 0)
        << absl::Substitute("Negative latency implies req resp mismatch [t_req=$0, t_resp=$1].",
                            req_timestamp_ns, resp_timestamp_ns);
  }
  return latency_ns;
}

}  // namespace

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::http::Record record, DataTable* data_table) {
  protocols::http::Message& req_message = record.req;
  protocols::http::Message& resp_message = record.resp;

  // Currently decompresses gzip content, but could handle other transformations too.
  // Note that we do this after filtering to avoid burning CPU cycles unnecessarily.
  protocols::http::PreProcessMessage(&resp_message);

  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  HTTPContentType content_type = HTTPContentType::kUnknown;
  if (protocols::http::IsJSONContent(resp_message)) {
    content_type = HTTPContentType::kJSON;
  }

  DataTable::RecordBuilder<&kHTTPTable> r(data_table, resp_message.timestamp_ns);
  r.Append<r.ColIndex("time_")>(resp_message.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  // Note that there is a string copy here,
  // But std::move is not allowed because we re-use conn object.
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("major_version")>(1);
  r.Append<r.ColIndex("minor_version")>(resp_message.minor_version);
  r.Append<r.ColIndex("content_type")>(static_cast<uint64_t>(content_type));
  r.Append<r.ColIndex("req_headers"), kMaxHTTPHeadersBytes>(ToJSONString(req_message.headers));
  r.Append<r.ColIndex("req_method")>(std::move(req_message.req_method));
  r.Append<r.ColIndex("req_path")>(std::move(req_message.req_path));
  r.Append<r.ColIndex("req_body_size")>(req_message.body.size());
  r.Append<r.ColIndex("req_body"), kMaxBodyBytes>(std::move(req_message.body));
  r.Append<r.ColIndex("resp_headers"), kMaxHTTPHeadersBytes>(ToJSONString(resp_message.headers));
  r.Append<r.ColIndex("resp_status")>(resp_message.resp_status);
  r.Append<r.ColIndex("resp_message")>(std::move(resp_message.resp_message));
  r.Append<r.ColIndex("resp_body_size")>(resp_message.body.size());
  r.Append<r.ColIndex("resp_body"), kMaxBodyBytes>(std::move(resp_message.body));
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(req_message.timestamp_ns, resp_message.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::http2::Record record, DataTable* data_table) {
  using ::px::grpc::MethodInputOutput;
  using ::px::stirling::grpc::ParsePB;

  protocols::http2::HalfStream* req_stream;
  protocols::http2::HalfStream* resp_stream;

  // Depending on whether the traced entity was the requestor or responder,
  // we need to flip the interpretation of the half-streams.
  if (conn_tracker.role() == kRoleClient) {
    req_stream = &record.send;
    resp_stream = &record.recv;
  } else {
    req_stream = &record.recv;
    resp_stream = &record.send;
  }

  // TODO(oazizi): Status should be in the trailers, not headers. But for now it is found in
  // headers. Fix when this changes.
  int64_t resp_status;
  ECHECK(absl::SimpleAtoi(resp_stream->headers().ValueByKey(":status", "-1"), &resp_status));

  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  std::string path = req_stream->headers().ValueByKey(protocols::http2::headers::kPath);

  HTTPContentType content_type = HTTPContentType::kUnknown;

  std::string req_data = req_stream->ConsumeData();
  std::string resp_data = resp_stream->ConsumeData();
  size_t req_data_size = req_stream->original_data_size();
  size_t resp_data_size = resp_stream->original_data_size();
  if (record.HasGRPCContentType()) {
    content_type = HTTPContentType::kGRPC;
    req_data = ParsePB(req_data, kMaxPBStringLen);
    if (req_stream->data_truncated()) {
      req_data.append(DataTable::kTruncatedMsg);
    }
    resp_data = ParsePB(resp_data, kMaxPBStringLen);
    if (resp_stream->data_truncated()) {
      resp_data.append(DataTable::kTruncatedMsg);
    }
  }

  DataTable::RecordBuilder<&kHTTPTable> r(data_table, resp_stream->timestamp_ns);
  r.Append<r.ColIndex("time_")>(resp_stream->timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("major_version")>(2);
  // HTTP2 does not define minor version.
  r.Append<r.ColIndex("minor_version")>(0);
  r.Append<r.ColIndex("req_headers"), kMaxHTTPHeadersBytes>(ToJSONString(req_stream->headers()));
  r.Append<r.ColIndex("content_type")>(static_cast<uint64_t>(content_type));
  r.Append<r.ColIndex("resp_headers"), kMaxHTTPHeadersBytes>(ToJSONString(resp_stream->headers()));
  r.Append<r.ColIndex("req_method")>(
      req_stream->headers().ValueByKey(protocols::http2::headers::kMethod));
  r.Append<r.ColIndex("req_path")>(req_stream->headers().ValueByKey(":path"));
  r.Append<r.ColIndex("resp_status")>(resp_status);
  // TODO(yzhao): Populate the following field from headers.
  r.Append<r.ColIndex("resp_message")>("OK");
  r.Append<r.ColIndex("req_body_size")>(req_data_size);
  // Do not apply truncation at this point, as the truncation was already done on serialized
  // protobuf message. This might result into longer text format data here, but the increase is
  // minimal.
  r.Append<r.ColIndex("req_body")>(std::move(req_data));
  r.Append<r.ColIndex("resp_body_size")>(resp_data_size);
  r.Append<r.ColIndex("resp_body")>(std::move(resp_data));
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(req_stream->timestamp_ns, resp_stream->timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::mysql::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kMySQLTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("req_cmd")>(static_cast<uint64_t>(entry.req.cmd));
  r.Append<r.ColIndex("req_body"), kMaxBodyBytes>(std::move(entry.req.msg));
  r.Append<r.ColIndex("resp_status")>(static_cast<uint64_t>(entry.resp.status));
  r.Append<r.ColIndex("resp_body"), kMaxBodyBytes>(std::move(entry.resp.msg));
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::cass::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kCQLTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("req_op")>(static_cast<uint64_t>(entry.req.op));
  r.Append<r.ColIndex("req_body"), kMaxBodyBytes>(std::move(entry.req.msg));
  r.Append<r.ColIndex("resp_op")>(static_cast<uint64_t>(entry.resp.op));
  r.Append<r.ColIndex("resp_body"), kMaxBodyBytes>(std::move(entry.resp.msg));
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::dns::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kDNSTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("req_header")>(entry.req.header);
  r.Append<r.ColIndex("req_body")>(entry.req.query);
  r.Append<r.ColIndex("resp_header")>(entry.resp.header);
  r.Append<r.ColIndex("resp_body")>(entry.resp.msg);
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::pgsql::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kPGSQLTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("req")>(std::move(entry.req.payload));
  r.Append<r.ColIndex("resp")>(std::move(entry.resp.payload));
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
  r.Append<r.ColIndex("req_cmd")>(ToString(entry.req.tag, /* is_req */ true));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

namespace {

EndpointRole SwapEndpointRole(EndpointRole role) {
  switch (role) {
    case kRoleClient:
      return kRoleServer;
    case kRoleServer:
      return kRoleClient;
    case kRoleUnknown:
      return kRoleUnknown;
  }
  // Needed for GCC build.
  return kRoleUnknown;
}

}  // namespace

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::redis::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  EndpointRole role = conn_tracker.role();
  if (entry.role_swapped) {
    role = SwapEndpointRole(role);
  }

  DataTable::RecordBuilder<&kRedisTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(role);
  r.Append<r.ColIndex("req_cmd")>(std::string(entry.req.command));
  r.Append<r.ColIndex("req_args")>(std::string(entry.req.payload));
  r.Append<r.ColIndex("resp")>(std::string(entry.resp.payload));
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::nats::Record record, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  EndpointRole role = conn_tracker.role();
  DataTable::RecordBuilder<&kNATSTable> r(data_table, record.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(record.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(role);
  r.Append<r.ColIndex("cmd")>(record.req.command);
  r.Append<r.ColIndex("body")>(record.req.options);
  r.Append<r.ColIndex("resp")>(record.resp.command);
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

void SocketTraceConnector::SetupOutput(const std::filesystem::path& path) {
  DCHECK(!path.empty());

  std::filesystem::path abs_path = std::filesystem::absolute(path);
  perf_buffer_events_output_stream_ = std::make_unique<std::ofstream>(abs_path);
  std::string format = "text";
  constexpr char kBinSuffix[] = ".bin";
  if (absl::EndsWith(FLAGS_perf_buffer_events_output_path, kBinSuffix)) {
    perf_buffer_events_output_format_ = OutputFormat::kBin;
    format = "binary";
  }
  LOG(INFO) << absl::Substitute("Writing output to: $0 in $1 format.", abs_path.string(), format);
}

namespace {
void SocketDataEventToPB(const SocketDataEvent& event, sockeventpb::SocketDataEvent* pb) {
  pb->mutable_attr()->set_timestamp_ns(event.attr.timestamp_ns);
  pb->mutable_attr()->mutable_conn_id()->set_pid(event.attr.conn_id.upid.pid);
  pb->mutable_attr()->mutable_conn_id()->set_start_time_ns(
      event.attr.conn_id.upid.start_time_ticks);
  pb->mutable_attr()->mutable_conn_id()->set_fd(event.attr.conn_id.fd);
  pb->mutable_attr()->mutable_conn_id()->set_generation(event.attr.conn_id.tsid);
  pb->mutable_attr()->set_protocol(event.attr.protocol);
  pb->mutable_attr()->set_role(event.attr.role);
  pb->mutable_attr()->set_direction(event.attr.direction);
  pb->mutable_attr()->set_pos(event.attr.pos);
  pb->mutable_attr()->set_msg_size(event.attr.msg_size);
  pb->set_msg(event.msg);
}
}  // namespace

void SocketTraceConnector::WriteDataEvent(const SocketDataEvent& event) {
  using ::google::protobuf::TextFormat;
  using ::google::protobuf::util::SerializeDelimitedToOstream;

  DCHECK(perf_buffer_events_output_stream_ != nullptr);

  sockeventpb::SocketDataEvent pb;
  SocketDataEventToPB(event, &pb);
  std::string text;
  switch (perf_buffer_events_output_format_) {
    case OutputFormat::kTxt:
      // TextFormat::Print() can print to a stream. That complicates things a bit, and we opt not
      // to do that as this is for debugging.
      TextFormat::PrintToString(pb, &text);
      // TextFormat already output a \n, so no need to do it here.
      *perf_buffer_events_output_stream_ << text << std::flush;
      break;
    case OutputFormat::kBin:
      SerializeDelimitedToOstream(pb, perf_buffer_events_output_stream_.get());
      *perf_buffer_events_output_stream_ << std::flush;
      break;
  }
}

//-----------------------------------------------------------------------------
// TransferData Helpers
//-----------------------------------------------------------------------------

template <typename TProtocolTraits>
void SocketTraceConnector::TransferStream(ConnectorContext* ctx, ConnTracker* tracker,
                                          DataTable* data_table) {
  VLOG(3) << absl::StrCat("Connection\n", DebugString<TProtocolTraits>(*tracker, ""));

  if (tracker->state() == ConnTracker::State::kTransferring) {
    // ProcessToRecords() parses raw events and produces messages in format that are expected by
    // table store. But those messages are not cached inside ConnTracker.
    auto records = tracker->ProcessToRecords<TProtocolTraits>();
    for (auto& record : records) {
      AppendMessage(ctx, *tracker, std::move(record), data_table);
    }

    auto expiry_timestamp =
        iteration_time_ - std::chrono::seconds(FLAGS_messages_expiration_duration_secs);
    tracker->Cleanup<TProtocolTraits>(FLAGS_messages_size_limit_bytes, expiry_timestamp);
  }
}

void SocketTraceConnector::TransferConnStats(ConnectorContext* ctx, DataTable* data_table) {
  namespace idx = ::px::stirling::conn_stats_idx;

  absl::flat_hash_set<md::UPID> upids = ctx->GetUPIDs();
  uint64_t time = CurrentTimeNS();

  auto& agg_stats = conn_stats_.UpdateStats();

  auto iter = agg_stats.begin();
  while (iter != agg_stats.end()) {
    const auto& key = iter->first;
    auto& stats = iter->second;

    DCHECK_GE(stats.conn_open, stats.conn_close);

    md::UPID upid(ctx->GetASID(), key.upid.pid, key.upid.start_time_ticks);
    bool active_upid = upids.contains(upid);

    bool activity = conn_stats_.Active(stats);

    VLOG(1) << absl::Substitute("upid=$0 active=$1 previously_active=$2", upid.String(),
                                active_upid, stats.reported);

    // Only export this record if there are actual changes.
    if ((active_upid || stats.reported) && activity) {
      DataTable::RecordBuilder<&kConnStatsTable> r(data_table, time);

      r.Append<idx::kTime>(time);
      r.Append<idx::kUPID>(upid.value());
      r.Append<idx::kRemoteAddr>(key.remote_addr);
      r.Append<idx::kRemotePort>(key.remote_port);
      r.Append<idx::kAddrFamily>(static_cast<int>(stats.addr_family));
      r.Append<idx::kProtocol>(stats.protocol);
      r.Append<idx::kRole>(stats.role);
      r.Append<idx::kConnOpen>(stats.conn_open);
      r.Append<idx::kConnClose>(stats.conn_close);
      r.Append<idx::kConnActive>(stats.conn_open - stats.conn_close);
      r.Append<idx::kBytesSent>(stats.bytes_sent);
      r.Append<idx::kBytesRecv>(stats.bytes_recv);
#ifndef NDEBUG
      r.Append<idx::kPxInfo>("");
#endif

      stats.reported = true;
    }

    // Check for pids that may have died.
    if (!active_upid && !activity) {
      const auto& sysconfig = system::Config::GetInstance();
      std::filesystem::path pid_file = sysconfig.proc_path() / std::to_string(key.upid.pid);
      if (!fs::Exists(pid_file).ok()) {
        agg_stats.erase(iter++);
        continue;
      }
    }

    // This is at the bottom, in order to avoid accidentally forgetting increment the iterator.
    ++iter;
  }
}

}  // namespace stirling
}  // namespace px
