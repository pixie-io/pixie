#ifdef __linux__

#include "src/stirling/socket_trace_connector.h"

#include <sys/types.h>
#include <unistd.h>

#include <deque>
#include <filesystem>
#include <utility>

#include <absl/strings/match.h>
#include <google/protobuf/empty.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/common/grpcutils/utils.h"
#include "src/common/json/json.h"
#include "src/common/protobufs/recordio.h"
#include "src/common/system/socket_info.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/bcc_bpf_interface/socket_trace.h"
#include "src/stirling/bcc_bpf_interface/symaddrs.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/common/go_grpc_types.h"
#include "src/stirling/connection_stats.h"
#include "src/stirling/obj_tools/dwarf_tools.h"
#include "src/stirling/proto/sock_event.pb.h"
#include "src/stirling/protocols/common/event_parser.h"
#include "src/stirling/protocols/http2/grpc.h"
#include "src/stirling/uprobe_symaddrs.h"
#include "src/stirling/utils/linux_headers.h"
#include "src/stirling/utils/proc_path_tools.h"

DEFINE_bool(stirling_enable_parsing_protobufs, false,
            "If true, parses binary protobufs captured in gRPC messages. "
            "As of 2019-07, the parser can only handle protobufs defined in Hipster Shop.");
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
DEFINE_bool(stirling_enable_grpc_tracing, true,
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
DEFINE_string(stirling_role_to_trace, "kRoleAll",
              "Must be one of [kRoleClient|kRoleServer|kRoleAll]. Specifies which role(s) will be "
              "traced by BPF.");

BPF_SRC_STRVIEW(socket_trace_bcc_script, socket_trace);

namespace pl {
namespace stirling {

using ::google::protobuf::TextFormat;
using ::pl::grpc::MethodInputOutput;
using ::pl::stirling::kCQLTable;
using ::pl::stirling::kHTTPTable;
using ::pl::stirling::kMySQLTable;
using ::pl::stirling::grpc::ParsePB;
using ::pl::stirling::obj_tools::DwarfReader;
using ::pl::stirling::obj_tools::ElfReader;
using ::pl::utils::ToJSONString;

SocketTraceConnector::SocketTraceConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables), bpf_tools::BCCWrapper() {
  proc_parser_ = std::make_unique<system::ProcParser>(system::Config::GetInstance());
  InitProtocolTransferSpecs();
}

void SocketTraceConnector::InitProtocolTransferSpecs() {
#define TRANSER_STREAM_PROTOCOL(protocol_name) \
  &SocketTraceConnector::TransferStream<protocols::protocol_name::ProtocolTraits>
  // PROTOCOL_LIST: Requires update on new protocols.
  protocol_transfer_specs_ = {
      {kProtocolHTTP,
       {kHTTPTableNum, TRANSER_STREAM_PROTOCOL(http), FLAGS_stirling_enable_http_tracing}},
      {kProtocolHTTP2,
       {kHTTPTableNum, TRANSER_STREAM_PROTOCOL(http2), FLAGS_stirling_enable_grpc_tracing}},
      {kProtocolMySQL,
       {kMySQLTableNum, TRANSER_STREAM_PROTOCOL(mysql), FLAGS_stirling_enable_mysql_tracing}},
      {kProtocolCQL,
       {kCQLTableNum, TRANSER_STREAM_PROTOCOL(cass), FLAGS_stirling_enable_cass_tracing}},
      {kProtocolPGSQL,
       {kPGSQLTableNum, TRANSER_STREAM_PROTOCOL(pgsql), FLAGS_stirling_enable_pgsql_tracing}},
      {kProtocolDNS,
       {kDNSTableNum, TRANSER_STREAM_PROTOCOL(dns), FLAGS_stirling_enable_dns_tracing}},
      {kProtocolRedis,
       {kRedisTableNum, TRANSER_STREAM_PROTOCOL(redis), FLAGS_stirling_enable_redis_tracing}},
      // Unknown protocols attached to HTTP table so that they run their cleanup functions,
      // but the use of nullptr transfer_fn means it won't actually transfer data to the HTTP table.
      {kProtocolUnknown, {kHTTPTableNum, nullptr}},
  };
#undef TRANSER_STREAM_PROTOCOL

  // Populate `role_to_trace` from flags.
  std::optional<EndpointRole> role_to_trace =
      magic_enum::enum_cast<EndpointRole>(FLAGS_stirling_role_to_trace);
  if (!role_to_trace.has_value()) {
    LOG(ERROR) << absl::Substitute(
        "--stirling_role_to_trace=$0 is not a valid trace role specifier",
        FLAGS_stirling_role_to_trace);
  }

  for (const auto& p : TrafficProtocolEnumValues()) {
    DCHECK(protocol_transfer_specs_.find(p) != protocol_transfer_specs_.end())
        << "Missing transfer spec for protocol: " << magic_enum::enum_name(p);
    if (role_to_trace.has_value()) {
      protocol_transfer_specs_[p].role_to_trace = role_to_trace.value();
    }
  }
}

Status SocketTraceConnector::InitImpl() {
  constexpr uint64_t kNanosPerSecond = 1000 * 1000 * 1000;
  if (kNanosPerSecond % sysconfig_.KernelTicksPerSecond() != 0) {
    return error::Internal(
        "SC_CLK_TCK aka USER_HZ must be 100, otherwise our BPF code may not generate proper "
        "timestamps in a way that matches how /proc/stat does it");
  }

  PL_ASSIGN_OR_RETURN(utils::KernelVersion kernel_version, utils::GetKernelVersion());
  std::string linux_header_macro =
      absl::Substitute("-DLINUX_VERSION_CODE=$0", kernel_version.code());

  PL_RETURN_IF_ERROR(InitBPFProgram(socket_trace_bcc_script, {std::move(linux_header_macro)}));
  PL_RETURN_IF_ERROR(AttachKProbes(kProbeSpecs));
  LOG(INFO) << absl::Substitute("Number of kprobes deployed = $0", kProbeSpecs.size());
  LOG(INFO) << "Probes successfully deployed.";

  PL_RETURN_IF_ERROR(OpenPerfBuffers(kPerfBufferSpecs, this));
  LOG(INFO) << absl::Substitute("Number of perf buffers opened = $0", kPerfBufferSpecs.size());

  // Set trace role to BPF probes.
  for (const auto& p : TrafficProtocolEnumValues()) {
    if (protocol_transfer_specs_[p].enabled) {
      PL_RETURN_IF_ERROR(UpdateBPFProtocolTraceRole(p, protocol_transfer_specs_[p].role_to_trace));
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

  conn_info_map_mgr_ = std::make_shared<ConnInfoMapManager>(&bpf());
  ConnectionTracker::SetConnInfoMapManager(conn_info_map_mgr_);

  go_common_symaddrs_map_ =
      std::make_unique<ebpf::BPFHashTable<uint32_t, struct go_common_symaddrs_t>>(
          bpf().get_hash_table<uint32_t, struct go_common_symaddrs_t>("go_common_symaddrs_map"));
  http2_symaddrs_map_ = std::make_unique<ebpf::BPFHashTable<uint32_t, struct go_http2_symaddrs_t>>(
      bpf().get_hash_table<uint32_t, struct go_http2_symaddrs_t>("http2_symaddrs_map"));
  go_tls_symaddrs_map_ = std::make_unique<ebpf::BPFHashTable<uint32_t, struct go_tls_symaddrs_t>>(
      bpf().get_hash_table<uint32_t, struct go_tls_symaddrs_t>("go_tls_symaddrs_map"));

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
  while (num_deploy_uprobes_threads_ != 0) {
  }

  // Must call Stop() after attach_uprobes_thread_ has joined,
  // otherwise the two threads will cause concurrent accesses to BCC,
  // that will cause races and undefined behavior.
  bpf_tools::BCCWrapper::Stop();
  return Status::OK();
}

void SocketTraceConnector::UpdateCommonState(ConnectorContext* ctx) {
  // Since events may be pushed into the perf buffer while reading it,
  // we establish a cutoff time before draining the perf buffer.
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
}

void SocketTraceConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                            DataTable* data_table) {
  DCHECK_LT(table_num, kTables.size())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);
  DCHECK(data_table != nullptr);

  UpdateCommonState(ctx);

  if (table_num == kConnStatsTableNum) {
    // Connection stats table does not follow the convention of tables for data streams.
    // So we handle it separately.
    TransferConnectionStats(ctx, data_table);
  } else {
    data_table->SetConsumeRecordsCutoffTime(perf_buffer_drain_time_);

    TransferStreams(ctx, table_num, data_table);
  }
}

template <typename TValueType>
Status UpdatePerCPUArrayValue(int idx, TValueType val, ebpf::BPFPercpuArrayTable<TValueType>* arr) {
  std::vector<TValueType> values(bpf_tools::BCCWrapper::kCPUCount, val);
  auto update_res = arr->update_value(idx, values);
  if (update_res.code() != 0) {
    return error::Internal(absl::Substitute("Failed to set value on index: $0, error message: $1",
                                            idx, update_res.msg()));
  }
  return Status::OK();
}

Status SocketTraceConnector::UpdateBPFProtocolTraceRole(TrafficProtocol protocol,
                                                        EndpointRole config_mask) {
  auto control_map_handle = bpf().get_percpu_array_table<uint64_t>(kControlMapName);
  return UpdatePerCPUArrayValue(static_cast<int>(protocol), static_cast<uint64_t>(config_mask),
                                &control_map_handle);
}

Status SocketTraceConnector::TestOnlySetTargetPID(int64_t pid) {
  auto control_map_handle = bpf().get_percpu_array_table<int64_t>(kControlValuesArrayName);
  return UpdatePerCPUArrayValue(kTargetTGIDIndex, pid, &control_map_handle);
}

Status SocketTraceConnector::DisableSelfTracing() {
  auto control_map_handle = bpf().get_percpu_array_table<int64_t>(kControlValuesArrayName);
  int64_t self_pid = getpid();
  return UpdatePerCPUArrayValue(kStirlingTGIDIndex, self_pid, &control_map_handle);
}

//-----------------------------------------------------------------------------
// Uprobe Management Functions
//-----------------------------------------------------------------------------

StatusOr<int> SocketTraceConnector::AttachUProbeTmpl(const ArrayView<UProbeTmpl>& probe_tmpls,
                                                     const std::string& binary,
                                                     obj_tools::ElfReader* elf_reader) {
  using bpf_tools::BPFProbeAttachType;

  int uprobe_count = 0;
  for (const auto& tmpl : probe_tmpls) {
    bpf_tools::UProbeSpec spec = {binary,
                                  {},
                                  0,
                                  bpf_tools::UProbeSpec::kDefaultPID,
                                  tmpl.attach_type,
                                  std::string(tmpl.probe_fn)};

    StatusOr<std::vector<ElfReader::SymbolInfo>> symbol_infos_status =
        elf_reader->ListFuncSymbols(tmpl.symbol, tmpl.match_type);
    if (!symbol_infos_status.ok()) {
      VLOG(1) << absl::Substitute("Could not list symbols [error=$0]",
                                  symbol_infos_status.ToString());
      continue;
    }
    const std::vector<ElfReader::SymbolInfo>& symbol_infos = symbol_infos_status.ValueOrDie();

    for (const auto& symbol_info : symbol_infos) {
      switch (tmpl.attach_type) {
        case BPFProbeAttachType::kEntry:
        case BPFProbeAttachType::kReturn: {
          spec.symbol = symbol_info.name;
          PL_RETURN_IF_ERROR(AttachUProbe(spec));
          ++uprobe_count;
          break;
        }
        case BPFProbeAttachType::kReturnInsts: {
          // TODO(yzhao): The following code that produces multiple UProbeSpec objects cannot be
          // replaced by TransformGolangReturnProbe(), because LLVM and ELFIO defines conflicting
          // symbol: EI_MAG0 appears as enum in include/llvm/BinaryFormat/ELF.h [1] and
          // EI_MAG0 appears as a macro in elfio/elf_types.hpp [2]. And there are many other such
          // symbols as well.
          //
          // [1] https://llvm.org/doxygen/BinaryFormat_2ELF_8h_source.html
          // [2] https://github.com/eth-sri/debin/blob/master/cpp/elfio/elf_types.hpp
          PL_ASSIGN_OR_RETURN(std::vector<uint64_t> ret_inst_addrs,
                              elf_reader->FuncRetInstAddrs(symbol_info));
          for (const uint64_t& addr : ret_inst_addrs) {
            spec.attach_type = BPFProbeAttachType::kEntry;
            spec.address = addr;
            PL_RETURN_IF_ERROR(AttachUProbe(spec));
            ++uprobe_count;
          }
          break;
        }
        default:
          LOG(DFATAL) << "Invalid attach type in switch statement.";
      }
    }
  }
  return uprobe_count;
}

namespace {
Status UpdateGoCommonSymAddrs(
    ElfReader* elf_reader, DwarfReader* dwarf_reader, const std::vector<int32_t>& pids,
    ebpf::BPFHashTable<uint32_t, struct go_common_symaddrs_t>* go_common_symaddrs_map) {
  PL_ASSIGN_OR_RETURN(struct go_common_symaddrs_t symaddrs,
                      GoCommonSymAddrs(elf_reader, dwarf_reader));

  for (auto& pid : pids) {
    ebpf::StatusTuple s = go_common_symaddrs_map->update_value(pid, symaddrs);
    LOG_IF(WARNING, s.code() != 0)
        << absl::StrCat("Could not update go_common_symaddrs_map. Message=", s.msg());
  }

  return Status::OK();
}

Status UpdateGoHTTP2SymAddrs(
    ElfReader* elf_reader, DwarfReader* dwarf_reader, const std::vector<int32_t>& pids,
    ebpf::BPFHashTable<uint32_t, struct go_http2_symaddrs_t>* http2_symaddrs_map) {
  PL_ASSIGN_OR_RETURN(struct go_http2_symaddrs_t symaddrs,
                      GoHTTP2SymAddrs(elf_reader, dwarf_reader));

  for (auto& pid : pids) {
    ebpf::StatusTuple s = http2_symaddrs_map->update_value(pid, symaddrs);
    LOG_IF(WARNING, s.code() != 0)
        << absl::StrCat("Could not update http2_symaddrs_map. Message=", s.msg());
  }

  return Status::OK();
}

Status UpdateGoTLSSymAddrs(
    ElfReader* elf_reader, DwarfReader* dwarf_reader, const std::vector<int32_t>& pids,
    ebpf::BPFHashTable<uint32_t, struct go_tls_symaddrs_t>* go_tls_symaddrs_map) {
  PL_ASSIGN_OR_RETURN(struct go_tls_symaddrs_t symaddrs, GoTLSSymAddrs(elf_reader, dwarf_reader));

  for (auto& pid : pids) {
    ebpf::StatusTuple s = go_tls_symaddrs_map->update_value(pid, symaddrs);
    LOG_IF(WARNING, s.code() != 0)
        << absl::StrCat("Could not update go_tls_symaddrs_map. Message=", s.msg());
  }

  return Status::OK();
}
}  // namespace

// TODO(oazizi/yzhao): Should HTTP uprobes use a different set of perf buffers than the kprobes?
// That allows the BPF code and companion user-space code for uprobe & kprobe be separated
// cleanly. For example, right now, enabling uprobe & kprobe simultaneously can crash Stirling,
// because of the mixed & duplicate data events from these 2 sources.
StatusOr<int> SocketTraceConnector::AttachHTTP2Probes(
    const std::string& binary, obj_tools::ElfReader* elf_reader,
    obj_tools::DwarfReader* dwarf_reader, const std::vector<int32_t>& new_pids,
    ebpf::BPFHashTable<uint32_t, struct go_http2_symaddrs_t>* http2_symaddrs_map) {
  // Step 1: Update BPF symaddrs for this binary.
  Status s = UpdateGoHTTP2SymAddrs(elf_reader, dwarf_reader, new_pids, http2_symaddrs_map);
  if (!s.ok()) {
    return 0;
  }

  // Step 2: Deploy uprobes on all new binaries.
  auto result = http2_probed_binaries_.insert(binary);
  if (!result.second) {
    // This is not a new binary, so nothing more to do.
    return 0;
  }
  return AttachUProbeTmpl(kHTTP2ProbeTmpls, binary, elf_reader);
}

StatusOr<int> SocketTraceConnector::AttachOpenSSLUProbes(const std::string& binary,
                                                         const std::vector<int32_t>& new_pids) {
  constexpr std::string_view kLibSSL = "libssl.so.1.1";

  // Only search for OpenSSL libraries on newly discovered binaries.
  // TODO(oazizi): Will this prevent us from discovering dynamically loaded OpenSSL instances?
  auto result = openssl_probed_binaries_.insert(binary);
  if (!result.second) {
    return 0;
  }

  const system::Config& sysconfig = system::Config::GetInstance();
  std::filesystem::path container_lib;

  PL_ASSIGN_OR_RETURN(std::unique_ptr<FilePathResolver> fp_resolver, FilePathResolver::Create());

  // Find the path to libssl for this binary, which may be inside a container.
  for (const auto& pid : new_pids) {
    StatusOr<absl::flat_hash_set<std::string>> libs_status = proc_parser_->GetMapPaths(pid);
    if (!libs_status.ok()) {
      VLOG(1) << absl::Substitute("Unable to check for libssl.so for $0. Message: $1", binary,
                                  libs_status.msg());
      continue;
    }

    Status s = fp_resolver->SetMountNamespace(pid);
    if (!s.ok()) {
      VLOG(1) << absl::Substitute("Could not set pid namespace. Did the pid terminate?");
      continue;
    }

    for (const auto& lib : libs_status.ValueOrDie()) {
      if (absl::EndsWith(lib, kLibSSL)) {
        StatusOr<std::filesystem::path> container_lib_status = fp_resolver->ResolvePath(lib);

        if (!container_lib_status.ok()) {
          VLOG(1) << absl::Substitute("Unable to resolve libssl.so path for $0. Message: $1",
                                      binary, container_lib_status.msg());
          continue;
        }
        container_lib = container_lib_status.ValueOrDie();
        break;
      }
    }
  }

  if (container_lib.empty()) {
    // Looks like this binary doesn't use libssl (or we ran into an error).
    return 0;
  }

  // Convert to host path, in case we're running inside a container ourselves.
  container_lib = sysconfig.ToHostPath(container_lib);
  PL_RETURN_IF_ERROR(fs::Exists(container_lib));

  // Only try probing .so files that we haven't already set probes on.
  result = openssl_probed_binaries_.insert(container_lib);
  if (!result.second) {
    return 0;
  }

  for (auto spec : kOpenSSLUProbes) {
    spec.binary_path = container_lib.string();
    PL_RETURN_IF_ERROR(AttachUProbe(spec));
  }
  return kOpenSSLUProbes.size();
}

StatusOr<int> SocketTraceConnector::AttachGoTLSUProbes(
    const std::string& binary, obj_tools::ElfReader* elf_reader,
    obj_tools::DwarfReader* dwarf_reader, const std::vector<int32_t>& new_pids,
    ebpf::BPFHashTable<uint32_t, struct go_tls_symaddrs_t>* go_tls_symaddrs_map) {
  // Step 1: Update BPF symbols_map on all new PIDs.
  Status s = UpdateGoTLSSymAddrs(elf_reader, dwarf_reader, new_pids, go_tls_symaddrs_map);
  if (!s.ok()) {
    // Doesn't appear to be a binary with the mandatory symbols.
    // Might not even be a golang binary.
    // Either way, not of interest to probe.
    return 0;
  }

  // Step 2: Deploy uprobes on all new binaries.
  auto result = go_tls_probed_binaries_.insert(binary);
  if (!result.second) {
    // This is not a new binary, so nothing more to do.
    return 0;
  }
  return AttachUProbeTmpl(kGoTLSUProbeTmpls, binary, elf_reader);
}

namespace {

// Convert PID list from list of UPIDs to a map with key=binary name, value=PIDs
std::map<std::string, std::vector<int32_t>> ConvertPIDsListToMap(
    const absl::flat_hash_set<md::UPID>& upids) {
  const system::Config& sysconfig = system::Config::GetInstance();

  // Convert to a map of binaries, with the upids that are instances of that binary.
  std::map<std::string, std::vector<int32_t>> new_pids;

  PL_ASSIGN_OR(std::unique_ptr<FilePathResolver> fp_resolver, FilePathResolver::Create(),
               return {});

  // Consider new UPIDs only.
  for (const auto& upid : upids) {
    PL_ASSIGN_OR(std::filesystem::path proc_exe, ProcExe(upid.pid()), continue);

    Status s = fp_resolver->SetMountNamespace(upid.pid());
    if (!s.ok()) {
      VLOG(1) << absl::Substitute("Could not set pid namespace. Did the pid terminate?");
      continue;
    }

    PL_ASSIGN_OR(std::filesystem::path exe_path, fp_resolver->ResolvePath(proc_exe), continue);

    std::filesystem::path host_exe_path = sysconfig.ToHostPath(exe_path);
    if (!fs::Exists(host_exe_path).ok()) {
      continue;
    }
    new_pids[host_exe_path.string()].push_back(upid.pid());
  }

  LOG_FIRST_N(INFO, 1) << absl::Substitute("New PIDs count = $0", new_pids.size());

  return new_pids;
}

}  // namespace

std::thread SocketTraceConnector::RunDeployUProbesThread(
    const absl::flat_hash_set<md::UPID>& pids) {
  proc_tracker_.Update(pids);
  auto& new_upids = proc_tracker_.new_upids();
  // The check that state is not uninitialized is required for socket_trace_connector_test,
  // which would otherwise try to deploy uprobes (for which it does not have permissions).
  if (!new_upids.empty() && state() != State::kUninitialized) {
    // Increment before starting thread to avoid race in case thread starts late.
    ++num_deploy_uprobes_threads_;
    return std::thread([this, new_upids]() {
      DeployUProbes(new_upids);
      --num_deploy_uprobes_threads_;
    });
  }
  return {};
}

void SocketTraceConnector::DeployUProbes(const absl::flat_hash_set<md::UPID>& pids) {
  const std::lock_guard<std::mutex> lock(deploy_uprobes_mutex_);

  // Before deploying new probes, clean-up map entries for old processes that are now dead.
  for (const auto& pid : proc_tracker_.deleted_upids()) {
    ebpf::StatusTuple s = http2_symaddrs_map_->remove_value(pid.pid());
    VLOG_IF(1, s.code() != 0) << absl::StrCat(
        "Could not remove entry from http2_symaddrs_map. Message=", s.msg());
  }

  int uprobe_count = 0;
  for (auto& [binary, pid_vec] : ConvertPIDsListToMap(pids)) {
    // OpenSSL Probes.
    {
      StatusOr<int> attach_status = AttachOpenSSLUProbes(binary, pid_vec);
      if (!attach_status.ok()) {
        LOG_FIRST_N(WARNING, 10) << absl::Substitute("Failed to attach SSL Uprobes to $0: $1",
                                                     binary, attach_status.ToString());
      } else {
        uprobe_count += attach_status.ValueOrDie();
      }
    }

    // Read binary's symbols.
    StatusOr<std::unique_ptr<ElfReader>> elf_reader_status = ElfReader::Create(binary);
    if (!elf_reader_status.ok()) {
      LOG(WARNING) << absl::Substitute(
          "Cannot analyze binary $0 for uprobe deployment. "
          "If file is under /var/lib, container may have terminated. "
          "Message = $1",
          binary, elf_reader_status.msg());
      continue;
    }
    std::unique_ptr<ElfReader> elf_reader = elf_reader_status.ConsumeValueOrDie();

    StatusOr<std::unique_ptr<DwarfReader>> dwarf_reader_status = DwarfReader::Create(binary);
    if (!dwarf_reader_status.ok()) {
      VLOG(1) << absl::Substitute(
          "Failed to get binary $0 debug symbols. Cannot deploy uprobes. "
          "Message = $1",
          binary, dwarf_reader_status.msg());
      continue;
    }
    std::unique_ptr<DwarfReader> dwarf_reader = dwarf_reader_status.ConsumeValueOrDie();

    Status s = UpdateGoCommonSymAddrs(elf_reader.get(), dwarf_reader.get(), pid_vec,
                                      go_common_symaddrs_map_.get());
    if (!s.ok()) {
      // Doesn't appear to be a binary with the mandatory symbols (e.g. TCPConn).
      // Might not even be a golang binary.
      // Either way, not of interest to probe.
      continue;
    }

    // GoTLS Probes.
    {
      StatusOr<int> attach_status = AttachGoTLSUProbes(binary, elf_reader.get(), dwarf_reader.get(),
                                                       pid_vec, go_tls_symaddrs_map_.get());
      if (!attach_status.ok()) {
        LOG_FIRST_N(WARNING, 10) << absl::Substitute("Failed to attach GoTLS Uprobes to $0: $1",
                                                     binary, attach_status.ToString());
      } else {
        uprobe_count += attach_status.ValueOrDie();
      }
    }

    // HTTP2 Probes.
    if (protocol_transfer_specs_[kProtocolHTTP2].enabled) {
      StatusOr<int> attach_status = AttachHTTP2Probes(binary, elf_reader.get(), dwarf_reader.get(),
                                                      pid_vec, http2_symaddrs_map_.get());
      if (!attach_status.ok()) {
        LOG_FIRST_N(WARNING, 10) << absl::Substitute("Failed to attach HTTP2 Uprobes to $0: $1",
                                                     binary, attach_status.ToString());
      } else {
        uprobe_count += attach_status.ValueOrDie();
      }
    }

    // Add other uprobes here.
  }

  LOG_FIRST_N(INFO, 1) << absl::Substitute("Number of uprobes deployed = $0", uprobe_count);
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

namespace {

std::string ProbeLossMessage(std::string_view perf_buffer_name, uint64_t lost) {
  return absl::Substitute("$0 lost $1 samples.", perf_buffer_name, lost);
}

}  // namespace

void SocketTraceConnector::HandleDataEventsLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << ProbeLossMessage("socket_data_events", lost);
}

void SocketTraceConnector::HandleControlEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  connector->AcceptControlEvent(*static_cast<const socket_control_event_t*>(data));
}

void SocketTraceConnector::HandleControlEventsLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << ProbeLossMessage("socket_control_events", lost);
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

void SocketTraceConnector::HandleHTTP2HeaderEventLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << ProbeLossMessage("go_grpc_header_events", lost);
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

void SocketTraceConnector::HandleHTTP2DataLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << ProbeLossMessage("go_grpc_data_events", lost);
}

//-----------------------------------------------------------------------------
// Connection Tracker Events
//-----------------------------------------------------------------------------

namespace {

uint64_t GetConnMapKey(uint32_t pid, uint32_t fd) {
  return (static_cast<uint64_t>(pid) << 32) | fd;
}

void SocketDataEventToPB(const SocketDataEvent& event, sockeventpb::SocketDataEvent* pb) {
  pb->mutable_attr()->set_timestamp_ns(event.attr.timestamp_ns);
  pb->mutable_attr()->mutable_conn_id()->set_pid(event.attr.conn_id.upid.pid);
  pb->mutable_attr()->mutable_conn_id()->set_start_time_ns(
      event.attr.conn_id.upid.start_time_ticks);
  pb->mutable_attr()->mutable_conn_id()->set_fd(event.attr.conn_id.fd);
  pb->mutable_attr()->mutable_conn_id()->set_generation(event.attr.conn_id.tsid);
  pb->mutable_attr()->mutable_traffic_class()->set_protocol(event.attr.traffic_class.protocol);
  pb->mutable_attr()->mutable_traffic_class()->set_role(event.attr.traffic_class.role);
  pb->mutable_attr()->set_direction(event.attr.direction);
  pb->mutable_attr()->set_pos(event.attr.pos);
  pb->mutable_attr()->set_msg_size(event.attr.msg_size);
  pb->set_msg(event.msg);
}

}  // namespace

void SocketTraceConnector::AcceptDataEvent(std::unique_ptr<SocketDataEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();

  if (perf_buffer_events_output_stream_ != nullptr) {
    WriteDataEvent(*event);
  }

  ConnectionTracker& tracker = GetMutableConnTracker(event->attr.conn_id);

  tracker.AddDataEvent(std::move(event));
}

void SocketTraceConnector::AcceptControlEvent(socket_control_event_t event) {
  // timestamp_ns is a common field of open and close fields.
  event.open.timestamp_ns += ClockRealTimeOffset();

  // conn_id is a common field of open & close.
  ConnectionTracker& tracker = GetMutableConnTracker(event.open.conn_id);

  tracker.AddControlEvent(event);
}

void SocketTraceConnector::AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();

  ConnectionTracker& tracker = GetMutableConnTracker(event->attr.conn_id);

  tracker.AddHTTP2Header(std::move(event));
}

void SocketTraceConnector::AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();

  ConnectionTracker& tracker = GetMutableConnTracker(event->attr.conn_id);

  tracker.AddHTTP2Data(std::move(event));
}

ConnectionTracker& SocketTraceConnector::GetMutableConnTracker(struct conn_id_t conn_id) {
  const uint64_t conn_map_key = GetConnMapKey(conn_id.upid.pid, conn_id.fd);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";

  auto& conn_trackers = connection_trackers_[conn_map_key];
  ConnectionTracker& conn_tracker = conn_trackers[conn_id.tsid];

  bool new_tracker = (conn_tracker.conn_id().tsid == 0);
  if (new_tracker) {
    conn_tracker.set_conn_stats(&connection_stats_);

    // If there is a another generation for this conn map key,
    // one of them needs to be marked for death.
    if (conn_trackers.size() > 1) {
      auto last_tracker_iter = --conn_trackers.end();

      // If the inserted conn_tracker is not the last generation, then mark it for death.
      // This can happen because the events draining from the perf buffers are not ordered.
      if (last_tracker_iter->second.conn_id().tsid != 0) {
        VLOG(1) << "Marking for death because not last generation.";
        conn_tracker.MarkForDeath();
      } else {
        // New tracker was the last, so the previous last should be marked for death.
        --last_tracker_iter;
        VLOG(1) << "Marking previous generation for death.";
        last_tracker_iter->second.MarkForDeath();
      }
    }
  }

  return conn_tracker;
}

const ConnectionTracker* SocketTraceConnector::GetConnectionTracker(uint32_t pid,
                                                                    uint32_t fd) const {
  const uint64_t conn_map_key = GetConnMapKey(pid, fd);

  auto tracker_set_it = connection_trackers_.find(conn_map_key);
  if (tracker_set_it == connection_trackers_.end()) {
    return nullptr;
  }

  const auto& tracker_generations = tracker_set_it->second;

  if (tracker_generations.empty()) {
    DCHECK(false);
    return nullptr;
  }

  // Return last connection.
  auto tracker_it = tracker_generations.end();
  --tracker_it;
  return &tracker_it->second;
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
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
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
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("http_major_version")>(1);
  r.Append<r.ColIndex("http_minor_version")>(resp_message.minor_version);
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(content_type));
  r.Append<r.ColIndex("http_req_headers"), kMaxHTTPHeadersBytes>(ToJSONString(req_message.headers));
  r.Append<r.ColIndex("http_req_method")>(std::move(req_message.req_method));
  r.Append<r.ColIndex("http_req_path")>(std::move(req_message.req_path));
  r.Append<r.ColIndex("http_req_body_size")>(req_message.body.size());
  r.Append<r.ColIndex("http_req_body"), kMaxBodyBytes>(std::move(req_message.body));
  r.Append<r.ColIndex("http_resp_headers"), kMaxHTTPHeadersBytes>(
      ToJSONString(resp_message.headers));
  r.Append<r.ColIndex("http_resp_status")>(resp_message.resp_status);
  r.Append<r.ColIndex("http_resp_message")>(std::move(resp_message.resp_message));
  r.Append<r.ColIndex("http_resp_body_size")>(resp_message.body.size());
  r.Append<r.ColIndex("http_resp_body"), kMaxBodyBytes>(std::move(resp_message.body));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_message.timestamp_ns, resp_message.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(std::move(record.px_info));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::http2::Record record, DataTable* data_table) {
  protocols::http2::HalfStream* req_stream;
  protocols::http2::HalfStream* resp_stream;

  // Depending on whether the traced entity was the requestor or responder,
  // we need to flip the interpretation of the half-streams.
  if (conn_tracker.traffic_class().role == kRoleClient) {
    req_stream = &record.send;
    resp_stream = &record.recv;
  } else {
    req_stream = &record.recv;
    resp_stream = &record.send;
  }

  // TODO(oazizi): Status should be in the trailers, not headers. But for now it is found in
  // headers. Fix when this changes.
  int64_t resp_status;
  ECHECK(absl::SimpleAtoi(resp_stream->headers.ValueByKey(":status", "-1"), &resp_status));

  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  std::string path = req_stream->headers.ValueByKey(protocols::http2::headers::kPath);

  if (FLAGS_stirling_enable_parsing_protobufs &&
      (req_stream->HasGRPCContentType() || resp_stream->HasGRPCContentType())) {
    MethodInputOutput rpc = grpc_desc_db_.GetMethodInputOutput(::pl::grpc::MethodPath(path));
    req_stream->data = ParsePB(req_stream->data, rpc.input.get());
    resp_stream->data = ParsePB(resp_stream->data, rpc.output.get());
  }

  DataTable::RecordBuilder<&kHTTPTable> r(data_table, resp_stream->timestamp_ns);
  r.Append<r.ColIndex("time_")>(resp_stream->timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("http_major_version")>(2);
  // HTTP2 does not define minor version.
  r.Append<r.ColIndex("http_minor_version")>(0);
  r.Append<r.ColIndex("http_req_headers"), kMaxHTTPHeadersBytes>(ToJSONString(req_stream->headers));
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(HTTPContentType::kGRPC));
  r.Append<r.ColIndex("http_resp_headers"), kMaxHTTPHeadersBytes>(
      ToJSONString(resp_stream->headers));
  r.Append<r.ColIndex("http_req_method")>(
      req_stream->headers.ValueByKey(protocols::http2::headers::kMethod));
  r.Append<r.ColIndex("http_req_path")>(req_stream->headers.ValueByKey(":path"));
  r.Append<r.ColIndex("http_resp_status")>(resp_status);
  // TODO(yzhao): Populate the following field from headers.
  r.Append<r.ColIndex("http_resp_message")>("OK");
  r.Append<r.ColIndex("http_req_body_size")>(req_stream->data.size());
  r.Append<r.ColIndex("http_req_body"), kMaxBodyBytes>(std::move(req_stream->data));
  r.Append<r.ColIndex("http_resp_body_size")>(resp_stream->data.size());
  r.Append<r.ColIndex("http_resp_body"), kMaxBodyBytes>(std::move(resp_stream->data));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_stream->timestamp_ns, resp_stream->timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>("");
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::mysql::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kMySQLTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("req_cmd")>(static_cast<uint64_t>(entry.req.cmd));
  r.Append<r.ColIndex("req_body"), kMaxBodyBytes>(std::move(entry.req.msg));
  r.Append<r.ColIndex("resp_status")>(static_cast<uint64_t>(entry.resp.status));
  r.Append<r.ColIndex("resp_body"), kMaxBodyBytes>(std::move(entry.resp.msg));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(std::move(entry.px_info));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::cass::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kCQLTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("req_op")>(static_cast<uint64_t>(entry.req.op));
  r.Append<r.ColIndex("req_body"), kMaxBodyBytes>(std::move(entry.req.msg));
  r.Append<r.ColIndex("resp_op")>(static_cast<uint64_t>(entry.resp.op));
  r.Append<r.ColIndex("resp_body"), kMaxBodyBytes>(std::move(entry.resp.msg));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>("");
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::dns::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kDNSTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("req_header")>(entry.req.header);
  r.Append<r.ColIndex("req_body")>(entry.req.query);
  r.Append<r.ColIndex("resp_header")>(entry.resp.header);
  r.Append<r.ColIndex("resp_body")>(entry.resp.msg);
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>("");
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::pgsql::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kPGSQLTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("req")>(std::move(entry.req.payload));
  r.Append<r.ColIndex("resp")>(std::move(entry.resp.payload));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>("");
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::redis::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kRedisTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("req")>(std::string(entry.req.payload));
  r.Append<r.ColIndex("resp")>(std::string(entry.resp.payload));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>("");
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

void SocketTraceConnector::WriteDataEvent(const SocketDataEvent& event) {
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
      rio::SerializeToStream(pb, perf_buffer_events_output_stream_.get());
      *perf_buffer_events_output_stream_ << std::flush;
      break;
  }
}

//-----------------------------------------------------------------------------
// TransferData Helpers
//-----------------------------------------------------------------------------

void SocketTraceConnector::TransferStreams(ConnectorContext* ctx, uint32_t table_num,
                                           DataTable* data_table) {
  // TODO(oazizi): TransferStreams() is slightly inefficient because it loops through all
  //               connection trackers, but processing a mutually exclusive subset each time.
  //               This is because trackers for different tables are mixed together
  //               in a single pool. This is not a big concern as long as the number of tables
  //               is small (currently only 2).
  //               Possible solutions: 1) different pools, 2) auxiliary pool of pointers.

  std::vector<CIDRBlock> cluster_cidrs = ctx->GetClusterCIDRs();

  // Outer loop iterates through tracker sets (keyed by PID+FD),
  // while inner loop iterates through generations of trackers for that PID+FD pair.
  auto tracker_set_it = connection_trackers_.begin();
  while (tracker_set_it != connection_trackers_.end()) {
    auto& tracker_generations = tracker_set_it->second;

    auto generation_it = tracker_generations.begin();
    while (generation_it != tracker_generations.end()) {
      auto& tracker = generation_it->second;

      VLOG(2) << absl::Substitute("Connection conn_id=$0 protocol=$1\n",
                                  ToString(tracker.conn_id()),
                                  magic_enum::enum_name(tracker.traffic_class().protocol));

      DCHECK(protocol_transfer_specs_.find(tracker.traffic_class().protocol) !=
             protocol_transfer_specs_.end())
          << absl::Substitute("Protocol=$0 not in protocol_transfer_specs_.",
                              tracker.traffic_class().protocol);

      const TransferSpec& transfer_spec =
          protocol_transfer_specs_[tracker.traffic_class().protocol];

      // Don't process trackers meant for a different table_num.
      if (transfer_spec.table_num != table_num) {
        ++generation_it;
        continue;
      }

      tracker.IterationPreTick(cluster_cidrs, proc_parser_.get(), socket_info_mgr_.get());

      if (transfer_spec.transfer_fn && transfer_spec.enabled) {
        transfer_spec.transfer_fn(*this, ctx, &tracker, data_table);
      }

      tracker.IterationPostTick();

      // Update iterator, handling deletions as we go. This must be the last line in the loop.
      generation_it = tracker.ReadyForDestruction() ? tracker_generations.erase(generation_it)
                                                    : ++generation_it;
    }

    tracker_set_it =
        tracker_generations.empty() ? connection_trackers_.erase(tracker_set_it) : ++tracker_set_it;
  }
}

template <typename TProtocolTraits>
void SocketTraceConnector::TransferStream(ConnectorContext* ctx, ConnectionTracker* tracker,
                                          DataTable* data_table) {
  VLOG(3) << absl::StrCat("Connection\n", DebugString<TProtocolTraits>(*tracker, ""));

  if (tracker->state() == ConnectionTracker::State::kTransferring) {
    // ProcessToRecords() parses raw events and produces messages in format that are expected by
    // table store. But those messages are not cached inside ConnectionTracker.
    //
    // TODO(yzhao): Consider caching produced messages if they are not transferred.
    auto result = tracker->ProcessToRecords<TProtocolTraits>();
    for (auto& msg : result) {
      AppendMessage(ctx, *tracker, std::move(msg), data_table);
    }
  }
}

void SocketTraceConnector::TransferConnectionStats(ConnectorContext* ctx, DataTable* data_table) {
  namespace idx = ::pl::stirling::conn_stats_idx;

  absl::flat_hash_set<md::UPID> upids = ctx->GetUPIDs();

  auto& agg_stats = connection_stats_.mutable_agg_stats();

  auto iter = agg_stats.begin();
  while (iter != agg_stats.end()) {
    const auto& key = iter->first;
    auto& stats = iter->second;

    if (stats.conn_open < stats.conn_close) {
      LOG_FIRST_N(WARNING, 10) << "Connection open should not be smaller than connection close.";
    }

    // Only export this record if there are actual changes.
    // TODO(yzhao): Exports these records after several iterations.
    if (!stats.prev_bytes_sent.has_value() || !stats.prev_bytes_recv.has_value() ||
        stats.bytes_sent != stats.prev_bytes_sent || stats.bytes_recv != stats.prev_bytes_recv) {
      uint64_t time = AdjustedSteadyClockNowNS();

      DataTable::RecordBuilder<&kConnStatsTable> r(data_table, time);

      r.Append<idx::kTime>(time);
      md::UPID upid(ctx->GetASID(), key.upid.tgid, key.upid.start_time_ticks);
      r.Append<idx::kUPID>(upid.value());
      r.Append<idx::kRemoteAddr>(key.remote_addr);
      r.Append<idx::kRemotePort>(key.remote_port);
      r.Append<idx::kAddrFamily>(static_cast<int>(stats.addr_family));
      r.Append<idx::kProtocol>(stats.traffic_class.protocol);
      r.Append<idx::kRole>(stats.traffic_class.role);
      r.Append<idx::kConnOpen>(stats.conn_open);
      r.Append<idx::kConnClose>(stats.conn_close);
      r.Append<idx::kConnActive>(stats.conn_open - stats.conn_close);
      // TODO(yzhao/oazizi): This is a bug, because the stats only reflect the bytes of BPF events
      //                     that made it through the perf buffer; lost events are not included.
      //                     A better approach is to have BPF events update the cumulative number of
      //                     bytes transferred,  and use that here directly. We already have a
      //                     ticket to convert seq numbers to bytes transferred, so we could kill
      //                     two birds with one stone.
      r.Append<idx::kBytesSent>(stats.bytes_sent);
      r.Append<idx::kBytesRecv>(stats.bytes_recv);
#ifndef NDEBUG
      r.Append<idx::kPxInfo>("");
#endif

      stats.prev_bytes_sent = stats.bytes_sent;
      stats.prev_bytes_recv = stats.bytes_recv;
    }

    // Remove data for exited upids. Do this after exporting data so that the last records are
    // exported.
    md::UPID current_upid(ctx->GetASID(), key.upid.tgid, key.upid.start_time_ticks);
    if (!upids.contains(current_upid)) {
      // NOTE: absl doesn't support iter = agg_stats.erase(iter), so must use this style.
      agg_stats.erase(iter++);
      continue;
    }

    // This is at the bottom, in order to avoid accidentally forgetting increment the iterator.
    ++iter;
  }
}

}  // namespace stirling
}  // namespace pl

#endif
