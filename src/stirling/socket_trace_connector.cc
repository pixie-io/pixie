#ifdef __linux__

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
#include "src/stirling/common/event_parser.h"
#include "src/stirling/common/go_grpc_types.h"
#include "src/stirling/cql/types.h"
#include "src/stirling/http/http_stitcher.h"
#include "src/stirling/http2/grpc.h"
#include "src/stirling/http2/http2.h"
#include "src/stirling/mysql/mysql_parse.h"
#include "src/stirling/obj_tools/obj_tools.h"
#include "src/stirling/obj_tools/proc_path_tools.h"
#include "src/stirling/proto/sock_event.pb.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/utils/linux_headers.h"

DEFINE_bool(stirling_enable_parsing_protobufs, false,
            "If true, parses binary protobufs captured in gRPC messages. "
            "As of 2019-07, the parser can only handle protobufs defined in Hipster Shop.");
DEFINE_int32(test_only_socket_trace_target_pid, kTraceAllTGIDs, "The process to trace.");
// TODO(oazizi): Consolidate with dynamic sampling period though SetSamplingPeriod().
DEFINE_uint32(stirling_socket_trace_sampling_period_millis, 100,
              "The sampling period, in milliseconds, at which Stirling reads the BPF perf buffers "
              "for events.");
// TODO(yzhao): If we ever need to write all events from different perf buffers, then we need either
// write to different files for individual perf buffers, or create a protobuf message with an oneof
// field to include all supported message types.
DEFINE_string(perf_buffer_events_output_path, "",
              "If not empty, specifies the path & format to a file to which the socket tracer "
              "writes data events. If the filename ends with '.bin', the events are serialized in "
              "binary format; otherwise, text format.");

// TODO(oazizi/yzhao): Re-enable grpc and mysql tracing once stable.
DEFINE_bool(stirling_enable_http_tracing, true,
            "If true, stirling will trace and process HTTP messages");
DEFINE_bool(stirling_enable_grpc_kprobe_tracing, false,
            "If true, stirling will trace and process gRPC RPCs.");
DEFINE_bool(stirling_enable_grpc_uprobe_tracing, true,
            "If true, stirling will trace and process gRPC RPCs.");
DEFINE_bool(stirling_enable_mysql_tracing, true,
            "If true, stirling will trace and process MySQL messages.");
DEFINE_bool(stirling_enable_cass_tracing, true,
            "If true, stirling will trace and process Cassandra messages.");
DEFINE_bool(stirling_disable_self_tracing, true,
            "If true, stirling will trace and process syscalls made by itself.");
const char kRoleClientStr[] = "CLIENT";
const char kRoleServerStr[] = "SERVER";
const char kRoleAllStr[] = "ALL";
DEFINE_string(stirling_role_to_trace, kRoleAllStr,
              "Must be one of [CLIENT|SERVER|ALL]. Specify which role(s) will be trace by BPF.");
DEFINE_string(stirling_cluster_cidr, "", "Manual Cluster CIDR");

// This flag is for survivability only, in case the host's located headers don't work.
DEFINE_bool(stirling_use_packaged_headers, false, "Force use of packaged kernel headers for BCC.");

BCC_SRC_STRVIEW(socket_trace_bcc_script, socket_trace);

namespace pl {
namespace stirling {

using ::google::protobuf::TextFormat;
using ::pl::grpc::MethodInputOutput;
using ::pl::stirling::kCQLTable;
using ::pl::stirling::kHTTPTable;
using ::pl::stirling::kMySQLTable;
using ::pl::stirling::elf_tools::ElfReader;
using ::pl::stirling::grpc::ParsePB;
using ::pl::stirling::http2::HTTP2Message;
using ::pl::stirling::obj_tools::GetActiveBinary;
using ::pl::stirling::obj_tools::ResolveProcessPath;
using ::pl::stirling::utils::ToJSONString;

namespace {

StatusOr<EndpointRole> ParseEndpointRoleFlag(std::string_view role_str) {
  if (role_str == kRoleClientStr) {
    return kRoleClient;
  } else if (role_str == kRoleServerStr) {
    return kRoleServer;
  } else if (role_str == kRoleAllStr) {
    return kRoleAll;
  } else {
    return error::InvalidArgument("Invalid flag value $0", role_str);
  }
}

}  // namespace

SocketTraceConnector::SocketTraceConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables,
                      std::chrono::milliseconds(FLAGS_stirling_socket_trace_sampling_period_millis),
                      kDefaultPushPeriod),
      bpf_tools::BCCWrapper() {
  proc_parser_ = std::make_unique<system::ProcParser>(system::Config::GetInstance());

  EndpointRole role_to_trace = ParseEndpointRoleFlag(FLAGS_stirling_role_to_trace).ValueOrDie();

  DCHECK(protocol_transfer_specs_.find(kProtocolHTTP) != protocol_transfer_specs_.end());
  protocol_transfer_specs_[kProtocolHTTP].enabled = FLAGS_stirling_enable_http_tracing;
  protocol_transfer_specs_[kProtocolHTTP].role_to_trace = role_to_trace;

  DCHECK(protocol_transfer_specs_.find(kProtocolHTTP2) != protocol_transfer_specs_.end());
  protocol_transfer_specs_[kProtocolHTTP2].enabled = FLAGS_stirling_enable_grpc_kprobe_tracing;
  protocol_transfer_specs_[kProtocolHTTP2].role_to_trace = role_to_trace;

  DCHECK(protocol_transfer_specs_.find(kProtocolMySQL) != protocol_transfer_specs_.end());
  protocol_transfer_specs_[kProtocolMySQL].enabled = FLAGS_stirling_enable_mysql_tracing;
  protocol_transfer_specs_[kProtocolMySQL].role_to_trace = role_to_trace;

  DCHECK(protocol_transfer_specs_.find(kProtocolCQL) != protocol_transfer_specs_.end());
  protocol_transfer_specs_[kProtocolCQL].enabled = FLAGS_stirling_enable_mysql_tracing;
  protocol_transfer_specs_[kProtocolCQL].role_to_trace = role_to_trace;

  DCHECK(protocol_transfer_specs_.find(kProtocolHTTP2Uprobe) != protocol_transfer_specs_.end());
  protocol_transfer_specs_[kProtocolHTTP2Uprobe].enabled =
      FLAGS_stirling_enable_grpc_uprobe_tracing;

  StatusOr<std::unique_ptr<system::SocketInfoManager>> s =
      system::SocketInfoManager::Create(system::Config::GetInstance().proc_path());
  if (!s.ok()) {
    LOG(WARNING) << absl::Substitute("Failed to set up socket prober manager. Message: $0",
                                     s.msg());
  } else {
    socket_info_mgr_ = s.ConsumeValueOrDie();
  }
}

Status SocketTraceConnector::InitImpl() {
  std::vector<utils::LinuxHeaderStrategy> linux_header_search_order =
      utils::kDefaultHeaderSearchOrder;
  if (FLAGS_stirling_use_packaged_headers) {
    linux_header_search_order = {utils::LinuxHeaderStrategy::kInstallPackagedHeaders};
  }
  PL_RETURN_IF_ERROR(utils::FindOrInstallLinuxHeaders(linux_header_search_order));

  constexpr uint64_t kNanosPerSecond = 1000 * 1000 * 1000;
  if (kNanosPerSecond % sysconfig_.KernelTicksPerSecond() != 0) {
    return error::Internal(
        "SC_CLK_TCK aka USER_HZ must be 100, otherwise our BPF code may not generate proper "
        "timestamps in a way that matches how /proc/stat does it");
  }

  if (FLAGS_stirling_enable_grpc_kprobe_tracing && FLAGS_stirling_enable_grpc_uprobe_tracing) {
    LOG(DFATAL) << "--stirling_enable_grpc_kprobe_tracing and "
                   "--stirling_enable_grpc_uprobe_tracing cannot be both true. "
                   "--stirling_enable_grpc_kprobe_tracing is forced to false.";
    FLAGS_stirling_enable_grpc_kprobe_tracing = false;
  }

  PL_RETURN_IF_ERROR(InitBPFProgram(socket_trace_bcc_script));
  PL_RETURN_IF_ERROR(AttachKProbes(kProbeSpecs));
  LOG(INFO) << absl::Substitute("Number of kprobes deployed = $0", kProbeSpecs.size());

  // Although we spawn off a uprobe attachment thread, call DeployUProbes() once before doing so.
  // Why? Some tests rely on UProbes being attached before returning from InitImpl().
  DeployUProbes();
  attach_uprobes_thread_ = std::thread([this]() { AttachUProbesLoop(); });

  PL_RETURN_IF_ERROR(OpenPerfBuffers(kPerfBufferSpecs, this));
  LOG(INFO) << absl::Substitute("Number of perf buffers opened = $0", kPerfBufferSpecs.size());

  LOG(INFO) << "Probes successfully deployed.";

  // TODO(yzhao): Consider adding a flag to switch the role to trace, i.e., between kRoleClient &
  // kRoleServer.
  if (protocol_transfer_specs_[kProtocolHTTP].enabled) {
    PL_RETURN_IF_ERROR(UpdateProtocolTraceRole(
        kProtocolHTTP, protocol_transfer_specs_[kProtocolHTTP].role_to_trace));
  }
  if (protocol_transfer_specs_[kProtocolHTTP2].enabled) {
    PL_RETURN_IF_ERROR(UpdateProtocolTraceRole(
        kProtocolHTTP2, protocol_transfer_specs_[kProtocolHTTP2].role_to_trace));
  }
  if (protocol_transfer_specs_[kProtocolMySQL].enabled) {
    PL_RETURN_IF_ERROR(UpdateProtocolTraceRole(
        kProtocolMySQL, protocol_transfer_specs_[kProtocolMySQL].role_to_trace));
  }
  if (protocol_transfer_specs_[kProtocolCQL].enabled) {
    PL_RETURN_IF_ERROR(UpdateProtocolTraceRole(
        kProtocolCQL, protocol_transfer_specs_[kProtocolCQL].role_to_trace));
  }
  PL_RETURN_IF_ERROR(TestOnlySetTargetPID(FLAGS_test_only_socket_trace_target_pid));
  if (FLAGS_stirling_disable_self_tracing) {
    PL_RETURN_IF_ERROR(DisableSelfTracing());
  }
  if (!FLAGS_perf_buffer_events_output_path.empty()) {
    std::filesystem::path output_path(FLAGS_perf_buffer_events_output_path);
    std::filesystem::path abs_path = std::filesystem::absolute(output_path);
    perf_buffer_events_output_stream_ = std::make_unique<std::ofstream>(abs_path);
    std::string format = "text";
    constexpr char kBinSuffix[] = ".bin";
    if (absl::EndsWith(FLAGS_perf_buffer_events_output_path, kBinSuffix)) {
      perf_buffer_events_output_format_ = OutputFormat::kBin;
      format = "binary";
    }
    LOG(INFO) << absl::Substitute("Writing output to: $0 in $1 format.", abs_path.string(), format);
  }

  bpf_table_info_ = std::make_shared<SocketTraceBPFTableManager>(&bpf());
  ConnectionTracker::SetBPFTableManager(bpf_table_info_);

  return Status::OK();
}

Status SocketTraceConnector::StopImpl() {
  if (perf_buffer_events_output_stream_ != nullptr) {
    perf_buffer_events_output_stream_->close();
  }
  if (attach_uprobes_thread_.joinable()) {
    attach_uprobes_thread_.join();
  }

  // Must call Stop() after attach_uprobes_thread_ has joined,
  // otherwise the two threads will cause concurrent accesses to BCC,
  // that will cause races and undefined behavior.
  bpf_tools::BCCWrapper::Stop();
  return Status::OK();
}

void SocketTraceConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                            DataTable* data_table) {
  DCHECK_LT(table_num, kTables.size())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);
  DCHECK(data_table != nullptr);

  // This drains all perf buffers, and causes Handle() callback functions to get called.
  // Note that it drains *all* perf buffers, not just those that are required for this table,
  // so raw data will be pushed to connection trackers more aggressively.
  // No data is lost, but this is a side-effect of sorts that affects timing of transfers.
  // It may be worth noting during debug.
  ReadPerfBuffers();

  // Set-up current state for connection inference purposes.
  if (socket_info_mgr_ != nullptr) {
    socket_info_mgr_->Flush();
  }

  TransferStreams(ctx, table_num, data_table);

  // Refresh UPIDs from MDS so that the uprobe attaching thread can detect new processes.
  set_mds_upids(ctx->GetMdsUpids());
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

Status SocketTraceConnector::UpdateProtocolTraceRole(TrafficProtocol protocol,
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
  int64_t my_pid = getpid();
  return UpdatePerCPUArrayValue(kStirlingTGIDIndex, my_pid, &control_map_handle);
}

std::map<std::string, std::vector<int32_t>> SocketTraceConnector::FindNewPIDs() {
  std::filesystem::path proc_path = system::Config::GetInstance().proc_path();

  // Get a list of all PIDs of interest: either from MDS,
  // or list all PIDs on the system if MDS is not present.
  // TODO(oazizi/yzhao): Technically the if statement is not checking for the presence of the MDS.
  // There could be a subtle bug lurking.
  absl::flat_hash_map<md::UPID, std::filesystem::path> upid_proc_path_map =
      ProcTracker::Cleanse(proc_path, get_mds_upids());
  if (upid_proc_path_map.empty()) {
    upid_proc_path_map = ProcTracker::ListUPIDs(proc_path);
  }

  // Consider new UPIDs only.
  absl::flat_hash_map<md::UPID, std::filesystem::path> new_upid_paths =
      proc_tracker_.TakeSnapshotAndDiff(std::move(upid_proc_path_map));

  // Convert to a map of binaries, with the upids that are instances of that binary.
  std::filesystem::path host_path = system::Config::GetInstance().host_path();
  std::map<std::string, std::vector<int32_t>> new_pids;

  for (const auto& [upid, pid_path] : new_upid_paths) {
    auto host_exe_or = GetActiveBinary(host_path, pid_path);
    if (!host_exe_or.ok()) {
      continue;
    }
    std::filesystem::path host_exe = host_exe_or.ConsumeValueOrDie();
    new_pids[host_exe.string()].push_back(upid.pid());
  }

  LOG_FIRST_N(INFO, 1) << absl::Substitute("New PIDs count = $0", new_pids.size());

  return new_pids;
}

bool SocketTraceConnector::UpdateHTTP2SymAddrs(
    ElfReader* elf_reader, const std::vector<int32_t>& pids,
    ebpf::BPFHashTable<uint32_t, struct conn_symaddrs_t>* http2_symaddrs_map) {
  struct conn_symaddrs_t symaddrs;
  symaddrs.syscall_conn =
      elf_reader
          ->SymbolAddress(
              "go.itab.*google.golang.org/grpc/credentials/internal.syscallConn,net.Conn")
          .value_or(-1);
  symaddrs.tls_conn = elf_reader->SymbolAddress("go.itab.*crypto/tls.Conn,net.Conn").value_or(-1);
  symaddrs.tcp_conn = elf_reader->SymbolAddress("go.itab.*net.TCPConn,net.Conn").value_or(-1);

  // TCPConn is mandatory by the HTTP2 uprobes probe, so bail if it is not found (-1).
  // It should be the last layer of nested interface, and contains the FD.
  // The other conns can be invalid, and will simply be ignored.
  if (symaddrs.tcp_conn == -1) {
    return false;
  }

  for (auto& pid : pids) {
    http2_symaddrs_map->update_value(pid, symaddrs);
  }

  return true;
}

StatusOr<int> SocketTraceConnector::AttachUProbeTmpl(
    const ArrayView<bpf_tools::UProbeTmpl>& probe_tmpls, const std::string& binary,
    elf_tools::ElfReader* elf_reader) {
  int uprobe_count = 0;
  for (const auto& tmpl : probe_tmpls) {
    bpf_tools::UProbeSpec spec = {binary, {}, tmpl.attach_type, std::string(tmpl.probe_fn)};
    const std::vector<std::string> symbol_names =
        elf_reader->ListSymbols(tmpl.symbol, tmpl.match_type);
    for (auto& symbol : symbol_names) {
      spec.symbol = symbol;
      PL_RETURN_IF_ERROR(AttachUProbe(spec));
      ++uprobe_count;
    }
  }
  return uprobe_count;
}

// TODO(oazizi/yzhao): Should HTTP uprobes use a different set of perf buffers than the kprobes?
// That allows the BPF code and companion user-space code for uprobe & kprobe be separated
// cleanly. For example, right now, enabling uprobe & kprobe simultaneously can crash Stirling,
// because of the mixed & duplicate data events from these 2 sources.
StatusOr<int> SocketTraceConnector::AttachHTTP2UProbes(
    const std::string& binary, elf_tools::ElfReader* elf_reader,
    const std::vector<int32_t>& new_pids,
    ebpf::BPFHashTable<uint32_t, struct conn_symaddrs_t>* http2_symaddrs_map) {
  // Step 1: Update BPF symbols_map on all new PIDs.
  bool found_symbols = UpdateHTTP2SymAddrs(elf_reader, new_pids, http2_symaddrs_map);
  if (!found_symbols) {
    // Doesn't appear to be a binary with a TCPConn.
    // Might not even be a golang binary.
    // Either way, not of interest to probe.
    return 0;
  }

  // Step 2: Deploy uprobes on all new binaries.
  auto result = http2_probed_binaries_.insert(binary);
  if (!result.second) {
    // This is not a new binary, so nothing more to do.
    return 0;
  }
  return AttachUProbeTmpl(kHTTP2UProbeTmpls, binary, elf_reader);
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

  std::filesystem::path container_lib;

  // Find the path to libssl for this binary, which may be inside a container.
  for (const auto& pid : new_pids) {
    StatusOr<absl::flat_hash_set<std::string>> libs_status = proc_parser_->GetMapPaths(pid);
    if (!libs_status.ok()) {
      VLOG(1) << absl::Substitute("Unable to check for libssl.so for $0. Message: $1", binary,
                                  libs_status.msg());
      continue;
    }
    std::filesystem::path proc_pid_path = std::filesystem::path("/proc") / std::to_string(pid);
    for (const auto& lib : libs_status.ValueOrDie()) {
      if (absl::EndsWith(lib, kLibSSL)) {
        StatusOr<std::filesystem::path> container_lib_status =
            ResolveProcessPath(proc_pid_path, lib);
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

  // Only try probing so files that we haven't already set probes on.
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

void SocketTraceConnector::DeployUProbes() {
  std::map<std::string, std::vector<int32_t>> new_pids = FindNewPIDs();

  // Suspected to be an expensive call, so perform outside the for loop.
  ebpf::BPFHashTable<uint32_t, struct conn_symaddrs_t> http2_symaddrs_map =
      bpf().get_hash_table<uint32_t, struct conn_symaddrs_t>("http2_symaddrs_map");

  int uprobe_count = 0;
  for (auto& [binary, pid_vec] : new_pids) {
    // Read binary's symbols.
    StatusOr<std::unique_ptr<ElfReader>> elf_reader_status = ElfReader::Create(binary);
    if (!elf_reader_status.ok()) {
      LOG(WARNING) << absl::Substitute(
          "Cannot analyze binary $0 for uprobe deployment. "
          "If file is under /var/lib/docker, container may have terminated. "
          "Message = $1",
          binary, elf_reader_status.msg());
      continue;
    }
    std::unique_ptr<ElfReader> elf_reader = elf_reader_status.ConsumeValueOrDie();

    // OpenSSL Probes.
    {
      StatusOr<int> attach_status = AttachOpenSSLUProbes(binary, pid_vec);
      if (!attach_status.ok()) {
        LOG(WARNING) << absl::Substitute("Failed to attach SSL Uprobes to $0: $1", binary,
                                         attach_status.ToString());
      } else {
        uprobe_count += attach_status.ValueOrDie();
      }
    }

    // HTTP2 Probes.
    if (protocol_transfer_specs_[kProtocolHTTP2Uprobe].enabled) {
      StatusOr<int> attach_status =
          AttachHTTP2UProbes(binary, elf_reader.get(), pid_vec, &http2_symaddrs_map);
      if (!attach_status.ok()) {
        LOG(WARNING) << absl::Substitute("Failed to attach HTTP2 Uprobes to $0: $1", binary,
                                         attach_status.ToString());
      } else {
        uprobe_count += attach_status.ValueOrDie();
      }
    }

    // Add other uprobes here.
  }

  LOG_FIRST_N(INFO, 1) << absl::Substitute("Number of uprobes deployed = $0", uprobe_count);
}

void SocketTraceConnector::AttachUProbesLoop() {
  constexpr std::chrono::seconds kLoopIterationSeconds = std::chrono::seconds(5);

  auto next_update_time_point = std::chrono::steady_clock::now() + kLoopIterationSeconds;
  while (state() != State::kStopped) {
    // Check more often than the actual attachment cycle, so that we can exit the loop faster.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (std::chrono::steady_clock::now() < next_update_time_point) {
      continue;
    }

    DeployUProbes();

    next_update_time_point = std::chrono::steady_clock::now() + kLoopIterationSeconds;
  }
}

//-----------------------------------------------------------------------------
// Perf Buffer Polling and Callback functions.
//-----------------------------------------------------------------------------

void SocketTraceConnector::ReadPerfBuffers() {
  for (auto& buffer_name : kPerfBuffers) {
    PollPerfBuffer(buffer_name);
  }
}

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
  LOG(WARNING) << ProbeLossMessage("socket_data_events", lost);
}

void SocketTraceConnector::HandleControlEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  connector->AcceptControlEvent(*static_cast<const socket_control_event_t*>(data));
}

void SocketTraceConnector::HandleControlEventsLoss(void* /*cb_cookie*/, uint64_t lost) {
  LOG(WARNING) << ProbeLossMessage("socket_control_events", lost);
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
  LOG(WARNING) << ProbeLossMessage("go_grpc_header_events", lost);
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
  LOG(WARNING) << ProbeLossMessage("go_grpc_data_events", lost);
}

//-----------------------------------------------------------------------------
// Connection Tracker Events
//-----------------------------------------------------------------------------

namespace {

uint64_t GetConnMapKey(struct conn_id_t conn_id) {
  return (static_cast<uint64_t>(conn_id.upid.pid) << 32) | conn_id.fd;
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
  pb->mutable_attr()->set_seq_num(event.attr.seq_num);
  pb->mutable_attr()->set_msg_size(event.attr.msg_size);
  pb->set_msg(event.msg);
}

}  // namespace

void SocketTraceConnector::AcceptDataEvent(std::unique_ptr<SocketDataEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();
  if (perf_buffer_events_output_stream_ != nullptr) {
    sockeventpb::SocketDataEvent pb;
    SocketDataEventToPB(*event, &pb);
    std::string text;
    switch (perf_buffer_events_output_format_) {
      case OutputFormat::kTxt:
        // TextFormat::Print() can print to a stream. That complicates things a bit, and we opt not
        // to do that as this is for debugging.
        TextFormat::PrintToString(pb, &text);
        // TextFormat already output a \n, so no need to do it here.
        *perf_buffer_events_output_stream_ << text;
        break;
      case OutputFormat::kBin:
        rio::SerializeToStream(pb, perf_buffer_events_output_stream_.get());
        *perf_buffer_events_output_stream_ << std::flush;
        break;
    }
  }

  const uint64_t conn_map_key = GetConnMapKey(event->attr.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";
  DCHECK(event->attr.traffic_class.protocol == kProtocolHTTP ||
         event->attr.traffic_class.protocol == kProtocolHTTP2 ||
         event->attr.traffic_class.protocol == kProtocolMySQL ||
         event->attr.traffic_class.protocol == kProtocolCQL)
      << absl::Substitute("AcceptDataEvent received event with unknown protocol: $0",
                          magic_enum::enum_name(event->attr.traffic_class.protocol));

  ConnectionTracker& tracker = connection_trackers_[conn_map_key][event->attr.conn_id.tsid];
  tracker.AddDataEvent(std::move(event));
}

void SocketTraceConnector::AcceptControlEvent(socket_control_event_t event) {
  // timestamp_ns is a common field of open and close fields.
  event.open.timestamp_ns += ClockRealTimeOffset();
  // conn_id is a common field of open & close.
  const uint64_t conn_map_key = GetConnMapKey(event.open.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";
  ConnectionTracker& tracker = connection_trackers_[conn_map_key][event.open.conn_id.tsid];
  tracker.AddControlEvent(event);
}

void SocketTraceConnector::AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();
  const uint64_t conn_map_key = GetConnMapKey(event->attr.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";
  ConnectionTracker& tracker = connection_trackers_[conn_map_key][event->attr.conn_id.tsid];
  tracker.AddHTTP2Header(std::move(event));
}

void SocketTraceConnector::AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();
  const uint64_t conn_map_key = GetConnMapKey(event->attr.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";
  ConnectionTracker& tracker = connection_trackers_[conn_map_key][event->attr.conn_id.tsid];
  tracker.AddHTTP2Data(std::move(event));
}

const ConnectionTracker* SocketTraceConnector::GetConnectionTracker(
    struct conn_id_t conn_id) const {
  const uint64_t conn_map_key = GetConnMapKey(conn_id);

  auto tracker_set_it = connection_trackers_.find(conn_map_key);
  if (tracker_set_it == connection_trackers_.end()) {
    return nullptr;
  }

  const auto& tracker_generations = tracker_set_it->second;
  auto tracker_it = tracker_generations.find(conn_id.tsid);
  if (tracker_it == tracker_generations.end()) {
    return nullptr;
  }

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
                                         const ConnectionTracker& conn_tracker, http::Record record,
                                         DataTable* data_table) {
  DCHECK_EQ(kHTTPTable.elements().size(), data_table->ActiveRecordBatch()->size());

  http::Message& req_message = record.req;
  http::Message& resp_message = record.resp;

  // Currently decompresses gzip content, but could handle other transformations too.
  // Note that we do this after filtering to avoid burning CPU cycles unnecessarily.
  http::PreProcessMessage(&resp_message);

  md::UPID upid(ctx->AgentMetadataState()->asid(), conn_tracker.pid(),
                conn_tracker.pid_start_time_ticks());

  HTTPContentType content_type = HTTPContentType::kUnknown;
  if (http::IsJSONContent(resp_message)) {
    content_type = HTTPContentType::kJSON;
  }

  RecordBuilder<&kHTTPTable> r(data_table);
  r.Append<r.ColIndex("time_")>(resp_message.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  // Note that there is a string copy here,
  // But std::move is not allowed because we re-use conn object.
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port);
  r.Append<r.ColIndex("http_major_version")>(1);
  r.Append<r.ColIndex("http_minor_version")>(resp_message.http_minor_version);
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(content_type));
  r.Append<r.ColIndex("http_req_headers")>(ToJSONString(req_message.http_headers));
  r.Append<r.ColIndex("http_req_method")>(std::move(req_message.http_req_method));
  r.Append<r.ColIndex("http_req_path")>(std::move(req_message.http_req_path));
  r.Append<r.ColIndex("http_req_body")>("-");
  r.Append<r.ColIndex("http_resp_headers")>(ToJSONString(resp_message.http_headers));
  r.Append<r.ColIndex("http_resp_status")>(resp_message.http_resp_status);
  r.Append<r.ColIndex("http_resp_message")>(std::move(resp_message.http_resp_message));
  r.Append<r.ColIndex("http_resp_body")>(std::move(resp_message.http_msg_body));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_message.timestamp_ns, resp_message.timestamp_ns));
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         http2::Record record, DataTable* data_table) {
  DCHECK_EQ(kHTTPTable.elements().size(), data_table->ActiveRecordBatch()->size());

  HTTP2Message& req_message = record.req;
  HTTP2Message& resp_message = record.resp;

  int64_t resp_status;
  ECHECK(absl::SimpleAtoi(resp_message.headers.ValueByKey(":status", "-1"), &resp_status));

  md::UPID upid(ctx->AgentMetadataState()->asid(), conn_tracker.pid(),
                conn_tracker.pid_start_time_ticks());

  std::string method = req_message.headers.ValueByKey(":method");

  if (FLAGS_stirling_enable_parsing_protobufs) {
    MethodInputOutput rpc = grpc_desc_db_.GetMethodInputOutput(::pl::grpc::MethodPath(method));
    req_message.message = ParsePB(req_message.message, rpc.input.get());
    resp_message.message = ParsePB(resp_message.message, rpc.output.get());
  }

  RecordBuilder<&kHTTPTable> r(data_table);
  r.Append<r.ColIndex("time_")>(resp_message.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port);
  r.Append<r.ColIndex("http_major_version")>(2);
  // HTTP2 does not define minor version.
  r.Append<r.ColIndex("http_minor_version")>(0);
  r.Append<r.ColIndex("http_req_headers")>(ToJSONString(req_message.headers));
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(HTTPContentType::kGRPC));
  r.Append<r.ColIndex("http_resp_headers")>(ToJSONString(resp_message.headers));
  r.Append<r.ColIndex("http_req_method")>(method);
  r.Append<r.ColIndex("http_req_path")>(req_message.headers.ValueByKey(":path"));
  r.Append<r.ColIndex("http_resp_status")>(resp_status);
  // TODO(yzhao): Populate the following field from headers.
  r.Append<r.ColIndex("http_resp_message")>("-");
  r.Append<r.ColIndex("http_req_body")>(std::move(req_message.message));
  r.Append<r.ColIndex("http_resp_body")>(std::move(resp_message.message));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_message.timestamp_ns, resp_message.timestamp_ns));
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         http2u::Record record, DataTable* data_table) {
  DCHECK_EQ(kHTTPTable.elements().size(), data_table->ActiveRecordBatch()->size());

  http2u::HalfStream* req_stream;
  http2u::HalfStream* resp_stream;

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
  ECHECK(absl::SimpleAtoi(resp_stream->headers.ValueByKey(":status", "-1"), &resp_status));

  md::UPID upid(ctx->AgentMetadataState()->asid(), conn_tracker.pid(),
                conn_tracker.pid_start_time_ticks());

  std::string method = req_stream->headers.ValueByKey(":method");

  if (FLAGS_stirling_enable_parsing_protobufs) {
    MethodInputOutput rpc = grpc_desc_db_.GetMethodInputOutput(::pl::grpc::MethodPath(method));
    req_stream->data = ParsePB(req_stream->data, rpc.input.get());
    resp_stream->data = ParsePB(resp_stream->data, rpc.output.get());
  }

  RecordBuilder<&kHTTPTable> r(data_table);
  r.Append<r.ColIndex("time_")>(req_stream->timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port);
  r.Append<r.ColIndex("http_major_version")>(2);
  // HTTP2 does not define minor version.
  r.Append<r.ColIndex("http_minor_version")>(0);
  r.Append<r.ColIndex("http_req_headers")>(ToJSONString(req_stream->headers));
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(HTTPContentType::kGRPC));
  r.Append<r.ColIndex("http_resp_headers")>(ToJSONString(resp_stream->headers));
  r.Append<r.ColIndex("http_req_method")>(method);
  r.Append<r.ColIndex("http_req_path")>(req_stream->headers.ValueByKey(":path"));
  r.Append<r.ColIndex("http_resp_status")>(resp_status);
  // TODO(yzhao): Populate the following field from headers.
  r.Append<r.ColIndex("http_resp_message")>("OK");
  r.Append<r.ColIndex("http_req_body")>(std::move(req_stream->data));
  r.Append<r.ColIndex("http_resp_body")>(std::move(resp_stream->data));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_stream->timestamp_ns, resp_stream->timestamp_ns));
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker, mysql::Record entry,
                                         DataTable* data_table) {
  DCHECK_EQ(kMySQLTable.elements().size(), data_table->ActiveRecordBatch()->size());

  md::UPID upid(ctx->AgentMetadataState()->asid(), conn_tracker.pid(),
                conn_tracker.pid_start_time_ticks());

  RecordBuilder<&kMySQLTable> r(data_table);
  r.Append<r.ColIndex("time_")>(entry.req.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port);
  r.Append<r.ColIndex("req_cmd")>(static_cast<uint64_t>(entry.req.cmd));
  r.Append<r.ColIndex("req_body")>(std::move(entry.req.msg));
  r.Append<r.ColIndex("resp_status")>(static_cast<uint64_t>(entry.resp.status));
  r.Append<r.ColIndex("resp_body")>(std::move(entry.resp.msg));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker, cass::Record entry,
                                         DataTable* data_table) {
  DCHECK_EQ(kCQLTable.elements().size(), data_table->ActiveRecordBatch()->size());

  md::UPID upid(ctx->GetASID(), conn_tracker.pid(), conn_tracker.pid_start_time_ticks());

  RecordBuilder<&kCQLTable> r(data_table);
  r.Append<r.ColIndex("time_")>(entry.req.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port);
  r.Append<r.ColIndex("req_op")>(static_cast<uint64_t>(entry.req.op));
  r.Append<r.ColIndex("req_body")>(std::move(entry.req.msg));
  r.Append<r.ColIndex("resp_op")>(static_cast<uint64_t>(entry.resp.op));
  r.Append<r.ColIndex("resp_body")>(std::move(entry.resp.msg));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
}

//-----------------------------------------------------------------------------
// TransferData Helpers
//-----------------------------------------------------------------------------

namespace {
std::vector<CIDRBlock> ClusterCIDRs(ConnectorContext* ctx) {
  // TODO(yzhao/oazizi): Cache CIDRs (Except for service CIDR, which must be updated continually).
  std::optional<CIDRBlock> pod_cidr =
      ctx->AgentMetadataState()->k8s_metadata_state().cluster_cidr();
  std::optional<CIDRBlock> service_cidr =
      ctx->AgentMetadataState()->k8s_metadata_state().service_cidr();

  std::vector<CIDRBlock> cluster_cidrs;
  if (pod_cidr.has_value()) {
    cluster_cidrs.push_back(std::move(pod_cidr.value()));
  }
  if (service_cidr.has_value()) {
    cluster_cidrs.push_back(std::move(service_cidr.value()));
  }
  if (!FLAGS_stirling_cluster_cidr.empty()) {
    CIDRBlock cidr;
    Status s = ParseCIDRBlock(FLAGS_stirling_cluster_cidr, &cidr);
    if (s.ok()) {
      cluster_cidrs.push_back(std::move(cidr));
    } else {
      LOG_FIRST_N(ERROR, 1) << absl::Substitute(
          "Could not parse flag --stirling_cluster_cidr as a CIDR. Value=$0",
          FLAGS_stirling_cluster_cidr);
    }
  }

  return cluster_cidrs;
}
}  // namespace

void SocketTraceConnector::TransferStreams(ConnectorContext* ctx, uint32_t table_num,
                                           DataTable* data_table) {
  // TODO(oazizi): TransferStreams() is slightly inefficient because it loops through all
  //               connection trackers, but processing a mutually exclusive subset each time.
  //               This is because trackers for different tables are mixed together
  //               in a single pool. This is not a big concern as long as the number of tables
  //               is small (currently only 2).
  //               Possible solutions: 1) different pools, 2) auxiliary pool of pointers.

  std::vector<CIDRBlock> cluster_cidrs = ClusterCIDRs(ctx);

  // Outer loop iterates through tracker sets (keyed by PID+FD),
  // while inner loop iterates through generations of trackers for that PID+FD pair.
  auto tracker_set_it = connection_trackers_.begin();
  while (tracker_set_it != connection_trackers_.end()) {
    auto& tracker_generations = tracker_set_it->second;

    auto generation_it = tracker_generations.begin();
    while (generation_it != tracker_generations.end()) {
      auto& tracker = generation_it->second;

      VLOG(2) << absl::Substitute("Connection pid=$0 fd=$1 tsid=$2 protocol=$3\n", tracker.pid(),
                                  tracker.fd(), tracker.tsid(),
                                  magic_enum::enum_name(tracker.protocol()));

      DCHECK(protocol_transfer_specs_.find(tracker.protocol()) != protocol_transfer_specs_.end())
          << absl::Substitute("Protocol=$0 not in protocol_transfer_specs_.", tracker.protocol());

      const TransferSpec& transfer_spec = protocol_transfer_specs_[tracker.protocol()];

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

      // Only the most recent generation of a connection on a PID+FD should be active.
      // Mark all others for death (after having their data processed, of course).
      if (generation_it != --tracker_generations.end()) {
        tracker.MarkForDeath();
      }

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
      AppendMessage(ctx, *tracker, msg, data_table);
    }
  }
}

template void SocketTraceConnector::TransferStream<http::ProtocolTraits>(ConnectorContext* ctx,
                                                                         ConnectionTracker* tracker,
                                                                         DataTable* data_table);
template void SocketTraceConnector::TransferStream<http2::ProtocolTraits>(
    ConnectorContext* ctx, ConnectionTracker* tracker, DataTable* data_table);
template void SocketTraceConnector::TransferStream<http2u::ProtocolTraits>(
    ConnectorContext* ctx, ConnectionTracker* tracker, DataTable* data_table);
template void SocketTraceConnector::TransferStream<mysql::ProtocolTraits>(
    ConnectorContext* ctx, ConnectionTracker* tracker, DataTable* data_table);
template void SocketTraceConnector::TransferStream<cass::ProtocolTraits>(ConnectorContext* ctx,
                                                                         ConnectionTracker* tracker,
                                                                         DataTable* data_table);

}  // namespace stirling
}  // namespace pl

#endif
