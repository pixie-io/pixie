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
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/pid_runtime/pid_runtime_connector.h"
#include "src/stirling/source_connectors/proc_stat/proc_stat_connector.h"
#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/system_stats/system_stats_connector.h"

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dynamic_tracer.h"

namespace px {
namespace stirling {

namespace {

// All sources, include experimental and deprecated ones.
std::unique_ptr<SourceRegistry> CreateAllSourceRegistry() {
  auto registry = std::make_unique<SourceRegistry>();
  registry->RegisterOrDie<JVMStatsConnector>("jvm_stats");
  registry->RegisterOrDie<PIDRuntimeConnector>("bcc_cpu_stat");
  registry->RegisterOrDie<ProcStatConnector>("proc_stat");
  registry->RegisterOrDie<SeqGenConnector>("sequences");
  registry->RegisterOrDie<SocketTraceConnector>("socket_tracer");
  registry->RegisterOrDie<SystemStatsConnector>("system_stats");
  registry->RegisterOrDie<PerfProfileConnector>("perf_profiler");
  return registry;
}

// All sources used in production.
std::unique_ptr<SourceRegistry> CreateProdSourceRegistry() {
  auto registry = std::make_unique<SourceRegistry>();
  registry->RegisterOrDie<JVMStatsConnector>("jvm_stats");
  registry->RegisterOrDie<SocketTraceConnector>("socket_tracer");
  registry->RegisterOrDie<SystemStatsConnector>("system_stats");
  registry->RegisterOrDie<PerfProfileConnector>("perf_profiler");
  return registry;
}

// All sources used in production, that produce traces.
std::unique_ptr<SourceRegistry> CreateTracerSourceRegistry() {
  auto registry = std::make_unique<SourceRegistry>();
  registry->RegisterOrDie<SocketTraceConnector>("socket_tracer");
  return registry;
}

// All sources used in production, that produce metrics.
std::unique_ptr<SourceRegistry> CreateMetricsSourceRegistry() {
  auto registry = std::make_unique<SourceRegistry>();
  registry->RegisterOrDie<JVMStatsConnector>("jvm_stats");
  registry->RegisterOrDie<SystemStatsConnector>("system_stats");
  return registry;
}

// The stack trace profiler.
std::unique_ptr<SourceRegistry> CreatePerfProfilerRegistry() {
  auto registry = std::make_unique<SourceRegistry>();
  registry->RegisterOrDie<PerfProfileConnector>("perf_profiler");
  return registry;
}
}  // namespace

std::unique_ptr<SourceRegistry> CreateSourceRegistry(SourceRegistrySpecifier sources) {
  switch (sources) {
    case SourceRegistrySpecifier::kNone:
      return std::make_unique<SourceRegistry>();
    case SourceRegistrySpecifier::kTracers:
      return px::stirling::CreateTracerSourceRegistry();
    case SourceRegistrySpecifier::kMetrics:
      return px::stirling::CreateMetricsSourceRegistry();
    case SourceRegistrySpecifier::kProd:
      return px::stirling::CreateProdSourceRegistry();
    case SourceRegistrySpecifier::kAll:
      return px::stirling::CreateAllSourceRegistry();
    case SourceRegistrySpecifier::kProfiler:
      return px::stirling::CreatePerfProfilerRegistry();
    default:
      // To keep GCC happy.
      DCHECK(false);
      return nullptr;
  }
}

class StirlingImpl final : public Stirling {
 public:
  explicit StirlingImpl(std::unique_ptr<SourceRegistry> registry);

  ~StirlingImpl() override;

  void RegisterUserDebugSignalHandlers() override;

  // TODO(oazizi/yzhao): Consider lift this as an interface method into Stirling, making it
  // symmetric with Stop().
  Status Init();

  void RegisterTracepoint(
      sole::uuid uuid,
      std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> program) override;
  StatusOr<stirlingpb::Publish> GetTracepointInfo(sole::uuid trace_id) override;
  Status RemoveTracepoint(sole::uuid trace_id) override;
  void GetPublishProto(stirlingpb::Publish* publish_pb) override;
  Status SetSubscription(const stirlingpb::Subscribe& subscribe_proto) override;
  void RegisterDataPushCallback(DataPushCallback f) override { data_push_callback_ = f; }
  void RegisterAgentMetadataCallback(AgentMetadataCallback f) override {
    DCHECK(f != nullptr);
    agent_metadata_callback_ = f;
  }
  std::unique_ptr<ConnectorContext> GetContext();

  void Run() override;
  Status RunAsThread() override;
  bool IsRunning() const override;
  void Stop() override;
  void WaitForThreadJoin() override;

  void ToggleDebug();
  void EnablePIDTrace(int pid);
  void DisablePIDTrace(int pid);

 private:
  // Create data source connectors from the registered sources.
  Status CreateSourceConnectors();

  // Adds a source to Stirling, and updates all state accordingly.
  Status AddSource(std::unique_ptr<SourceConnector> source, bool dynamic = false);

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

  // Holds InfoClassManager and DataTable.
  struct SourceOutput {
    std::vector<InfoClassManager*> info_class_mgrs;
    std::vector<DataTable*> data_tables;
  };
  // TODO(yzhao): Move InfoClassManager objects into SourceConnector, and remove this map.
  absl::flat_hash_map<SourceConnector*, SourceOutput> source_output_map_
      ABSL_GUARDED_BY(info_class_mgrs_lock_);

  std::vector<std::unique_ptr<DataTable>> tables_;

  InfoClassManagerVec info_class_mgrs_ ABSL_GUARDED_BY(info_class_mgrs_lock_);

  // Lock to protect both info_class_mgrs_ and sources_.
  absl::base_internal::SpinLock info_class_mgrs_lock_;

  std::unique_ptr<PubSubManager> config_;

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

  int debug_level_ = 0;
};

StirlingImpl::StirlingImpl(std::unique_ptr<SourceRegistry> registry)
    : config_(std::make_unique<PubSubManager>()), registry_(std::move(registry)) {
  LOG(INFO) << "Creating Stirling";

  std::string msg = "Stirling: Registered sources: [ ";
  for (const auto& registered_source : registry_->sources()) {
    absl::StrAppend(&msg, registered_source.first, " ");
  }
  absl::StrAppend(&msg, "]");
  LOG(INFO) << msg;
}

StirlingImpl::~StirlingImpl() { Stop(); }

StirlingImpl* g_stirling_ptr = nullptr;

// Turn on/off the debug flag for all of Stirling.
void UserSignalHandler(int /* signum */) {
  if (g_stirling_ptr == nullptr) {
    return;
  }

  g_stirling_ptr->ToggleDebug();
}

// Set flags in the Socket Tracer to start tracing the specified PID.
void UserSignalHandler2(int /* signum */, siginfo_t* info, void* /* context */) {
  if (g_stirling_ptr == nullptr) {
    return;
  }

  int pid = info->si_int;
  if (pid >= 0) {
    LOG(INFO) << absl::Substitute("Enabling tracing of PID: $0", pid);
    g_stirling_ptr->EnablePIDTrace(pid);
  } else {
    LOG(INFO) << absl::Substitute("Disabling tracing of PID: $0", -1 * pid);
    g_stirling_ptr->DisablePIDTrace(pid);
  }
}

void StirlingImpl::RegisterUserDebugSignalHandlers() {
  g_stirling_ptr = this;

  // Signal for USR1: Set-up a general debug signal that gets broadcast to all source connectors.
  // Source connectors can enable logging of debug information as desired.
  // Trigger this via `kill -USR1`
  signal(SIGUSR1, UserSignalHandler);

  // Signal for USR2: This is a PID-based signal that currently sets flags in the Socket Tracer,
  // to enable connection tracing for the particular PID.
  // This uses sigaction() instead of signal() because it needs to accept an integer.
  // Note that `kill -USR2` will no longer work for this signal. Instead sigqueue must be used
  // to send the signal.
  struct sigaction sigaction_specs = {};
  sigaction_specs.sa_sigaction = UserSignalHandler2;
  sigaction_specs.sa_flags = SA_SIGINFO;
  sigemptyset(&sigaction_specs.sa_mask);
  sigaction(SIGUSR2, &sigaction_specs, NULL);
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
  PL_RETURN_IF_ERROR(CreateSourceConnectors());
  return Status::OK();
}

Status StirlingImpl::CreateSourceConnectors() {
  if (!registry_) {
    return error::NotFound("Source registry doesn't exist");
  }
  auto sources = registry_->sources();
  for (const auto& [name, registry_element] : sources) {
    Status s = AddSource(registry_element.create_source_fn(name));
    LOG_IF(DFATAL, !s.ok()) << absl::Substitute(
        "Source Connector (registry name=$0) not instantiated, error: $1", name, s.ToString());
  }
  return Status::OK();
}

std::unique_ptr<ConnectorContext> StirlingImpl::GetContext() {
  if (agent_metadata_callback_ != nullptr) {
    return std::unique_ptr<ConnectorContext>(new AgentContext(agent_metadata_callback_()));
  }
  return std::unique_ptr<ConnectorContext>(new StandaloneContext());
}

Status StirlingImpl::AddSource(std::unique_ptr<SourceConnector> source, bool dynamic) {
  // Step 1: Init the source.
  PL_RETURN_IF_ERROR(source->Init());

  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

  std::vector<InfoClassManager*> mgrs;
  mgrs.reserve(source->num_tables());

  for (uint32_t i = 0; i < source->num_tables(); ++i) {
    const DataTableSchema& schema = source->TableSchema(i);
    LOG(INFO) << "Adding info class " << schema.name();

    // Step 2: Create the info class manager.
    auto mgr = std::make_unique<InfoClassManager>(
        schema, dynamic ? stirlingpb::DYNAMIC : stirlingpb::STATIC);
    mgr->SetSourceConnector(source.get(), i);

    // Step 3: Setup the manager.
    mgr->SetSamplingPeriod(schema.default_sampling_period());
    mgr->SetPushPeriod(schema.default_push_period());

    mgrs.push_back(mgr.get());

    // Step 4: Keep pointers to all the objects
    info_class_mgrs_.push_back(std::move(mgr));
  }

  source_output_map_[source.get()] = {std::move(mgrs),
                                      // DataTable objects are created after subscribing.
                                      {}};
  sources_.push_back(std::move(source));

  return Status::OK();
}

Status StirlingImpl::RemoveSource(std::string_view source_name) {
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

  // Find the source.
  auto source_iter = std::find_if(sources_.begin(), sources_.end(),
                                  [&source_name](const std::unique_ptr<SourceConnector>& s) {
                                    return s->source_name() == source_name;
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

  LOG(INFO) << absl::Substitute("DynamicTraceConnector [$0] created in $1 ms.",
                                source->source_name(), timer.ElapsedTime_us() / 1000.0);

  // Cache table schema name as source will be moved below.
  std::string output_name(source->TableSchema(0).name());

  timer.Start();
  // Next, try adding the source (this actually tries to deploy BPF code).
  // On failure, set status and exit, but do this outside the lock for efficiency reasons.
  RETURN_IF_ERROR(AddSource(std::move(source), /*dynamic*/ true));
  LOG(INFO) << absl::Substitute("DynamicTrace [$0]: Deployed BPF program in $1 ms.", trace_id.str(),
                                timer.ElapsedTime_us() / 1000.0);

  stirlingpb::Publish publication;
  {
    absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
    config_->PopulatePublishProto(&publication, info_class_mgrs_, output_name);
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
  config_->PopulatePublishProto(publish_pb, info_class_mgrs_);
}

// Assumes info_class_mgrs are ordered by source_table_num, which was guaranteed in AddSource().
std::vector<DataTable*> GetDataTables(const std::vector<InfoClassManager*>& info_class_mgrs) {
  std::vector<DataTable*> data_tables;
  data_tables.reserve(info_class_mgrs.size());

  for (InfoClassManager* mgr : info_class_mgrs) {
    if (mgr->subscribed()) {
      data_tables.push_back(mgr->data_table());
    } else {
      data_tables.push_back(nullptr);
    }
  }
  return data_tables;
}

Status StirlingImpl::SetSubscription(const stirlingpb::Subscribe& subscribe_proto) {
  // Acquire lock to update info_class_mgrs_.
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

  // Last append before clearing tables from old subscriptions.
  for (const auto& mgr : info_class_mgrs_) {
    if (mgr->subscribed()) {
      mgr->PushData(data_push_callback_);
    }
  }
  tables_.clear();

  // Update schemas based on the subscribe_proto.
  PL_CHECK_OK(config_->UpdateSchemaFromSubscribe(subscribe_proto, info_class_mgrs_));

  // Generate the tables required based on subscribed Info Classes.
  for (const auto& mgr : info_class_mgrs_) {
    if (mgr->subscribed()) {
      auto data_table = std::make_unique<DataTable>(mgr->Schema());
      mgr->SetDataTable(data_table.get());
      tables_.push_back(std::move(data_table));
    }
  }

  // Update mapping from SourceConnector to DataTable objects.
  for (auto& [source, source_output] : source_output_map_) {
    source_output.data_tables = GetDataTables(source_output.info_class_mgrs);
  }

  return Status::OK();
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
std::chrono::milliseconds TimeUntilNextTick(const InfoClassManagerVec& info_class_mgrs) {
  // The amount to sleep depends on when the earliest Source needs to be sampled again.
  // Do this to avoid burning CPU cycles unnecessarily

  auto now = std::chrono::steady_clock::now();

  // Worst case, wake-up every so often.
  // This is important if there are no subscribed info classes, to avoid sleeping eternally.
  auto wakeup_time = now + kMaxSleepDuration;

  for (const auto& mgr : info_class_mgrs) {
    if (mgr->subscribed()) {
      wakeup_time = std::min(wakeup_time, mgr->NextPushTime());
      const SourceConnector* source = mgr->source();
      if (source->output_multi_tables()) {
        // Note that the same SourceConnector could be examined multiple times for the associated
        // InfoClassManager objects, but that does not affect the result.
        wakeup_time = std::min(wakeup_time, source->sample_push_mgr().NextSamplingTime());
      } else {
        // If the SourceConnector can output to multiple data tables, then the sampling frequency is
        // managed by the SourceConnector, not InfoClassManager.
        wakeup_time = std::min(wakeup_time, mgr->NextSamplingTime());
      }
    }
  }

  return std::chrono::duration_cast<std::chrono::milliseconds>(wakeup_time - now);
}

void SleepForDuration(std::chrono::milliseconds sleep_duration) {
  if (sleep_duration > kMinSleepDuration) {
    std::this_thread::sleep_for(sleep_duration);
  }
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
        if (source->output_multi_tables()) {
          if (source->sample_push_mgr().SamplingRequired()) {
            source->TransferData(ctx.get(), output.data_tables);
          }
        } else {
          // TODO(yzhao): Reduce sampling periods if we are dropping data.
          for (const auto& mgr : output.info_class_mgrs) {
            if (mgr->subscribed() && mgr->SamplingRequired()) {
              mgr->SampleData(ctx.get());
            }
          }
        }

        // Phase 2: Push Data upstream.
        for (auto* mgr : output.info_class_mgrs) {
          if (mgr->subscribed() && mgr->PushRequired()) {
            mgr->PushData(data_push_callback_);
          }
        }
      }

      // Figure out how long to sleep.
      sleep_duration = TimeUntilNextTick(info_class_mgrs_);
    }

    SleepForDuration(sleep_duration);
  }
  running_ = false;
}

bool StirlingImpl::IsRunning() const { return run_enable_; }

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
                                               source->source_name(), s.ToString());
  }
}

void StirlingImpl::ToggleDebug() {
  debug_level_ = (debug_level_ + 1) % 2;

  // Lock not really required, but compiler is making sure we're safe.
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
  for (auto& s : sources_) {
    s->SetDebugLevel(debug_level_);
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
  // Create Stirling object.
  auto stirling = std::unique_ptr<StirlingImpl>(new StirlingImpl(std::move(registry)));

  // Initialize Stirling (brings-up all source connectors).
  PL_CHECK_OK(stirling->Init());

  return stirling;
}

}  // namespace stirling
}  // namespace px
