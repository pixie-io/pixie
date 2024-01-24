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

#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <utility>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/match.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <magic_enum.hpp>
#include "src/common/system/kernel_version.h"

#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/common/json/json.h"
#include "src/common/system/proc_pid_path.h"
#include "src/common/system/socket_info.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/bpf_tools/utils.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/go_grpc_types.hpp"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/conn_stats.h"
#include "src/stirling/source_connectors/socket_tracer/proto/sock_event.pb.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/grpc.h"
#include "src/stirling/utils/linux_headers.h"
#include "src/stirling/utils/proc_path_tools.h"

// 50 X less often than the normal sampling frequency. Based on the conn_stats_table.h's
// sampling period of 5 seconds, and other tables' 100 milliseconds.
DEFINE_uint32(
    stirling_conn_stats_sampling_ratio, 50,
    "Ratio of how frequently conn_stats_table is populated relative to the base sampling period.");

DEFINE_uint32(stirling_socket_tracer_stats_logging_ratio,
              std::chrono::minutes(10) / px::stirling::SocketTraceConnector::kSamplingPeriod,
              "Ratio of how frequently summary logging information is displayed.");

DEFINE_bool(stirling_enable_periodic_bpf_map_cleanup, true,
            "Disable periodic BPF map cleanup (for testing)");

DEFINE_int32(test_only_socket_trace_target_pid, kTraceAllTGIDs,
             "The PID of a process to trace. This forces BPF to export events by ignoring event "
             "filtering. The purpose is to observe the underlying raw events for debugging.");
// TODO(yzhao): If we ever need to write all events from different perf buffers, then we need either
// write to different files for individual perf buffers, or create a protobuf message with an oneof
// field to include all supported message types.
DEFINE_string(socket_trace_data_events_output_path, "",
              "If not empty, specifies the path & format to a file to which the socket tracer "
              "writes data events. If the filename ends with '.bin', the events are serialized in "
              "binary format; otherwise, text format.");

// PROTOCOL_LIST: Requires update on new protocols.
//
// Due to BPF instruction limits (< 4096 instructions) on kernels older than
// 5.2, we can't simultaneously enable all protocols. Thus, some protocols
// are only enabled on newer kernels.
DEFINE_int32(stirling_enable_http_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_HTTP_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process HTTP messages");
DEFINE_int32(stirling_enable_http2_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_HTTP2_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process gRPC RPCs.");
DEFINE_int32(stirling_enable_mysql_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_MYSQL_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process MySQL messages.");
DEFINE_int32(stirling_enable_pgsql_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_PGSQL_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process PostgreSQL messages.");
DEFINE_int32(stirling_enable_cass_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_CASS_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process Cassandra messages.");
DEFINE_int32(stirling_enable_dns_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_DNS_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process DNS messages.");
DEFINE_int32(stirling_enable_redis_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_REDIS_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process Redis messages.");
DEFINE_int32(stirling_enable_nats_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_NATS_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process NATS messages.");
DEFINE_int32(stirling_enable_kafka_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_KAFKA_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process Kafka messages.");
DEFINE_int32(stirling_enable_mux_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_MUX_TRACING",
                                  px::stirling::TraceMode::OnForNewerKernel),
             "If true, stirling will trace and process Mux messages.");
DEFINE_int32(stirling_enable_amqp_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_AMQP_TRACING", px::stirling::TraceMode::On),
             "If true, stirling will trace and process AMQP messages.");
DEFINE_int32(stirling_enable_mongodb_tracing,
             gflags::Int32FromEnv("PX_STIRLING_ENABLE_MONGODB_TRACING",
                                  px::stirling::TraceMode::OnForNewerKernel),
             "If true, stirling will trace and process MongoDB messages");
DEFINE_bool(stirling_disable_golang_tls_tracing,
            gflags::BoolFromEnv("PX_STIRLING_DISABLE_GOLANG_TLS_TRACING", false),
            "If true, stirling will not trace TLS traffic for Go applications. This implies "
            "stirling_enable_http2_tracing=false.");

DEFINE_bool(stirling_disable_self_tracing, true,
            "If true, stirling will not trace and process syscalls made by itself.");

// Assume a moderate default network bandwidth peak of 100MiB/s across socket connections for data.
DEFINE_uint32(stirling_socket_tracer_target_data_bw_percpu, 100 * 1024 * 1024,
              "Target bytes/sec of data per CPU");
// Assume a default of 5MiB/s across socket connections for control events.
DEFINE_uint32(stirling_socket_tracer_target_control_bw_percpu, 5 * 1024 * 1024,
              "Target bytes/sec of control events per CPU");

DEFINE_double(
    stirling_socket_tracer_percpu_bw_scaling_factor, 8,
    "Per CPU scaling factor to apply to perf buffers, with the formula "
    "(1+x)/(ncpus+x), where x is the value of this flag. "
    "A value of infinity means each CPU's perf buffer is sized to handle the target BW. "
    "A value of 0 means each CPU's perf buffer is sized to handle a 1/ncpu * target BW. "
    "Values in between trade-off between these two extremes in a way that prunes back "
    "memory usage more aggressively for high core counts. "
    "8 is the default to curb memory use in perf buffers for CPUs with high core counts.");

DEFINE_uint64(stirling_socket_tracer_max_total_data_bw,
              static_cast<uint64_t>(10 * 1024 * 1024) * 1024 / 8 /*10Gibit/s*/,
              "Maximum total bytes/sec of data.");
DEFINE_uint64(stirling_socket_tracer_max_total_control_bw, 512 * 1024 * 1024 / 8 /*512Mibit/s*/,
              "Maximum total bytes/sec of control events.");
DEFINE_double(stirling_socket_tracer_max_total_bw_overprovision_factor, 1,
              "Factor to overprovision maximum total bandwidth, to account for the fact that "
              "traffic won't be exactly evenly distributed over all cpus.");

DEFINE_uint32(messages_expiry_duration_secs, 1 * 60,
              "The duration after which a parsed message is erased.");
DEFINE_uint32(messages_size_limit_bytes, 1024 * 1024,
              "The limit of the size of the parsed messages, not the BPF events, "
              "for each direction, of each connection tracker. "
              "All stored messages are erased if this limit is breached.");

DEFINE_uint32(
    datastream_buffer_expiry_duration_secs, 10,
    "The duration after which a buffer will be cleared if there is no progress in the parser.");
DEFINE_uint32(datastream_buffer_retention_size,
              gflags::Uint32FromEnv("PL_DATASTREAM_BUFFER_SIZE", 1024 * 1024),
              "The maximum size of a data stream buffer retained between cycles.");

DEFINE_uint64(max_body_bytes, gflags::Uint64FromEnv("PL_STIRLING_MAX_BODY_BYTES", 512),
              "The maximum number of bytes in the body of protocols like HTTP");

DEFINE_bool(
    stirling_trace_static_tls_binaries, gflags::BoolFromEnv("PX_TRACE_STATIC_TLS_BINARIES", true),
    "If true, stirling will tls trace binaries statically linked with OpenSSL or BoringSSL");

DEFINE_bool(
    stirling_debug_tls_sources, gflags::BoolFromEnv("PX_DEBUG_TLS_SOURCES", false),
    "If true, stirling will add additional prometheus metrics regarding the traced tls sources");

DEFINE_uint32(stirling_bpf_loop_limit, 42,
              "The maximum number of iovecs to capture for syscalls. "
              "Set conservatively for older kernels by default to keep the instruction count below "
              "BPF's limit for version 4 kernels (4096 per probe).");

DEFINE_uint32(stirling_bpf_chunk_limit, 4,
              "The maximum number of chunks a perf_submit can support. "
              "This applies to messages that are over MAX_MSG_SIZE.");

OBJ_STRVIEW(socket_trace_bcc_script, socket_trace);

namespace px {
namespace stirling {

using px::stirling::bpf_tools::WrappedBCCPerCPUArrayTable;
using px::system::ProcPath;
using px::system::ProcPidPath;
using px::utils::ToJSONString;

// Most HTTP servers support 8K headers, so we truncate after that.
// https://stackoverflow.com/questions/686217/maximum-on-http-header-values
constexpr size_t kMaxHTTPHeadersBytes = 8192;

// Protobuf printer will limit strings to this length.
constexpr size_t kMaxPBStringLen = 64;

constexpr char openssl_mismatched_fds_metric[] = "openssl_trace_mismatched_fds";
constexpr char openssl_mismatched_fds_help[] =
    "Count of the times a syscall's fd was mismatched when detecting fds from an active user space "
    "call";
constexpr char openssl_tls_source_metric[] = "openssl_tls_source_debug";
constexpr char openssl_tls_source_help[] =
    "Records the number of times a protocol was traced along with additional debugging information";

SocketTraceConnector::SocketTraceConnector(std::string_view source_name)
    : BCCSourceConnector(source_name, kTables),
      conn_stats_(&conn_trackers_mgr_),
      openssl_trace_mismatched_fds_counter_family_(
          BuildCounterFamily(openssl_mismatched_fds_metric, openssl_mismatched_fds_help)),
      openssl_trace_tls_source_counter_family_(
          BuildCounterFamily(openssl_tls_source_metric, openssl_tls_source_help)),
      uprobe_mgr_(&this->BCC()) {
  proc_parser_ = std::make_unique<system::ProcParser>();
  InitProtocolTransferSpecs();
}

void SocketTraceConnector::InitProtocolTransferSpecs() {
#define TRANSFER_STREAM_PROTOCOL(protocol_name) \
  &SocketTraceConnector::TransferStream<protocols::protocol_name::ProtocolTraits>

  // PROTOCOL_LIST: Requires update on new protocols.

  // We popluate transfer_specs_by_protocol so that we guarantee the protocol_transfer_specs_
  // is stuffed in the *correct* order.
  // Also, this will fail fast (when we stuff the vector) if we forget a protocol.
  absl::flat_hash_map<traffic_protocol_t, TransferSpec> transfer_specs_by_protocol = {
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
      {kProtocolNATS, TransferSpec{FLAGS_stirling_enable_nats_tracing,
                                   kNATSTableNum,
                                   {kRoleClient, kRoleServer},
                                   TRANSFER_STREAM_PROTOCOL(nats)}},
      {kProtocolKafka, TransferSpec{FLAGS_stirling_enable_kafka_tracing,
                                    kKafkaTableNum,
                                    {kRoleClient, kRoleServer},
                                    TRANSFER_STREAM_PROTOCOL(kafka)}},
      {kProtocolMux, TransferSpec{FLAGS_stirling_enable_mux_tracing,
                                  kMuxTableNum,
                                  {kRoleClient, kRoleServer},
                                  TRANSFER_STREAM_PROTOCOL(mux)}},
      {kProtocolMongo, TransferSpec{FLAGS_stirling_enable_mongodb_tracing,
                                    kMongoDBTableNum,
                                    {kRoleClient, kRoleServer},
                                    TRANSFER_STREAM_PROTOCOL(mongodb)}},
      {kProtocolAMQP, TransferSpec{FLAGS_stirling_enable_amqp_tracing,
                                   kAMQPTableNum,
                                   {kRoleClient, kRoleServer},
                                   TRANSFER_STREAM_PROTOCOL(amqp)}},
      {kProtocolUnknown, TransferSpec{/* trace_mode */ px::stirling::TraceMode::Off,
                                      /* table_num */ static_cast<uint32_t>(-1),
                                      /* trace_roles */ {},
                                      /* transfer_fn */ nullptr}}};
#undef TRANSFER_STREAM_PROTOCOL

  for (uint64_t i = 0; i < kNumProtocols; ++i) {
    // First, we double check that we have a transfer spec for the protocol in question.
    // Next, we stuff the vector of transfer specs,
    // by indexing into the transfer_specs_by_protocol map.
    DCHECK(transfer_specs_by_protocol.contains(traffic_protocol_t(i))) << absl::Substitute(
        "Protocol $0 is not mapped in transfer_specs_by_protocol.", traffic_protocol_t(i));

    // Enables protocol tracing based on trace mode flags and kernel version.
    EnableIfNeeded(&transfer_specs_by_protocol[traffic_protocol_t(i)]);
    protocol_transfer_specs_.push_back(transfer_specs_by_protocol[traffic_protocol_t(i)]);
  }
}

using ProbeType = bpf_tools::BPFProbeAttachType;
const auto kProbeSpecs = MakeArray<bpf_tools::KProbeSpec>(
    {{"connect", ProbeType::kEntry, "syscall__probe_entry_connect"},
     {"connect", ProbeType::kReturn, "syscall__probe_ret_connect"},
     {"accept", ProbeType::kEntry, "syscall__probe_entry_accept"},
     {"accept", ProbeType::kReturn, "syscall__probe_ret_accept"},
     {"accept4", ProbeType::kEntry, "syscall__probe_entry_accept4"},
     {"accept4", ProbeType::kReturn, "syscall__probe_ret_accept4"},
     {"write", ProbeType::kEntry, "syscall__probe_entry_write"},
     {"write", ProbeType::kReturn, "syscall__probe_ret_write"},
     {"writev", ProbeType::kEntry, "syscall__probe_entry_writev"},
     {"writev", ProbeType::kReturn, "syscall__probe_ret_writev"},
     {"send", ProbeType::kEntry, "syscall__probe_entry_send"},
     {"send", ProbeType::kReturn, "syscall__probe_ret_send"},
     {"sendto", ProbeType::kEntry, "syscall__probe_entry_sendto"},
     {"sendto", ProbeType::kReturn, "syscall__probe_ret_sendto"},
     {"sendmsg", ProbeType::kEntry, "syscall__probe_entry_sendmsg"},
     {"sendmsg", ProbeType::kReturn, "syscall__probe_ret_sendmsg"},
     {"sendmmsg", ProbeType::kEntry, "syscall__probe_entry_sendmmsg"},
     {"sendmmsg", ProbeType::kReturn, "syscall__probe_ret_sendmmsg"},
     {"sendfile", ProbeType::kEntry, "syscall__probe_entry_sendfile"},
     {"sendfile", ProbeType::kReturn, "syscall__probe_ret_sendfile"},
     {"sendfile64", ProbeType::kEntry, "syscall__probe_entry_sendfile"},
     {"sendfile64", ProbeType::kReturn, "syscall__probe_ret_sendfile"},
     {"read", ProbeType::kEntry, "syscall__probe_entry_read"},
     {"read", ProbeType::kReturn, "syscall__probe_ret_read"},
     {"readv", ProbeType::kEntry, "syscall__probe_entry_readv"},
     {"readv", ProbeType::kReturn, "syscall__probe_ret_readv"},
     {"recv", ProbeType::kEntry, "syscall__probe_entry_recv"},
     {"recv", ProbeType::kReturn, "syscall__probe_ret_recv"},
     {"recvfrom", ProbeType::kEntry, "syscall__probe_entry_recvfrom"},
     {"recvfrom", ProbeType::kReturn, "syscall__probe_ret_recvfrom"},
     {"recvmsg", ProbeType::kEntry, "syscall__probe_entry_recvmsg"},
     {"recvmsg", ProbeType::kReturn, "syscall__probe_ret_recvmsg"},
     {"recvmmsg", ProbeType::kEntry, "syscall__probe_entry_recvmmsg"},
     {"recvmmsg", ProbeType::kReturn, "syscall__probe_ret_recvmmsg"},
     {"close", ProbeType::kEntry, "syscall__probe_entry_close"},
     {"close", ProbeType::kReturn, "syscall__probe_ret_close"},
     {"mmap", ProbeType::kEntry, "syscall__probe_entry_mmap"},
     {"sock_alloc", ProbeType::kReturn, "probe_ret_sock_alloc", /*is_syscall*/ false},
     {"security_socket_sendmsg", ProbeType::kEntry, "probe_entry_security_socket_sendmsg",
      /*is_syscall*/ false},
     {"security_socket_recvmsg", ProbeType::kEntry, "probe_entry_security_socket_recvmsg",
      /*is_syscall*/ false}});

using bpf_tools::PerfBufferSizeCategory;

namespace {
// Resize each category of perf buffers such that it doesn't exceed a maximum size across all cpus.
template <size_t N>
void ResizePerfBufferSpecs(std::array<bpf_tools::PerfBufferSpec, N>* perf_buffer_specs,
                           const std::map<PerfBufferSizeCategory, size_t>& category_maximums) {
  std::map<PerfBufferSizeCategory, size_t> category_sizes;
  for (const auto& spec : *perf_buffer_specs) {
    category_sizes[spec.size_category] += spec.size_bytes;
  }

  const int kNCPUs = get_nprocs_conf();
  // Factor to multiply on both sides of division, to avoid float/double division.
  const size_t kDivisorFactor = 100;
  std::map<PerfBufferSizeCategory, size_t> category_divisor;
  for (const auto& [category, size] : category_sizes) {
    auto max_it = category_maximums.find(category);
    DCHECK(max_it != category_maximums.end());

    auto max_per_cpu = max_it->second / kNCPUs;

    if (size < max_per_cpu) {
      category_divisor[category] = kDivisorFactor;
    } else {
      category_divisor[category] = IntRoundUpDivide(size * kDivisorFactor, max_per_cpu);
    }
  }
  // Clear category sizes so we can print out the total buffer sizes at the end.
  category_sizes.clear();
  for (auto& spec : *perf_buffer_specs) {
    auto divisor_it = category_divisor.find(spec.size_category);
    DCHECK(divisor_it != category_divisor.end());
    spec.size_bytes = IntRoundUpDivide(spec.size_bytes * kDivisorFactor, divisor_it->second);
    category_sizes[spec.size_category] += spec.size_bytes;
  }
  for (const auto& [category, size] : category_sizes) {
    LOG(INFO) << absl::Substitute("Total perf buffer usage for $0 buffers across all cpus: $1",
                                  magic_enum::enum_name(category), size * kNCPUs);
  }
}
}  // namespace

auto SocketTraceConnector::InitPerfBufferSpecs() {
  const size_t ncpus = get_nprocs_conf();

  double cpu_scaling_factor = (1 + FLAGS_stirling_socket_tracer_percpu_bw_scaling_factor) /
                              (ncpus + FLAGS_stirling_socket_tracer_percpu_bw_scaling_factor);
  LOG(INFO) << absl::Substitute("Initializing perf buffers with ncpus=$0 and scaling_factor=$1",
                                ncpus, cpu_scaling_factor);

  const double kSecondsPerPeriod =
      std::chrono::duration_cast<std::chrono::milliseconds>(kSamplingPeriod).count() / 1000.0;
  const int kTargetDataBufferSize = static_cast<int>(
      FLAGS_stirling_socket_tracer_target_data_bw_percpu * kSecondsPerPeriod * cpu_scaling_factor);
  const int kTargetControlBufferSize =
      static_cast<int>(FLAGS_stirling_socket_tracer_target_control_bw_percpu * kSecondsPerPeriod *
                       cpu_scaling_factor);

  const int kMaxTotalDataSize =
      static_cast<int64_t>(FLAGS_stirling_socket_tracer_max_total_bw_overprovision_factor *
                           FLAGS_stirling_socket_tracer_max_total_data_bw * kSecondsPerPeriod);
  const int kMaxTotalControlSize =
      static_cast<int64_t>(FLAGS_stirling_socket_tracer_max_total_bw_overprovision_factor *
                           FLAGS_stirling_socket_tracer_max_total_control_bw * kSecondsPerPeriod);
  std::map<PerfBufferSizeCategory, size_t> category_maximums(
      {{PerfBufferSizeCategory::kData, kMaxTotalDataSize},
       {PerfBufferSizeCategory::kControl, kMaxTotalControlSize}});

  auto specs = MakeArray<bpf_tools::PerfBufferSpec>({
      // For data events. The order must be consistent with output tables.
      {"socket_data_events", HandleDataEvent, HandleDataEventLoss, this, kTargetDataBufferSize,
       PerfBufferSizeCategory::kData},
      // For non-data events. Must not mix with the above perf buffers for data events.
      {"socket_control_events", HandleControlEvent, HandleControlEventLoss, this,
       kTargetControlBufferSize, PerfBufferSizeCategory::kControl},
      {"conn_stats_events", HandleConnStatsEvent, HandleConnStatsEventLoss, this,
       kTargetControlBufferSize, PerfBufferSizeCategory::kControl},
      {"mmap_events", HandleMMapEvent, HandleMMapEventLoss, this, kTargetControlBufferSize / 10,
       PerfBufferSizeCategory::kControl},
      {"go_grpc_events", HandleHTTP2Event, HandleHTTP2EventLoss, this, kTargetDataBufferSize,
       PerfBufferSizeCategory::kData},
      {"grpc_c_events", HandleGrpcCEvent, HandleGrpcCDataLoss, this, kTargetDataBufferSize,
       PerfBufferSizeCategory::kData},
      {"grpc_c_header_events", HandleGrpcCHeaderEvent, HandleGrpcCHeaderDataLoss, this,
       kTargetDataBufferSize, PerfBufferSizeCategory::kData},
      {"grpc_c_close_events", HandleGrpcCCloseEvent, HandleGrpcCCloseDataLoss, this,
       kTargetDataBufferSize, PerfBufferSizeCategory::kData},
  });
  ResizePerfBufferSpecs(&specs, category_maximums);
  return specs;
}

Status SocketTraceConnector::InitBPF() {
  // set BPF loop limit and chunk limit based on kernel version
  auto kernel = system::GetCachedKernelVersion();
  if (kernel.version >= 5 || (kernel.version == 5 && kernel.major_rev >= 1)) {
    // Kernels >= 5.1 have higher BPF instruction limits (1 million for verifier).
    // This enables a 21x increase to our loop and chunk limits
    FLAGS_stirling_bpf_loop_limit = 882;
    FLAGS_stirling_bpf_chunk_limit = 84;
    LOG(INFO) << absl::Substitute(
        "Kernel version greater than V5.1 detected ($0), raised loop limit to $1 and chunk limit "
        "to $2",
        kernel.ToString(), FLAGS_stirling_bpf_loop_limit, FLAGS_stirling_bpf_chunk_limit);
  }
  // PROTOCOL_LIST: Requires update on new protocols.
  std::vector<std::string> defines = {
      absl::StrCat("-DENABLE_TLS_DEBUG_SOURCES=", FLAGS_stirling_debug_tls_sources),
      absl::StrCat("-DENABLE_HTTP_TRACING=", protocol_transfer_specs_[kProtocolHTTP].enabled),
      absl::StrCat("-DENABLE_CQL_TRACING=", protocol_transfer_specs_[kProtocolCQL].enabled),
      absl::StrCat("-DENABLE_MUX_TRACING=", protocol_transfer_specs_[kProtocolMux].enabled),
      absl::StrCat("-DENABLE_PGSQL_TRACING=", protocol_transfer_specs_[kProtocolPGSQL].enabled),
      absl::StrCat("-DENABLE_MYSQL_TRACING=", protocol_transfer_specs_[kProtocolMySQL].enabled),
      absl::StrCat("-DENABLE_KAFKA_TRACING=", protocol_transfer_specs_[kProtocolKafka].enabled),
      absl::StrCat("-DENABLE_DNS_TRACING=", protocol_transfer_specs_[kProtocolDNS].enabled),
      absl::StrCat("-DENABLE_REDIS_TRACING=", protocol_transfer_specs_[kProtocolRedis].enabled),
      absl::StrCat("-DENABLE_NATS_TRACING=", protocol_transfer_specs_[kProtocolNATS].enabled),
      absl::StrCat("-DENABLE_AMQP_TRACING=", protocol_transfer_specs_[kProtocolAMQP].enabled),
      absl::StrCat("-DENABLE_MONGO_TRACING=", protocol_transfer_specs_[kProtocolMongo].enabled),
      absl::StrCat("-DBPF_LOOP_LIMIT=", FLAGS_stirling_bpf_loop_limit),
      absl::StrCat("-DBPF_CHUNK_LIMIT=", FLAGS_stirling_bpf_chunk_limit),
  };
  PX_RETURN_IF_ERROR(bcc_->InitBPFProgram(socket_trace_bcc_script, defines));

  PX_RETURN_IF_ERROR(bcc_->AttachKProbes(kProbeSpecs));
  LOG(INFO) << absl::Substitute("Number of kprobes deployed = $0", kProbeSpecs.size());
  LOG(INFO) << "Probes successfully deployed.";

  const auto perf_buffer_specs = InitPerfBufferSpecs();
  PX_RETURN_IF_ERROR(bcc_->OpenPerfBuffers(perf_buffer_specs));
  LOG(INFO) << absl::Substitute("Number of perf buffers opened = $0", perf_buffer_specs.size());

  // Set trace role to BPF probes.
  for (const auto& p : magic_enum::enum_values<traffic_protocol_t>()) {
    if (protocol_transfer_specs_[p].enabled) {
      uint64_t role_mask = 0;
      for (auto role : protocol_transfer_specs_[p].trace_roles) {
        role_mask |= role;
      }
      PX_RETURN_IF_ERROR(UpdateBPFProtocolTraceRole(p, role_mask));
    }
  }

  PX_RETURN_IF_ERROR(TestOnlySetTargetPID());
  if (FLAGS_stirling_disable_self_tracing) {
    PX_RETURN_IF_ERROR(DisableSelfTracing());
  }
  if (!FLAGS_socket_trace_data_events_output_path.empty()) {
    SetupOutput(FLAGS_socket_trace_data_events_output_path);
  }

  return Status::OK();
}

namespace {

// Check debugging flags are compatible with each other.
void CheckDebugFlags() {
  if (FLAGS_test_only_socket_trace_target_pid != kTraceAllTGIDs &&
      FLAGS_stirling_conn_trace_pid != kUnsetPIDFD) {
    CHECK_EQ(FLAGS_test_only_socket_trace_target_pid, FLAGS_stirling_conn_trace_pid)
        << "--test_only_socket_trace_target_pid and --stirling_conn_trace_pid flag values, "
           "when both specified, must be the same.";
  }
}

}  // namespace

Status SocketTraceConnector::InitImpl() {
  CheckDebugFlags();

  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);

  constexpr uint64_t kNanosPerSecond = 1000 * 1000 * 1000;
  if (kNanosPerSecond % sysconfig_.KernelTicksPerSecond() != 0) {
    return error::Internal(
        "SC_CLK_TCK aka USER_HZ must be 100, otherwise our BPF code may not generate proper "
        "timestamps in a way that matches how /proc/stat does it");
  }

  PX_RETURN_IF_ERROR(InitBPF());

  auto s = system::SocketInfoManager::Create(
      ProcPath(), system::kTCPEstablishedState | system::kTCPListeningState);
  if (!s.ok()) {
    LOG(WARNING) << absl::Substitute("Failed to set up SocketInfoManager. Message: $0", s.msg());
  } else {
    socket_info_mgr_ = s.ConsumeValueOrDie();
  }

  conn_info_map_mgr_ = std::make_shared<ConnInfoMapManager>(bcc_.get());
  ConnTracker::SetConnInfoMapManager(conn_info_map_mgr_);

  uprobe_mgr_.Init(FLAGS_stirling_disable_golang_tls_tracing,
                   protocol_transfer_specs_[kProtocolHTTP2].enabled,
                   FLAGS_stirling_disable_self_tracing);

  openssl_trace_state_ = WrappedBCCArrayTable<int>::Create(bcc_.get(), "openssl_trace_state");
  openssl_trace_state_debug_ = WrappedBCCMap<uint32_t, struct openssl_trace_state_debug_t>::Create(
      bcc_.get(), "openssl_trace_state_debug");

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
  bcc_->Close();
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
  auto map = WrappedBCCMap<TBPFTableKey, TBPFTableVal>::Create(bcc, name.data());

  size_t map_size = map->GetTableOffline().size();
  if (1.0 * map_size / map->capacity() > 0.9) {
    LOG(WARNING) << absl::Substitute("BPF Table $0 is nearly at capacity [size=$0 capacity=$1]",
                                     map_size, map->capacity());
  }
  return absl::Substitute("\nBPFTable=$0 occupancy=$1 capacity=$2", name, map_size,
                          map->capacity());
}

std::string BPFMapsInfo(bpf_tools::BCCWrapper* bcc) {
  std::string out;

  out += BPFMapInfo<uint32_t, struct go_common_symaddrs_t>(bcc, "go_common_symaddrs_map");
  out += BPFMapInfo<uint32_t, struct openssl_symaddrs_t>(bcc, "openssl_symaddrs_map");
  out += BPFMapInfo<uint64_t, struct data_args_t>(bcc, "active_ssl_read_args_map");
  out += BPFMapInfo<uint64_t, struct data_args_t>(bcc, "active_ssl_write_args_map");
  out += BPFMapInfo<uint32_t, struct go_tls_symaddrs_t>(bcc, "go_tls_symaddrs_map");
  out += BPFMapInfo<uint32_t, struct go_http2_symaddrs_t>(bcc, "http2_symaddrs_map");
  out += BPFMapInfo<void*, struct go_grpc_event_attr_t>(bcc, "active_write_headers_frame_map");
  out += BPFMapInfo<uint64_t, struct conn_info_t>(bcc, "conn_info_map");
  out += BPFMapInfo<uint64_t, uint64_t>(bcc, "conn_disabled_map");
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
  bcc_->PollPerfBuffers();

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

// Verifies that our openssl tracing does not encounter conditions that invalidate our
// assumptions and records error conditions in prometheus metrics.
void SocketTraceConnector::CheckTracerState() {
  // The check that state is not uninitialized is required for socket_trace_connector_test.
  // Since it doesn't initialize the BPF program, accessing the map will fail.
  if (state() == State::kUninitialized) {
    return;
  }

  const int error_code =
      openssl_trace_state_->GetValue(kOpenSSLTraceStatusIdx).ConsumeValueOr(kOpenSSLTraceOk);
  const bool mismatched_fds = error_code == kOpenSSLMismatchedFDsDetected;

  if (FLAGS_stirling_debug_tls_sources || mismatched_fds) {
    if (mismatched_fds) {
      openssl_trace_mismatched_fds_counter_family_.Add({{"name", openssl_mismatched_fds_metric}})
          .Increment();
    }

    // Record the offending applications and clear the BPF hash in the process.
    auto table = openssl_trace_state_debug_->GetTableOffline(true);
    for (auto& entry : table) {
      struct openssl_trace_state_debug_t debug = std::get<1>(entry);
      auto ssl_source = std::string(magic_enum::enum_name(debug.ssl_source));
      auto is_mismatched_entry = debug.mismatched_fd;

      if (is_mismatched_entry) {
        openssl_trace_mismatched_fds_counter_family_
            .Add({{"name", openssl_mismatched_fds_metric},
                  {"exe", debug.comm},
                  {"ssl_source", ssl_source}})
            .Increment();
      }

      if (FLAGS_stirling_debug_tls_sources) {
        auto protocol = std::string(magic_enum::enum_name(debug.protocol));
        openssl_trace_tls_source_counter_family_
            .Add({
                {"name", openssl_tls_source_metric},
                {"exe", debug.comm},
                {"ssl_source", ssl_source},
                {"protocol", protocol},
            })
            .Increment();
      }
    }
  }
  DCHECK_EQ(error_code, kOpenSSLTraceOk);

  // Reset the BPF map to its default value so that each occurrence
  // can be detected.
  if (error_code != kOpenSSLTraceOk) {
    PX_UNUSED(openssl_trace_state_->SetValue(kOpenSSLTraceStatusIdx, kOpenSSLTraceOk));
  }
}

using stream_id_t = protocols::http::stream_id_t;
using message_t = protocols::http::Message;

void SocketTraceConnector::TransferDataImpl(ConnectorContext* ctx) {
  set_iteration_time(now_fn_());

  UpdateCommonState(ctx);

  DataTable* conn_stats_table = data_tables_[kConnStatsTableNum];
  if (conn_stats_table != nullptr &&
      sampling_freq_mgr_.count() % FLAGS_stirling_conn_stats_sampling_ratio == 0) {
    TransferConnStats(ctx, conn_stats_table);
  }

  if ((sampling_freq_mgr_.count() + 1) % FLAGS_stirling_socket_tracer_stats_logging_ratio == 0) {
    conn_trackers_mgr_.ComputeProtocolStats();
    LOG(INFO) << "ConnTracker statistics: " << conn_trackers_mgr_.StatsString();
    LOG(INFO) << "SocketTracer statistics: " << stats_.Print();
  }

  constexpr auto kDebugDumpPeriod = std::chrono::minutes(1);
  if (sampling_freq_mgr_.count() % (kDebugDumpPeriod / kSamplingPeriod) == 0) {
    if (debug_level_ >= 1) {
      LOG(INFO) << "Context: " << DumpContext(ctx);
      LOG(INFO) << "BPF map info: " << BPFMapsInfo(bcc_.get());
    }
  }

  std::vector<CIDRBlock> cluster_cidrs = ctx->GetClusterCIDRs();

  for (size_t i = 0; i < data_tables_.size(); ++i) {
    DataTable* data_table = data_tables_[i];

    // Ensure records are within the time window, in order to ensure the order between record
    // batches. Exception: conn_stats table does not need cutoff time, because its timestamps
    // are assigned artificially.
    if (i != kConnStatsTableNum && data_table != nullptr) {
      data_table->SetConsumeRecordsCutoffTime(perf_buffer_drain_time_);
    }
  }

  for (const auto& conn_tracker : conn_trackers_mgr_.active_trackers()) {
    const auto& transfer_spec = protocol_transfer_specs_[conn_tracker->protocol()];

    DataTable* data_table = nullptr;
    if (transfer_spec.enabled) {
      data_table = data_tables_[transfer_spec.table_num];
    }

    UpdateTrackerTraceLevel(conn_tracker);

    // Once a known UPID, always a known UPID.
    if (!conn_tracker->is_tracked_upid()) {
      md::UPID upid(ctx->GetASID(), conn_tracker->conn_id().upid.pid,
                    conn_tracker->conn_id().upid.start_time_ticks);
      if (ctx->GetUPIDs().contains(upid)) {
        conn_tracker->set_is_tracked_upid();
      }
    }

    conn_tracker->IterationPreTick(iteration_time_, cluster_cidrs, proc_parser_.get(),
                                   socket_info_mgr_.get());

    if (transfer_spec.transfer_fn != nullptr) {
      transfer_spec.transfer_fn(*this, ctx, conn_tracker, data_table);
    } else {
      // If there's no transfer function, then the tracker should not be holding any data.
      // http::ProtocolTraits is used as a placeholder; the frames deque is expected to be
      // std::monostate.
      DCHECK((conn_tracker->send_data().Empty<stream_id_t, message_t>()));
      DCHECK((conn_tracker->recv_data().Empty<stream_id_t, message_t>()));
    }

    conn_tracker->IterationPostTick();
  }

  CheckTracerState();

  // Once we've cleared all the debug trace levels for this pid, we can remove it from the list.
  pids_to_trace_disable_.clear();
}

Status SocketTraceConnector::UpdateBPFProtocolTraceRole(traffic_protocol_t protocol,
                                                        uint64_t role_mask) {
  auto control_map = WrappedBCCPerCPUArrayTable<uint64_t>::Create(bcc_.get(), kControlMapName);
  return control_map->SetValues(static_cast<int>(protocol), role_mask);
}

Status SocketTraceConnector::TestOnlySetTargetPID() {
  int64_t pid = FLAGS_test_only_socket_trace_target_pid;
  if (pid != kTraceAllTGIDs) {
    LOG(WARNING) << absl::Substitute(
        "Target trace PID set to pid=$0, will force BPF to ignore event filtering and "
        "submit events of this PID to userspace",
        pid);
    LOG(WARNING) << absl::Substitute(
        "Enable CONN_TRACE for pid=$0 following --test_only_socket_trace_target_pid", pid);
    FLAGS_stirling_conn_trace_pid = pid;
  }
  auto control_map =
      WrappedBCCPerCPUArrayTable<int64_t>::Create(bcc_.get(), kControlValuesArrayName);
  return control_map->SetValues(kTargetTGIDIndex, pid);
}

Status SocketTraceConnector::DisableSelfTracing() {
  auto control_map =
      WrappedBCCPerCPUArrayTable<int64_t>::Create(bcc_.get(), kControlValuesArrayName);
  const int64_t self_pid = getpid();
  return control_map->SetValues(kStirlingTGIDIndex, self_pid);
}

//-----------------------------------------------------------------------------
// Perf Buffer Polling and Callback functions.
//-----------------------------------------------------------------------------

void SocketTraceConnector::HandleDataEvent(void* cb_cookie, void* data, int data_size) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  connector->stats_.Increment(StatKey::kPollSocketDataEventSize, data_size);

  std::unique_ptr<SocketDataEvent> data_event_ptr = std::make_unique<SocketDataEvent>(data);

  // The servers of certain protocols (e.g. Kafka) read the length headers of frames separately
  // from the payload. In these cases, the protocol inference misses the header of the first frame.
  // This header is encoded in the attributes instead.
  // We account for this with a separate header event.
  std::unique_ptr<SocketDataEvent> header_event_ptr = data_event_ptr->ExtractHeaderEvent();

  // In some scenarios when we are unable to trace the data (notably including sendfile syscalls),
  // we create a filler event instead. This is important to Kafka, for example,
  // where the sendfile data is in the payload and the protocol parser can still succeed
  // as long as it is properly accounted for.
  std::unique_ptr<SocketDataEvent> filler_event_ptr = data_event_ptr->ExtractFillerEvent();

  if (header_event_ptr) {
    connector->AcceptDataEvent(std::move(header_event_ptr));
  }
  if (data_event_ptr && !data_event_ptr->msg.empty()) {
    connector->AcceptDataEvent(std::move(data_event_ptr));
  }
  if (filler_event_ptr) {
    connector->AcceptDataEvent(std::move(filler_event_ptr));
  }
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

void SocketTraceConnector::HandleHTTP2Event(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);

  // Note: Directly accessing data through the data pointer can result in mis-aligned accesses.
  // This is because the perf buffer data starts at an offset of 4 bytes.
  // Accessing the event_type should be safe as long as it is 4-byte data type.
  auto event_type = reinterpret_cast<go_grpc_event_attr_t*>(data)->event_type;

  switch (event_type) {
    case kHeaderEventRead:
    case kHeaderEventWrite: {
      auto event = std::make_unique<HTTP2HeaderEvent>(data);

      VLOG(3) << absl::Substitute(
          "t=$0 pid=$1 type=$2 fd=$3 tsid=$4 stream_id=$5 end_stream=$6 name=$7 value=$8",
          event->attr.timestamp_ns, event->attr.conn_id.upid.pid,
          magic_enum::enum_name(event->attr.event_type), event->attr.conn_id.fd,
          event->attr.conn_id.tsid, event->attr.stream_id, event->attr.end_stream, event->name,
          event->value);
      connector->AcceptHTTP2Header(std::move(event));
    } break;
    case kDataFrameEventRead:
    case kDataFrameEventWrite: {
      auto event = std::make_unique<HTTP2DataEvent>(data);

      VLOG(3) << absl::Substitute(
          "t=$0 pid=$1 type=$2 fd=$3 tsid=$4 stream_id=$5 end_stream=$6 data=$7",
          event->attr.timestamp_ns, event->attr.conn_id.upid.pid,
          magic_enum::enum_name(event->attr.event_type), event->attr.conn_id.fd,
          event->attr.conn_id.tsid, event->attr.stream_id, event->attr.end_stream, event->payload);
      connector->AcceptHTTP2Data(std::move(event));
    } break;
    default:
      LOG(DFATAL) << absl::Substitute("Unexpected event_type $0",
                                      magic_enum::enum_name(event_type));
  }
}

void SocketTraceConnector::HandleHTTP2EventLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossHTTP2Event, lost);
}

void SocketTraceConnector::HandleGrpcCHeaderEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(data != nullptr) << "GrpcCHeaderEvent data argument is null.";
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  struct grpc_c_header_event_data_t* event_data =
      static_cast<struct grpc_c_header_event_data_t*>(data);
  // TODO(yzhao): Implement a C++ companion of the C struct so that smart object management applies.
  auto event = std::make_unique<struct grpc_c_header_event_data_t>(*event_data);

  connector->AcceptGrpcCHeaderEventData(std::move(event));
}

void SocketTraceConnector::HandleGrpcCEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(data != nullptr) << "GrpcCEvent data argument is null.";
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  struct grpc_c_event_data_t* event_data = static_cast<struct grpc_c_event_data_t*>(data);
  // TODO(yzhao): Implement a C++ companion of the C struct so that smart object management applies.
  auto event = std::make_unique<struct grpc_c_event_data_t>(*event_data);

  connector->AcceptGrpcCEventData(std::move(event));
}

void SocketTraceConnector::HandleGrpcCCloseEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(data != nullptr) << "GrpcCCloseEvent data argument is null.";
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  struct grpc_c_stream_closed_data* event_data =
      static_cast<struct grpc_c_stream_closed_data*>(data);
  auto event = std::make_unique<struct grpc_c_stream_closed_data>(*event_data);

  connector->AcceptGrpcCCloseEvent(std::move(event));
}

void SocketTraceConnector::HandleGrpcCDataLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossGrpcCEvent, lost);
}

void SocketTraceConnector::HandleGrpcCHeaderDataLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossGrpcCHeaderEvent,
                                                                  lost);
}

void SocketTraceConnector::HandleGrpcCCloseDataLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  static_cast<SocketTraceConnector*>(cb_cookie)->stats_.Increment(StatKey::kLossGrpcCCloseEvent,
                                                                  lost);
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
  if (perf_buffer_events_output_stream_ != nullptr) {
    WriteDataEvent(*event);
  }

  stats_.Increment(StatKey::kPollSocketDataEventCount);
  stats_.Increment(StatKey::kPollSocketDataEventAttrSize, sizeof(event->attr));
  stats_.Increment(StatKey::kPollSocketDataEventDataSize, event->msg.size());

  ConnTracker& tracker = GetOrCreateConnTracker(event->attr.conn_id);
  tracker.AddDataEvent(std::move(event));
}

void SocketTraceConnector::AcceptControlEvent(socket_control_event_t event) {
  ConnTracker& tracker = GetOrCreateConnTracker(event.conn_id);
  tracker.AddControlEvent(event);
}

void SocketTraceConnector::AcceptConnStatsEvent(conn_stats_event_t event) {
  ConnTracker& tracker = conn_trackers_mgr_.GetOrCreateConnTracker(event.conn_id);
  tracker.AddConnStats(event);
}

void SocketTraceConnector::AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event) {
  ConnTracker& tracker = GetOrCreateConnTracker(event->attr.conn_id);
  tracker.AddHTTP2Header(std::move(event));
}

void SocketTraceConnector::AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event) {
  ConnTracker& tracker = GetOrCreateConnTracker(event->attr.conn_id);
  tracker.AddHTTP2Data(std::move(event));
}

void SocketTraceConnector::AcceptGrpcCEvent(struct conn_id_t connection_id, uint32_t stream_id,
                                            uint64_t timestamp, bool outgoing,
                                            uint64_t position_in_stream,
                                            std::vector<struct grpc_c_data_slice_t> slices) {
  // Initiate data event template as if it arrived from Golang gRPC eBPF.
  struct go_grpc_data_event_t data_event_go_style = {};

  data_event_go_style.attr.end_stream = false;  // End stream will be on an empty header, not data.
  data_event_go_style.attr.stream_id = stream_id;
  if (outgoing == true) {
    data_event_go_style.attr.event_type = kDataFrameEventWrite;
  } else {
    data_event_go_style.attr.event_type = kDataFrameEventRead;
  }
  data_event_go_style.data_attr.pos = position_in_stream;
  data_event_go_style.attr.timestamp_ns = timestamp;
  data_event_go_style.attr.probe_type = k_probe_type_other;
  data_event_go_style.attr.conn_id = connection_id;

  uint64_t extra_position = 0;
  for (struct grpc_c_data_slice_t data_slice : slices) {
    struct go_grpc_data_event_t current_data_event = data_event_go_style;
    current_data_event.data_attr.pos = position_in_stream += extra_position;
    extra_position += data_slice.length;  // Add extra position for next slices.
    current_data_event.data_attr.data_size = data_slice.length;
    current_data_event.data_attr.data_buf_size =
        std::min((uint32_t)data_slice.length, (uint32_t)GRPC_C_SLICE_SIZE);
    if ((uint32_t)data_slice.length > (uint32_t)GRPC_C_SLICE_SIZE) {
      LOG(WARNING) << absl::Substitute("gRPC-c $0 slice of size $1 is larger than maximum size $2",
                                       outgoing ? "outgoing" : "incoming",
                                       (uint32_t)data_slice.length, (uint32_t)GRPC_C_SLICE_SIZE);
    }

    auto d_event_ready = std::make_unique<HTTP2DataEvent>();
    d_event_ready->attr = current_data_event.attr;
    d_event_ready->data_attr = current_data_event.data_attr;
    d_event_ready->payload =
        std::string_view(data_slice.bytes, current_data_event.data_attr.data_buf_size);
    this->AcceptHTTP2Data(std::move(d_event_ready));
  }
}

void SocketTraceConnector::AcceptGrpcCEventData(std::unique_ptr<struct grpc_c_event_data_t> event) {
  if (event->direction != kIngress && event->direction != kEgress) {
    LOG(WARNING) << absl::Substitute("gRPC-C event from pid $0 with invalid direction $1",
                                     event->conn_id.upid.pid, event->direction);
    return;
  }

  // Prepare list of slices.
  std::vector<struct grpc_c_data_slice_t> slices(0);
  slices.push_back(event->slice);

  this->AcceptGrpcCEvent(event->conn_id, event->stream_id, event->timestamp,
                         event->direction == kEgress, event->position_in_stream, std::move(slices));
}

void SocketTraceConnector::InitiateHeaderEventDataGoStyle(
    struct conn_id_t conn_id, uint32_t stream_id, uint64_t timestamp, bool end_stream,
    bool outgoing,
    /* OUT */ struct go_grpc_http2_header_event_t* header_event_data_go_style) {
  if (NULL == header_event_data_go_style) {
    LOG(WARNING) << "Initiation of header event data at Go-style with null pointer.";
    return;
  }
  memset(header_event_data_go_style, 0, sizeof(*header_event_data_go_style));
  header_event_data_go_style->attr.end_stream = end_stream;
  header_event_data_go_style->attr.stream_id = stream_id;
  header_event_data_go_style->attr.conn_id = conn_id;
  header_event_data_go_style->attr.timestamp_ns = timestamp;
  header_event_data_go_style->attr.probe_type = k_probe_type_other;
  if (outgoing) {
    header_event_data_go_style->attr.event_type = kHeaderEventWrite;
  } else {
    header_event_data_go_style->attr.event_type = kHeaderEventRead;
  }
}

void SocketTraceConnector::AcceptGrpcCHeaderEventData(
    std::unique_ptr<struct grpc_c_header_event_data_t> event) {
  if (event->direction != kIngress && event->direction != kEgress) {
    LOG(WARNING) << absl::Substitute("gRPC-C header event from pid $0 with invalid direction $1",
                                     event->conn_id.upid.pid, event->direction);
    return;
  }

  // Initiate header event template as if it arrived from Golang gRPC eBPF.
  struct go_grpc_http2_header_event_t header_event_data_go_style = {};
  InitiateHeaderEventDataGoStyle(event->conn_id, event->stream_id, event->timestamp,
                                 /* Whether this event indicates that a stream was closed */ false,
                                 event->direction == kEgress, &header_event_data_go_style);

  std::string name(event->header.key, event->header.key_size);
  std::string value(event->header.value, event->header.value_size);
  auto h_event_ready = std::make_unique<HTTP2HeaderEvent>();
  h_event_ready->attr = header_event_data_go_style.attr;
  h_event_ready->name = name;
  h_event_ready->value = value;

  this->AcceptHTTP2Header(std::move(h_event_ready));
}

void SocketTraceConnector::AcceptGrpcCCloseEvent(
    std::unique_ptr<struct grpc_c_stream_closed_data> event) {
  // Initiate header event template as if it arrived from Golang gRPC eBPF.
  struct go_grpc_http2_header_event_t header_event_data_go_style = {};
  InitiateHeaderEventDataGoStyle(
      event->conn_id, event->stream_id, event->timestamp,
      /* Whether this event indicates that a stream was closed */ true,
      /* Whether outgoing. This does not matter because it's overridden later */ true,
      &header_event_data_go_style);

  if (event->write_closed) {
    header_event_data_go_style.attr.event_type = kHeaderEventWrite;
    auto h_event_ready = std::make_unique<HTTP2HeaderEvent>();
    h_event_ready->attr = header_event_data_go_style.attr;
    h_event_ready->name = "";
    h_event_ready->value = "";
    this->AcceptHTTP2Header(std::move(h_event_ready));
  }

  if (event->read_closed) {
    header_event_data_go_style.attr.event_type = kHeaderEventRead;
    auto h_event_ready = std::make_unique<HTTP2HeaderEvent>();
    h_event_ready->attr = header_event_data_go_style.attr;
    h_event_ready->name = "";
    h_event_ready->value = "";
    this->AcceptHTTP2Header(std::move(h_event_ready));
  }
}

//-----------------------------------------------------------------------------
// Append-Related Functions
//-----------------------------------------------------------------------------

namespace {

int64_t CalculateLatency(int64_t req_timestamp_ns, int64_t resp_timestamp_ns) {
  int64_t latency_ns = 0;
  if (req_timestamp_ns > 0 && resp_timestamp_ns > 0) {
    latency_ns = resp_timestamp_ns - req_timestamp_ns;
    LOG_IF_EVERY_N(WARNING, latency_ns < 0, 100)
        << absl::Substitute("Negative latency implies req resp mismatch [t_req=$0, t_resp=$1].",
                            req_timestamp_ns, resp_timestamp_ns);
  }
  return latency_ns;
}

int64_t AMQPCalculateLatency(const int64_t req_timestamp_ns, const int64_t resp_timestamp_ns,
                             const bool req_synchronous, const bool resp_synchronous) {
  if (!req_synchronous || !resp_synchronous) {
    return 0;
  }

  if (req_timestamp_ns <= 0 || resp_timestamp_ns <= 0) {
    return 0;
  }

  const int64_t latency_ns =
      std::max(req_timestamp_ns, resp_timestamp_ns) - std::min(req_timestamp_ns, resp_timestamp_ns);
  return latency_ns;
}

template <typename TRecordType>
std::string PXInfoString(const ConnTracker& conn_tracker, const TRecordType& record) {
  return absl::Substitute("conn_tracker=$0 record=$1", conn_tracker.ToString(), record.ToString());
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
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("major_version")>(1);
  r.Append<r.ColIndex("minor_version")>(resp_message.minor_version);
  r.Append<r.ColIndex("content_type")>(static_cast<uint64_t>(content_type));
  r.Append<r.ColIndex("req_headers")>(ToJSONString(req_message.headers), kMaxHTTPHeadersBytes);
  r.Append<r.ColIndex("req_method")>(std::move(req_message.req_method));
  r.Append<r.ColIndex("req_path")>(std::move(req_message.req_path));
  r.Append<r.ColIndex("req_body_size")>(req_message.body_size);
  r.Append<r.ColIndex("req_body")>(std::move(req_message.body), FLAGS_max_body_bytes);
  r.Append<r.ColIndex("resp_headers")>(ToJSONString(resp_message.headers), kMaxHTTPHeadersBytes);
  r.Append<r.ColIndex("resp_status")>(resp_message.resp_status);
  r.Append<r.ColIndex("resp_message")>(std::move(resp_message.resp_message));
  r.Append<r.ColIndex("resp_body_size")>(resp_message.body_size);
  r.Append<r.ColIndex("resp_body")>(std::move(resp_message.body), FLAGS_max_body_bytes);
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(req_message.timestamp_ns, resp_message.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, record));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::http2::Record record, DataTable* data_table) {
  using ::px::grpc::MethodInputOutput;
  using ::px::stirling::grpc::ParseReqRespBody;

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
  if (record.HasGRPCContentType()) {
    content_type = HTTPContentType::kGRPC;
  }

  ParseReqRespBody(&record, DataTable::kTruncatedMsg, kMaxPBStringLen);

  DataTable::RecordBuilder<&kHTTPTable> r(data_table, resp_stream->timestamp_ns);
  r.Append<r.ColIndex("time_")>(resp_stream->timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("major_version")>(2);
  // HTTP2 does not define minor version.
  r.Append<r.ColIndex("minor_version")>(0);
  r.Append<r.ColIndex("req_headers")>(ToJSONString(req_stream->headers()), kMaxHTTPHeadersBytes);
  r.Append<r.ColIndex("content_type")>(static_cast<uint64_t>(content_type));
  r.Append<r.ColIndex("resp_headers")>(ToJSONString(resp_stream->headers()), kMaxHTTPHeadersBytes);
  r.Append<r.ColIndex("req_method")>(
      req_stream->headers().ValueByKey(protocols::http2::headers::kMethod));
  r.Append<r.ColIndex("req_path")>(req_stream->headers().ValueByKey(":path"));
  r.Append<r.ColIndex("resp_status")>(resp_status);
  // TODO(yzhao): Populate the following field from headers.
  r.Append<r.ColIndex("resp_message")>("OK");
  r.Append<r.ColIndex("req_body_size")>(req_stream->original_data_size());
  // Do not apply truncation at this point, as the truncation was already done on serialized
  // protobuf message. This might result into longer text format data here, but the increase is
  // minimal.
  r.Append<r.ColIndex("req_body")>(req_stream->ConsumeData());
  r.Append<r.ColIndex("resp_body_size")>(resp_stream->original_data_size());
  r.Append<r.ColIndex("resp_body")>(resp_stream->ConsumeData());
  int64_t latency_ns = CalculateLatency(req_stream->timestamp_ns, resp_stream->timestamp_ns);
  r.Append<r.ColIndex("latency")>(latency_ns);
  // TODO(yzhao): Remove once http2::Record::bpf_timestamp_ns is removed.
  LOG_IF_EVERY_N(WARNING, latency_ns < 0, 100)
      << absl::Substitute("Negative latency found in HTTP2 records, record=$0", record.ToString());
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, record));
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
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("req_cmd")>(static_cast<uint64_t>(entry.req.cmd));
  r.Append<r.ColIndex("req_body")>(std::move(entry.req.msg), FLAGS_max_body_bytes);
  r.Append<r.ColIndex("resp_status")>(static_cast<uint64_t>(entry.resp.status));
  r.Append<r.ColIndex("resp_body")>(std::move(entry.resp.msg), FLAGS_max_body_bytes);
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, entry));
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
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("req_op")>(static_cast<uint64_t>(entry.req.op));
  r.Append<r.ColIndex("req_body")>(std::move(entry.req.msg), FLAGS_max_body_bytes);
  r.Append<r.ColIndex("resp_op")>(static_cast<uint64_t>(entry.resp.op));
  r.Append<r.ColIndex("resp_body")>(std::move(entry.resp.msg), FLAGS_max_body_bytes);
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, entry));
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
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("req_header")>(entry.req.header);
  r.Append<r.ColIndex("req_body")>(entry.req.query);
  r.Append<r.ColIndex("resp_header")>(entry.resp.header);
  r.Append<r.ColIndex("resp_body")>(entry.resp.msg);
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, entry));
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
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("req")>(std::move(entry.req.payload));
  r.Append<r.ColIndex("resp")>(std::move(entry.resp.payload));
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
  r.Append<r.ColIndex("req_cmd")>(ToString(entry.req.tag, /* is_req */ true));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, entry));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::mux::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kMuxTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());
  r.Append<r.ColIndex("req_type")>(entry.req.type);
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, entry));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::amqp::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);
  int64_t timestamp_ns = std::max(entry.req.timestamp_ns, entry.resp.timestamp_ns);
  DataTable::RecordBuilder<&kAMQPTable> r(data_table, timestamp_ns);

  r.Append<r.ColIndex("time_")>(timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.role());

  size_t frame_type = std::max(entry.req.frame_type, entry.resp.frame_type);
  r.Append<r.ColIndex("frame_type")>(frame_type);
  r.Append<r.ColIndex("channel")>(entry.req.channel);
  r.Append<r.ColIndex("req_class_id")>(entry.req.class_id);
  r.Append<r.ColIndex("req_method_id")>(entry.req.method_id);

  r.Append<r.ColIndex("resp_class_id")>(entry.resp.class_id);
  r.Append<r.ColIndex("resp_method_id")>(entry.resp.method_id);

  r.Append<r.ColIndex("req_msg")>(entry.req.msg);
  r.Append<r.ColIndex("resp_msg")>(entry.resp.msg);
  r.Append<r.ColIndex("latency")>(
      AMQPCalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns, entry.req.synchronous,
                           entry.resp.synchronous));
}

namespace {

endpoint_role_t Swapendpoint_role_t(endpoint_role_t role) {
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

  endpoint_role_t role = conn_tracker.role();
  if (entry.role_swapped) {
    role = Swapendpoint_role_t(role);
  }

  DataTable::RecordBuilder<&kRedisTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(role);
  r.Append<r.ColIndex("req_cmd")>(std::string(entry.req.command));
  r.Append<r.ColIndex("req_args")>(std::string(entry.req.payload));
  r.Append<r.ColIndex("resp")>(std::string(entry.resp.payload));
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, entry));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::nats::Record record, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  endpoint_role_t role = conn_tracker.role();
  DataTable::RecordBuilder<&kNATSTable> r(data_table, record.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(record.req.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(role);
  r.Append<r.ColIndex("cmd")>(record.req.command);
  r.Append<r.ColIndex("body")>(record.req.options);
  r.Append<r.ColIndex("resp")>(record.resp.command);
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, record));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::kafka::Record record, DataTable* data_table) {
  constexpr size_t kMaxKafkaBodyBytes = 65536;

  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  endpoint_role_t role = conn_tracker.role();
  DataTable::RecordBuilder<&kKafkaTable> r(data_table, record.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(record.req.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(role);
  r.Append<r.ColIndex("req_cmd")>(static_cast<int64_t>(record.req.api_key));
  r.Append<r.ColIndex("client_id")>(std::move(record.req.client_id), FLAGS_max_body_bytes);
  r.Append<r.ColIndex("req_body")>(std::move(record.req.msg), kMaxKafkaBodyBytes);
  r.Append<r.ColIndex("resp")>(std::move(record.resp.msg), kMaxKafkaBodyBytes);
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(record.req.timestamp_ns, record.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, record));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx, const ConnTracker& conn_tracker,
                                         protocols::mongodb::Record record, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  endpoint_role_t role = conn_tracker.role();
  DataTable::RecordBuilder<&kMongoDBTable> r(data_table, record.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(record.req.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("local_addr")>(conn_tracker.local_endpoint().AddrStr());
  r.Append<r.ColIndex("local_port")>(conn_tracker.local_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(role);
  r.Append<r.ColIndex("req_cmd")>(std::move(record.req.op_msg_type));
  r.Append<r.ColIndex("req_body")>(std::move(record.req.frame_body));
  r.Append<r.ColIndex("resp_status")>(std::move(record.resp.op_msg_type));
  r.Append<r.ColIndex("resp_body")>(std::move(record.resp.frame_body));
  r.Append<r.ColIndex("latency")>(
      CalculateLatency(record.req.timestamp_ns, record.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(PXInfoString(conn_tracker, record));
#endif
}

void SocketTraceConnector::SetupOutput(const std::filesystem::path& path) {
  DCHECK(!path.empty());

  std::filesystem::path abs_path = std::filesystem::absolute(path);
  perf_buffer_events_output_stream_ = std::make_unique<std::ofstream>(abs_path);
  std::string format = "text";
  constexpr char kBinSuffix[] = ".bin";
  if (absl::EndsWith(FLAGS_socket_trace_data_events_output_path, kBinSuffix)) {
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
  pb->set_msg(std::string(event.msg));
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
  using TFrameType = typename TProtocolTraits::frame_type;
  using TKey = typename TProtocolTraits::key_type;

  VLOG(3) << absl::StrCat("Connection\n", DebugString<TProtocolTraits>(*tracker, ""));

  // Make sure the tracker's frames containers have been properly initialized.
  // This is a nop if the containers are already of the right type.
  tracker->InitFrames<TKey, TFrameType>();

  if (data_table != nullptr && tracker->state() == ConnTracker::State::kTransferring) {
    // ProcessToRecords() parses raw events and produces messages in format that are expected by
    // table store. But those messages are not cached inside ConnTracker.
    auto records = tracker->ProcessToRecords<TProtocolTraits>();
    for (auto& record : records) {
      TProtocolTraits::ConvertTimestamps(
          &record, [&](uint64_t mono_time) { return ConvertToRealTime(mono_time); });
      AppendMessage(ctx, *tracker, std::move(record), data_table);
    }
  }

  auto buffer_expiry_timestamp =
      iteration_time() - std::chrono::seconds(FLAGS_datastream_buffer_expiry_duration_secs);
  auto message_expiry_timestamp =
      iteration_time() - std::chrono::seconds(FLAGS_messages_expiry_duration_secs);

  tracker->Cleanup<TProtocolTraits>(FLAGS_messages_size_limit_bytes,
                                    FLAGS_datastream_buffer_retention_size,
                                    message_expiry_timestamp, buffer_expiry_timestamp);
}

void SocketTraceConnector::TransferConnStats(ConnectorContext* ctx, DataTable* data_table) {
  namespace idx = ::px::stirling::conn_stats_idx;

  absl::flat_hash_set<md::UPID> upids = ctx->GetUPIDs();
  uint64_t time = AdjustedSteadyClockNowNS();

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
      // TODO(@benkilimnik: uncomment when we have local addr/port in the key)
      // r.Append<idx::kLocalAddr>(key.local_addr);
      // r.Append<idx::kLocalPort>(key.local_port);
      r.Append<idx::kAddrFamily>(static_cast<int>(stats.addr_family));
      r.Append<idx::kProtocol>(stats.protocol);
      r.Append<idx::kRole>(stats.role);
      r.Append<idx::kSSL>(stats.ssl);
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
      const auto proc_pid_path = ProcPidPath(key.upid.pid);
      if (!fs::Exists(proc_pid_path)) {
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
