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

#include "src/stirling/stirling.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <absl/base/internal/spinlock.h>

#include "src/common/base/base.h"
#include "src/common/perf/elapsed_timer.h"
#include "src/common/system/system_info.h"

#include "src/stirling/bpf_tools/probe_cleaner.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/pub_sub_manager.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/proto/stirling.pb.h"

#include "src/stirling/source_connectors/dynamic_bpftrace/dynamic_bpftrace_connector.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_trace_connector.h"
#include "src/stirling/source_connectors/jvm_stats/jvm_stats_connector.h"
#include "src/stirling/source_connectors/network_stats/network_stats_connector.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/pid_runtime/pid_runtime_connector.h"
#include "src/stirling/source_connectors/proc_stat/proc_stat_connector.h"
#include "src/stirling/source_connectors/process_stats/process_stats_connector.h"
#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dynamic_tracer.h"

namespace px {
namespace stirling {

namespace {

#define REGISTRY_PAIR(source) \
  { source::kName, SourceRegistry::CreateRegistryElement<source>() }
const absl::flat_hash_map<std::string_view, SourceRegistry::RegistryElement> kAllSources = {
    REGISTRY_PAIR(JVMStatsConnector),     REGISTRY_PAIR(PIDRuntimeConnector),
    REGISTRY_PAIR(ProcStatConnector),     REGISTRY_PAIR(SeqGenConnector),
    REGISTRY_PAIR(SocketTraceConnector),  REGISTRY_PAIR(ProcessStatsConnector),
    REGISTRY_PAIR(NetworkStatsConnector), REGISTRY_PAIR(PerfProfileConnector),
};
#undef REGISTRY_PAIR

}  // namespace

// clang-format off
absl::flat_hash_set<std::string_view> GetSourceNamesForGroup(SourceConnectorGroup group) {
  switch (group) {
    case SourceConnectorGroup::kNone:
      return {};
    case SourceConnectorGroup::kProd:
      return {
        ProcessStatsConnector::kName,
        NetworkStatsConnector::kName,
        JVMStatsConnector::kName,
        SocketTraceConnector::kName,
        PerfProfileConnector::kName
      };
    case SourceConnectorGroup::kAll:
      return {
        ProcessStatsConnector::kName,
        NetworkStatsConnector::kName,
        JVMStatsConnector::kName,
        PIDRuntimeConnector::kName,
        ProcStatConnector::kName,
        SeqGenConnector::kName,
        SocketTraceConnector::kName,
        PerfProfileConnector::kName
      };
    case SourceConnectorGroup::kTracers:
      return {
        SocketTraceConnector::kName
      };
    case SourceConnectorGroup::kMetrics:
      return {
        ProcessStatsConnector::kName,
        NetworkStatsConnector::kName,
        JVMStatsConnector::kName
      };
    case SourceConnectorGroup::kProfiler:
      return {
        PerfProfileConnector::kName
      };
    default:
      // To keep GCC happy.
      DCHECK(false);
      return {};
  }
}
// clang-format on

StatusOr<std::unique_ptr<SourceRegistry>> CreateSourceRegistry(
    const absl::flat_hash_set<std::string_view>& source_names) {
  auto result = std::make_unique<SourceRegistry>();

  for (auto name : source_names) {
    auto iter = kAllSources.find(name);
    if (iter == kAllSources.end()) {
      return error::InvalidArgument("Source name $0 is not available.");
    }
    PL_RETURN_IF_ERROR(result->Register(iter->first, iter->second));
  }

  return result;
}

std::unique_ptr<SourceRegistry> CreateProdSourceRegistry() {
  return CreateSourceRegistry(GetSourceNamesForGroup(SourceConnectorGroup::kProd))
      .ConsumeValueOrDie();
}

// Holds InfoClassManager and DataTable.
struct SourceOutput {
  std::vector<InfoClassManager*> info_class_mgrs;
  std::vector<DataTable*> data_tables;
};

class StirlingImpl final : public Stirling {
 public:
  explicit StirlingImpl(std::unique_ptr<SourceRegistry> registry);

  ~StirlingImpl() override;

  void RegisterUserDebugSignalHandlers(int signum) override;

  // TODO(oazizi/yzhao): Consider lift this as an interface method into Stirling, making it
  // symmetric with Stop().
  Status Init();

  void RegisterTracepoint(
      sole::uuid uuid,
      std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> program) override;
  StatusOr<stirlingpb::Publish> GetTracepointInfo(sole::uuid trace_id) override;
  Status RemoveTracepoint(sole::uuid trace_id) override;
  void GetPublishProto(stirlingpb::Publish* publish_pb) override;
  void RegisterDataPushCallback(DataPushCallback f) override { data_push_callback_ = f; }
  void RegisterAgentMetadataCallback(AgentMetadataCallback f) override {
    DCHECK(f != nullptr);
    agent_metadata_callback_ = f;
  }
  std::unique_ptr<ConnectorContext> GetContext();

  void Run() override;
  Status RunAsThread() override;
  bool IsRunning() const override;
  Status WaitUntilRunning(std::chrono::milliseconds timeout) const override;
  void Stop() override;
  void WaitForThreadJoin() override;

  void SetDebugLevel(int level);
  void EnablePIDTrace(int pid);
  void DisablePIDTrace(int pid);

 private:
  // Create data source connectors from the registered sources.
  Status CreateSourceConnectors();

  // Adds a source to Stirling, and updates all state accordingly.
  Status AddSource(std::unique_ptr<SourceConnector> source);

  // Removes a source and all its info classes from stirling.
  Status RemoveSource(std::string_view source_name);

  // Creates and deploys dynamic tracing source.
  void DeployDynamicTraceConnector(
      sole::uuid trace_id,
      std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> program);

  // Destroys a dynamic tracing source created by DeployDynamicTraceConnector.
  void DestroyDynamicTraceConnector(sole::uuid trace_id);

  // Main run implementation.
  void RunCore();

  // Wait for Stirling to stop its main loop.
  void WaitForStop();

  // Main thread used to spawn off RunThread().
  std::thread run_thread_;

  std::atomic<bool> run_enable_ = false;
  std::atomic<bool> running_ = false;
  std::vector<std::unique_ptr<SourceConnector>> sources_ ABSL_GUARDED_BY(info_class_mgrs_lock_);

  // TODO(yzhao): Move InfoClassManager objects into SourceConnector, and remove this map.
  absl::flat_hash_map<SourceConnector*, SourceOutput> source_output_map_
      ABSL_GUARDED_BY(info_class_mgrs_lock_);

  InfoClassManagerVec info_class_mgrs_ ABSL_GUARDED_BY(info_class_mgrs_lock_);

  // Lock to protect both info_class_mgrs_ and sources_.
  absl::base_internal::SpinLock info_class_mgrs_lock_;

  std::unique_ptr<SourceRegistry> registry_;

  /**
   * Function to call to push data to the agent.
   * Function signature is:
   *   uint64_t table_id
   *   std::unique_ptr<ColumnWrapperRecordBatch> data
   */
  DataPushCallback data_push_callback_ = nullptr;

  AgentMetadataCallback agent_metadata_callback_ = nullptr;
  AgentMetadataType agent_metadata_;

  absl::base_internal::SpinLock dynamic_trace_status_map_lock_;
  absl::flat_hash_map<sole::uuid, StatusOr<stirlingpb::Publish>> dynamic_trace_status_map_
      ABSL_GUARDED_BY(dynamic_trace_status_map_lock_);
};

StirlingImpl* g_stirling_ptr = nullptr;

enum class SignalOpCode {
  // Reset the opcode. Signal handler will be waiting to receive an opcode.
  kNone = 0,

  // Set a general debug level for all source connectors.
  // Source connectors can dump more information according to the specified level.
  kSetDebugLevel = 1,

  // Specify a PID of interest for tracing. More information for this PID will be dumped.
  // Only the SocketTracer currently implements this, but in theory other source connectors
  // could enable PID traces as well.
  kPIDTrace = 2,
};

void ProcessSetDebugLevelOpcode(int level) {
  LOG(INFO) << absl::Substitute("Setting debug level to $0", level);
  g_stirling_ptr->SetDebugLevel(level);
}

void ProcessPIDTraceOpcode(int pid) {
  if (pid >= 0) {
    LOG(INFO) << absl::Substitute("Enabling tracing of PID: $0", pid);
    g_stirling_ptr->EnablePIDTrace(pid);
  } else {
    pid = -1 * pid;
    LOG(INFO) << absl::Substitute("Disabling tracing of PID: $0", pid);
    g_stirling_ptr->DisablePIDTrace(pid);
  }
}

// To multiplex different actions onto a single signal handler, Stirling uses a simple
// opcode+value protocol. Stirling expects signals to arrive in pairs:
//   signal 1: opcode - Chooses what action to perform.
//   signal 2: value  - An argument for the opcode.
// For example, to ask stirling to enable PID tracing for PID 33, one would send
//   1) opcode = 2 (kPIDTrace)
//   2) value = 33
//
// New opcodes can be added to expand the aspects of Stirling one can control via signals.
//
// Note that sending an opcode of 0 is special and resets the state. Thus sending 0 will
// always guarantee that the state machine expects an opcode next.
//
// See the stirling_ctrl utility for sending such control messages to stirling;
// it takes care of managing the protocol.
void UserSignalHandler(int /* signum */, siginfo_t* info, void* /* context */) {
  static SignalOpCode opcode = SignalOpCode::kNone;

  if (g_stirling_ptr == nullptr) {
    return;
  }

  if (opcode == SignalOpCode::kNone) {
    opcode = static_cast<SignalOpCode>(info->si_int);
    return;
  }

  int value = info->si_int;

  switch (opcode) {
    case SignalOpCode::kSetDebugLevel:
      ProcessSetDebugLevelOpcode(value);
      break;
    case SignalOpCode::kPIDTrace:
      ProcessPIDTraceOpcode(value);
      break;
    default:
      LOG(INFO) << absl::Substitute("Unexpected signal opcode: $0", value);
  }

  opcode = SignalOpCode::kNone;
}

StirlingImpl::StirlingImpl(std::unique_ptr<SourceRegistry> registry)
    : registry_(std::move(registry)) {}

StirlingImpl::~StirlingImpl() { Stop(); }

void StirlingImpl::RegisterUserDebugSignalHandlers(int signum) {
  g_stirling_ptr = this;

  // Signal for USR2: This is a PID-based signal that currently sets flags in the Socket Tracer,
  // to enable connection tracing for the particular PID.
  // This uses sigaction() instead of signal() because it needs to accept an integer.
  // Note that `kill -USR2` will no longer work for this signal. Instead sigqueue must be used
  // to send the signal.
  struct sigaction sigaction_specs = {};
  sigaction_specs.sa_sigaction = UserSignalHandler;
  sigaction_specs.sa_flags = SA_SIGINFO;
  sigemptyset(&sigaction_specs.sa_mask);
  sigaction(signum, &sigaction_specs, NULL);
}

Status StirlingImpl::Init() {
  system::LogSystemInfo();

  // Clean up any probes from a previous instance.
  Status s = utils::CleanProbes();

  // TODO(yzhao): The below logging cannot be DFATAL. Otherwise, non-OPT built stirling_wrapper
  // deployed along side PEM will always crash as the probes owned by PEM cannot be modified by
  // stirling_wrapper. Figure out a way to detect active probes owned by other processes,
  // in order to skip cleaning up those probes.
  LOG_IF(WARNING, !s.ok()) << absl::Substitute("Kprobe Cleaner failed. Message $0", s.msg());

  if (!registry_) {
    return error::NotFound("Source registry doesn't exist");
  }

  for (const auto& [name, registry_element] : registry_->sources()) {
    Status s = AddSource(registry_element.create_source_fn(name));
    LOG_IF(DFATAL, !s.ok()) << absl::Substitute(
        "Source Connector (registry name=$0) not instantiated, error: $1", name, s.ToString());
  }
  LOG(INFO) << "Stirling successfully initialized.";
  return Status::OK();
}

std::unique_ptr<ConnectorContext> StirlingImpl::GetContext() {
  if (agent_metadata_callback_ != nullptr) {
    return std::unique_ptr<ConnectorContext>(new AgentContext(agent_metadata_callback_()));
  }
  return std::unique_ptr<ConnectorContext>(new StandaloneContext());
}

namespace {

std::vector<DataTable*> GetDataTables(const std::vector<InfoClassManager*>& info_class_mgrs) {
  std::vector<DataTable*> data_tables;
  data_tables.reserve(info_class_mgrs.size());
  for (InfoClassManager* mgr : info_class_mgrs) {
    data_tables.push_back(mgr->data_table());
  }
  return data_tables;
}

}  // namespace

Status StirlingImpl::AddSource(std::unique_ptr<SourceConnector> source) {
  // Step 1: Init the source.
  PL_RETURN_IF_ERROR(source->Init());

  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

  std::vector<InfoClassManager*> mgrs;
  mgrs.reserve(source->table_schemas().size());

  for (const DataTableSchema& schema : source->table_schemas()) {
    LOG(INFO) << absl::Substitute("Adding info class: [$0/$1]", source->name(), schema.name());
    auto mgr = std::make_unique<InfoClassManager>(schema);
    mgr->SetSourceConnector(source.get());
    mgrs.push_back(mgr.get());
    info_class_mgrs_.push_back(std::move(mgr));
  }

  std::vector<DataTable*> data_tables = GetDataTables(mgrs);

  source_output_map_[source.get()] = {std::move(mgrs),
                                      // DataTable objects are created after subscribing.
                                      std::move(data_tables)};
  sources_.push_back(std::move(source));

  return Status::OK();
}

Status StirlingImpl::RemoveSource(std::string_view source_name) {
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

  // Find the source.
  auto source_iter = std::find_if(sources_.begin(), sources_.end(),
                                  [&source_name](const std::unique_ptr<SourceConnector>& s) {
                                    return s->name() == source_name;
                                  });
  if (source_iter == sources_.end()) {
    return error::Internal("RemoveSource(): could not find source with name=$0", source_name);
  }
  std::unique_ptr<SourceConnector>& source = *source_iter;

  // Remove all info class managers that point back to the source.
  info_class_mgrs_.erase(std::remove_if(info_class_mgrs_.begin(), info_class_mgrs_.end(),
                                        [&source](std::unique_ptr<InfoClassManager>& mgr) {
                                          return mgr->source() == source.get();
                                        }),
                         info_class_mgrs_.end());

  // Now perform the removal.
  PL_RETURN_IF_ERROR(source->Stop());
  source_output_map_.erase(source.get());
  sources_.erase(source_iter);

  return Status::OK();
}

// Returns, but updates the status map in a concurrent-safe way before doing so.
// Also converts all statuses to Internal so they don't conflict with the formal
// codes used on the API (e.g. NotFound or ResourceUnavailable).
// Since these errors come from a myriad of places, there would be no way to make sure
// an error from an underlying library doesn't produce NotFound, ResourceUnavailable
// or some future code that we plan to reserve.
#define RETURN_ERROR(s)                                                        \
  {                                                                            \
    Status ret_status(px::statuspb::Code::INTERNAL, s.msg());                  \
    absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_); \
    dynamic_trace_status_map_[trace_id] = ret_status;                          \
    LOG(INFO) << ret_status.ToString();                                        \
    return;                                                                    \
  }

#define ASSIGN_OR_RETURN_ERROR(lhs, rexpr) PL_ASSIGN_OR(lhs, rexpr, RETURN_ERROR(__s__.status());)
#define RETURN_IF_ERROR(s) \
  auto __s__ = s;          \
  if (!__s__.ok()) {       \
    RETURN_ERROR(__s__);   \
  }

namespace {

constexpr char kDynTraceSourcePrefix[] = "DT_";

StatusOr<std::unique_ptr<SourceConnector>> CreateDynamicSourceConnector(
    sole::uuid trace_id,
    dynamic_tracing::ir::logical::TracepointDeployment* tracepoint_deployment) {
  if (tracepoint_deployment->tracepoints().empty()) {
    return error::Internal("Nothing defined in the input tracepoint_deployment.");
  }

  if (tracepoint_deployment->tracepoints_size() > 1) {
    return error::Internal("Only one Tracepoint is currently supported.");
  }

  const auto tracepoint = tracepoint_deployment->tracepoints(0);

  if (tracepoint.has_program() && tracepoint.has_bpftrace()) {
    return error::Internal("Cannot have both PXL program and bpftrace.");
  }

  std::string source_name = absl::StrCat(kDynTraceSourcePrefix, trace_id.str());

  if (tracepoint.has_bpftrace()) {
    return DynamicBPFTraceConnector::Create(source_name, tracepoint);
  }
  return DynamicTraceConnector::Create(source_name, tracepoint_deployment);
}

}  // namespace

void StirlingImpl::DeployDynamicTraceConnector(
    sole::uuid trace_id,
    std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> program) {
  auto timer = ElapsedTimer();
  timer.Start();

  // Try creating the DynamicTraceConnector--which compiles BCC code.
  // On failure, set status and exit.
  ASSIGN_OR_RETURN_ERROR(std::unique_ptr<SourceConnector> source,
                         CreateDynamicSourceConnector(trace_id, program.get()));

  LOG(INFO) << absl::Substitute("DynamicTraceConnector [$0] created in $1 ms.", source->name(),
                                timer.ElapsedTime_us() / 1000.0);

  // Cache table schema name as source will be moved below.
  std::string output_name(source->table_schemas()[0].name());

  timer.Start();
  // Next, try adding the source (this actually tries to deploy BPF code).
  // On failure, set status and exit, but do this outside the lock for efficiency reasons.
  RETURN_IF_ERROR(AddSource(std::move(source)));
  LOG(INFO) << absl::Substitute("DynamicTrace [$0]: Deployed BPF program in $1 ms.", trace_id.str(),
                                timer.ElapsedTime_us() / 1000.0);

  stirlingpb::Publish publication;
  {
    absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
    PopulatePublishProto(&publication, info_class_mgrs_, output_name);
  }

  absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
  dynamic_trace_status_map_[trace_id] = publication;
}

void StirlingImpl::DestroyDynamicTraceConnector(sole::uuid trace_id) {
  auto timer = ElapsedTimer();
  timer.Start();

  // Remove from stirling.
  RETURN_IF_ERROR(RemoveSource(kDynTraceSourcePrefix + trace_id.str()));

  LOG(INFO) << absl::Substitute("DynamicTrace [$0]: Removed tracepoint $1 ms.", trace_id.str(),
                                timer.ElapsedTime_us() / 1000.0);

  // Remove from map.
  {
    absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
    dynamic_trace_status_map_.erase(trace_id);
  }
}

#undef RETURN_ERROR
#undef RETURN_IF_ERROR
#undef ASSIGN_OR_RETURN

void StirlingImpl::RegisterTracepoint(
    sole::uuid trace_id,
    std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> program) {
  // Temporary: Check if the target exists on this PEM, otherwise return NotFound.
  // TODO(oazizi): Need to think of a better way of doing this.
  //               Need to differentiate errors caused by the binary not being on the host vs
  //               other errors. Also should consider races with binary creation/deletion.

  if (program->has_deployment_spec()) {
    std::unique_ptr<ConnectorContext> conn_ctx = GetContext();

    if (conn_ctx == nullptr) {
      absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
      dynamic_trace_status_map_[trace_id] = error::FailedPrecondition(
          "Failed to get K8s metadata; cannot resolve K8s entity to UPID");
      return;
    }

    Status s = dynamic_tracing::ResolveTargetObjPath(conn_ctx->GetK8SMetadata(),
                                                     program->mutable_deployment_spec());
    if (!s.ok()) {
      LOG(ERROR) << s.ToString();
      absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
      // Most failures of ResolveTargetObjPath() are caused by incorrect/incomplete user input.
      // So the error message is sent back directly to the UI.
      dynamic_trace_status_map_[trace_id] = error::FailedPrecondition(
          "Target binary/UPID not found, error message: $0",
          error::IsInternal(s) ? "internal error, chat with us on Intercom" : s.ToString());
      return;
    }
  }

  // Initialize the status of this trace to pending.
  {
    absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
    dynamic_trace_status_map_[trace_id] =
        error::ResourceUnavailable("Probe deployment in progress.");
  }

  auto t =
      std::thread(&StirlingImpl::DeployDynamicTraceConnector, this, trace_id, std::move(program));
  t.detach();
}

StatusOr<stirlingpb::Publish> StirlingImpl::GetTracepointInfo(sole::uuid trace_id) {
  absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);

  auto iter = dynamic_trace_status_map_.find(trace_id);
  if (iter == dynamic_trace_status_map_.end()) {
    return error::NotFound("Tracepoint $0 not found.", trace_id.str());
  }

  StatusOr<stirlingpb::Publish> s = iter->second;
  return s;
}

Status StirlingImpl::RemoveTracepoint(sole::uuid trace_id) {
  // Change the status of this trace to pending while we delete it.
  {
    absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
    dynamic_trace_status_map_[trace_id] = error::ResourceUnavailable("Probe removal in progress.");
  }

  auto t = std::thread(&StirlingImpl::DestroyDynamicTraceConnector, this, trace_id);
  t.detach();

  return Status::OK();
}

void StirlingImpl::GetPublishProto(stirlingpb::Publish* publish_pb) {
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
  PopulatePublishProto(publish_pb, info_class_mgrs_);
}

// Main call to start the data collection.
Status StirlingImpl::RunAsThread() {
  if (data_push_callback_ == nullptr) {
    return error::Internal("No callback function is registered in Stirling. Refusing to run.");
  }

  bool prev_run_enable_ = run_enable_.exchange(true);
  if (prev_run_enable_) {
    return error::AlreadyExists("A Stirling thread is already running.");
  }

  run_thread_ = std::thread(&StirlingImpl::RunCore, this);

  return Status::OK();
}

void StirlingImpl::WaitForThreadJoin() {
  if (run_thread_.joinable()) {
    run_thread_.join();
    CHECK_EQ(running_, false);
  }
}

void StirlingImpl::WaitForStop() {
  ECHECK(!run_enable_) << "Should only be called from Stop().";

  // If Stirling is managing the thread, this should be sufficient.
  WaitForThreadJoin();

  // If Stirling is not managing the thread,
  // then wait until we're not running anymore.
  // We should have come here through Stop().
  while (running_) {
  }
}

void StirlingImpl::Run() {
  if (data_push_callback_ == nullptr) {
    LOG(ERROR) << "No callback function is registered in Stirling. Refusing to run.";
    return;
  }

  // Make sure multiple instances of Run() are not active,
  // which would be possible if the caller created multiple threads.
  bool prev_run_enable_ = run_enable_.exchange(true);
  if (prev_run_enable_) {
    LOG(ERROR) << "A Stirling thread is already running.";
    return;
  }

  RunCore();
}

namespace {

static constexpr std::chrono::milliseconds kMinSleepDuration{1};
static constexpr std::chrono::milliseconds kMaxSleepDuration{1000};

// Helper function: Figure out when to wake up next.
std::chrono::milliseconds TimeUntilNextTick(
    const absl::flat_hash_map<SourceConnector*, SourceOutput> source_output_map) {
  // The amount to sleep depends on when the earliest Source needs to be sampled again.
  // Do this to avoid burning CPU cycles unnecessarily
  auto now = px::chrono::coarse_steady_clock::now();

  // Worst case, wake-up every so often.
  // This is important if there are no subscribed info classes, to avoid sleeping eternally.
  auto wakeup_time = now + kMaxSleepDuration;
  for (const auto& [source, output] : source_output_map) {
    wakeup_time = std::min(wakeup_time, source->sampling_freq_mgr().next());
    wakeup_time = std::min(wakeup_time, source->push_freq_mgr().next());
  }

  return std::chrono::duration_cast<std::chrono::milliseconds>(wakeup_time - now);
}

void SleepForDuration(std::chrono::milliseconds sleep_duration) {
  if (sleep_duration > kMinSleepDuration) {
    std::this_thread::sleep_for(sleep_duration);
  }
}

// Returns true if any of the input tables are beyond the threshold.
bool DataExceedsThreshold(const std::vector<DataTable*>& data_tables) {
  // Data push threshold, based on percentage of buffer that is filled.
  constexpr uint32_t kDefaultOccupancyPctThreshold = 100;

  // Data push threshold, based number of records after which a push.
  constexpr uint32_t kDefaultOccupancyThreshold = 1024;

  for (const auto* data_table : data_tables) {
    if (static_cast<uint32_t>(100 * data_table->OccupancyPct()) > kDefaultOccupancyPctThreshold) {
      return true;
    }
    if (data_table->Occupancy() > kDefaultOccupancyThreshold) {
      return true;
    }
  }
  return false;
}

}  // namespace

// Main Data Collector loop.
// Poll on Data Source Through connectors, when appropriate, then go to sleep.
// Must run as a thread, so only call from Run() as a thread.
void StirlingImpl::RunCore() {
  running_ = true;

  // First initialize each info class manager with context.
  {
    absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
    std::unique_ptr<ConnectorContext> initial_context = GetContext();
    for (const auto& s : sources_) {
      s->InitContext(initial_context.get());
    }
  }
  // TODO(oazizi): We need to call InitContext on dynamic sources too. Fix.

  while (run_enable_) {
    auto sleep_duration = std::chrono::milliseconds::zero();

    // Update the context/state on each iteration.
    // Note that if no changes are present, the same pointer will be returned back.
    // TODO(oazizi): If context constructor does a lot of work (e.g. ListUPIDs()),
    //               then there might be an inefficiency here, since we don't know if
    //               mgr->SamplingRequired() will be true for any manager.
    std::unique_ptr<ConnectorContext> ctx = GetContext();

    {
      // Acquire spin lock to go through one iteration of sampling and pushing data.
      // Needed to avoid race with main thread update info_class_mgrs_ on new subscription.
      absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

      // Run through every SourceConnector and InfoClassManager being managed.
      for (auto& [source, output] : source_output_map_) {
        // Phase 1: Probe each source for its data.
        if (source->sampling_freq_mgr().Expired()) {
          source->TransferData(ctx.get(), output.data_tables);
        }
        // Phase 2: Push Data upstream.
        if (source->push_freq_mgr().Expired() || DataExceedsThreshold(output.data_tables)) {
          source->PushData(data_push_callback_, output.data_tables);
        }
      }

      // Figure out how long to sleep.
      sleep_duration = TimeUntilNextTick(source_output_map_);
    }

    SleepForDuration(sleep_duration);
  }
  running_ = false;
}

bool StirlingImpl::IsRunning() const { return running_; }

Status StirlingImpl::WaitUntilRunning(std::chrono::milliseconds timeout) const {
  const auto timeout_time = std::chrono::steady_clock::now() + timeout;

  while (!IsRunning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (std::chrono::steady_clock::now() > timeout_time) {
      break;
    }
  }

  return IsRunning() ? Status::OK() : error::Internal("Stirling failed to reach running state.");
}

void StirlingImpl::Stop() {
  run_enable_ = false;
  WaitForStop();

  // Stop all sources.
  // This is important to release any BPF resources that were acquired.
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
  for (auto& source : sources_) {
    Status s = source->Stop();

    // Forge on, because death is imminent!
    LOG_IF(ERROR, !s.ok()) << absl::Substitute("Failed to stop source connector '$0', error: $1",
                                               source->name(), s.ToString());
  }
}

void StirlingImpl::SetDebugLevel(int level) {
  // Lock not really required, but compiler is making sure we're safe.
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
  for (auto& s : sources_) {
    s->SetDebugLevel(level);
  }
}

void StirlingImpl::EnablePIDTrace(int pid) {
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
  for (auto& s : sources_) {
    s->EnablePIDTrace(pid);
  }
}

void StirlingImpl::DisablePIDTrace(int pid) {
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
  for (auto& s : sources_) {
    s->DisablePIDTrace(pid);
  }
}

std::unique_ptr<Stirling> Stirling::Create(std::unique_ptr<SourceRegistry> registry) {
  LOG(INFO) << absl::Substitute(
      "Creating Stirling, registered sources: [$0]",
      absl::StrJoin(registry->sources(), ", ",
                    [](std::string* out, const auto& v) { absl::StrAppend(out, v.first); }));

  auto stirling = std::unique_ptr<StirlingImpl>(new StirlingImpl(std::move(registry)));

  PL_CHECK_OK(stirling->Init());

  return stirling;
}

}  // namespace stirling
}  // namespace px
