#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/base/internal/spinlock.h>

#include "src/common/base/base.h"
#include "src/common/perf/elapsed_timer.h"
#include "src/common/system/system_info.h"

#include "src/stirling/bpf_tools/probe_cleaner.h"
#include "src/stirling/data_table.h"
#include "src/stirling/obj_tools/proc_path_tools.h"
#include "src/stirling/proto/stirling.pb.h"
#include "src/stirling/pub_sub_manager.h"
#include "src/stirling/source_connector.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"

#include "src/stirling/dynamic_trace_connector.h"
#include "src/stirling/jvm_stats_connector.h"
#include "src/stirling/pid_runtime_connector.h"
#include "src/stirling/proc_stat_connector.h"
#include "src/stirling/seq_gen_connector.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/system_stats_connector.h"

#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

namespace pl {
namespace stirling {

namespace {

// All sources, include experimental and deprecated ones.
std::unique_ptr<SourceRegistry> CreateAllSourceRegistry() {
  auto registry = std::make_unique<SourceRegistry>();
  registry->RegisterOrDie<JVMStatsConnector>("jvm_stats");
  registry->RegisterOrDie<FakeProcStatConnector>("fake_proc_stat");
  registry->RegisterOrDie<PIDRuntimeConnector>("bcc_cpu_stat");
  registry->RegisterOrDie<ProcStatConnector>("proc_stat");
  registry->RegisterOrDie<SeqGenConnector>("sequences");
  registry->RegisterOrDie<SocketTraceConnector>("socket_tracer");
  registry->RegisterOrDie<SystemStatsConnector>("system_stats");
  return registry;
}

// All sources used in production.
std::unique_ptr<SourceRegistry> CreateProdSourceRegistry() {
  auto registry = std::make_unique<SourceRegistry>();
  registry->RegisterOrDie<JVMStatsConnector>("jvm_stats");
  registry->RegisterOrDie<SocketTraceConnector>("socket_tracer");
  registry->RegisterOrDie<SystemStatsConnector>("system_stats");
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
}  // namespace

std::unique_ptr<SourceRegistry> CreateSourceRegistry(SourceRegistrySpecifier sources) {
  switch (sources) {
    case SourceRegistrySpecifier::kTracers:
      return pl::stirling::CreateTracerSourceRegistry();
    case SourceRegistrySpecifier::kMetrics:
      return pl::stirling::CreateMetricsSourceRegistry();
    case SourceRegistrySpecifier::kProd:
      return pl::stirling::CreateProdSourceRegistry();
    case SourceRegistrySpecifier::kAll:
      return pl::stirling::CreateAllSourceRegistry();
    default:
      // To keep GCC happy.
      DCHECK(false);
      return nullptr;
  }
}

// TODO(oazizi/kgandhi): Is there a better place for this function?
stirlingpb::Subscribe SubscribeToAllInfoClasses(const stirlingpb::Publish& publish_proto) {
  stirlingpb::Subscribe subscribe_proto;

  for (const auto& info_class : publish_proto.published_info_classes()) {
    auto sub_info_class = subscribe_proto.add_subscribed_info_classes();
    sub_info_class->MergeFrom(info_class);
    sub_info_class->set_subscribed(true);
  }
  return subscribe_proto;
}

stirlingpb::Subscribe SubscribeToInfoClass(const stirlingpb::Publish& publish_proto,
                                           std::string_view name) {
  stirlingpb::Subscribe subscribe_proto;

  for (const auto& info_class : publish_proto.published_info_classes()) {
    auto sub_info_class = subscribe_proto.add_subscribed_info_classes();
    sub_info_class->CopyFrom(info_class);
    if (sub_info_class->schema().name() == name) {
      sub_info_class->set_subscribed(true);
    }
  }
  return subscribe_proto;
}

class StirlingImpl final : public Stirling {
 public:
  explicit StirlingImpl(std::unique_ptr<SourceRegistry> registry);

  ~StirlingImpl() override;

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
  void Stop() override;
  void WaitForThreadJoin() override;

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

  static inline const std::string kDynTraceSourcePrefix = "DT_";
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

Status StirlingImpl::Init() {
  system::LogSystemInfo();

  // Clean up any probes from a previous instance.
  static constexpr char kPixieBPFProbeMarker[] = "__pixie__";
  Status s = utils::CleanProbes(kPixieBPFProbeMarker);
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

    if (!s.ok()) {
      LOG(WARNING) << absl::Substitute("Source Connector (registry name=$0) not instantiated",
                                       name);
      LOG(WARNING) << s.status().ToString();
    }
  }
  return Status::OK();
}

std::unique_ptr<ConnectorContext> StirlingImpl::GetContext() {
  if (agent_metadata_callback_ != nullptr) {
    return std::unique_ptr<ConnectorContext>(new AgentContext(agent_metadata_callback_()));
  }
  return std::unique_ptr<ConnectorContext>(new StandaloneContext());
}

Status StirlingImpl::AddSource(std::unique_ptr<SourceConnector> source) {
  // Step 1: Init the source.
  PL_RETURN_IF_ERROR(source->Init());

  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

  for (uint32_t i = 0; i < source->num_tables(); ++i) {
    const DataTableSchema& schema = source->TableSchema(i);
    LOG(INFO) << "Adding info class " << schema.name();

    // Step 2: Create the info class manager.
    auto mgr = std::make_unique<InfoClassManager>(schema);
    mgr->SetSourceConnector(source.get(), i);

    // Step 3: Setup the manager.
    mgr->SetSamplingPeriod(schema.default_sampling_period());
    mgr->SetPushPeriod(schema.default_push_period());

    // Step 4: Keep pointers to all the objects
    info_class_mgrs_.push_back(std::move(mgr));
  }

  sources_.push_back(std::move(source));

  return Status::OK();
}

// TODO(oazizi): Consider a data structure more friendly to erase().
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
    Status ret_status(pl::statuspb::Code::INTERNAL, s.msg());                  \
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

void StirlingImpl::DeployDynamicTraceConnector(
    sole::uuid trace_id,
    std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> program) {
  if (program->tracepoints_size() == 0) {
    RETURN_ERROR(error::Internal("Dynamic trace must define at least one Tracepoint."));
  }
  if (program->tracepoints_size() > 1) {
    RETURN_ERROR(error::Internal("Only one Trancepoint is currently supported."));
  }

  auto timer = ElapsedTimer();
  timer.Start();

  // Try creating the DynamicTraceConnector--which compiles BCC code.
  // On failure, set status and exit.
  ASSIGN_OR_RETURN_ERROR(
      std::unique_ptr<SourceConnector> source,
      DynamicTraceConnector::Create(kDynTraceSourcePrefix + trace_id.str(), program.get()));

  LOG(INFO) << absl::Substitute("DynamicTrace [$0]: Program compiled to BCC code in $1 ms.",
                                trace_id.str(), timer.ElapsedTime_us() / 1000.0);

  timer.Start();
  // Next, try adding the source (this actually tries to deploy BPF code).
  // On failure, set status and exit, but do this outside the lock for efficiency reasons.
  RETURN_IF_ERROR(AddSource(std::move(source)));
  LOG(INFO) << absl::Substitute("DynamicTrace [$0]: Deployed BCC code in $1 ms.", trace_id.str(),
                                timer.ElapsedTime_us() / 1000.0);

  stirlingpb::Publish publication;
  {
    absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);
    for (const auto& output : program->tracepoints(0).program().outputs()) {
      config_->PopulatePublishProto(&publication, info_class_mgrs_, output.name());
    }
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
  Status s = dynamic_tracing::ResolveTargetObjPath(program->mutable_deployment_spec());
  if (!s.ok()) {
    LOG(ERROR) << s.ToString();
    absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
    dynamic_trace_status_map_[trace_id] =
        error::FailedPrecondition("Target binary/UPID not found: '$0'", s.ToString());
    return;
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
    // TODO(oazizi): Make implementation of NextPushTime/NextSamplingTime low cost.
    if (mgr->subscribed()) {
      wakeup_time = std::min(wakeup_time, mgr->NextPushTime());
      wakeup_time = std::min(wakeup_time, mgr->NextSamplingTime());
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

    {
      // Update the context/state on each iteration.
      // Note that if no changes are present, the same pointer will be returned back.
      // TODO(oazizi): If context constructor does a lot of work (e.g. ListUPIDs()),
      //               then there might be an inefficiency here, since we don't know if
      //               mgr->SamplingRequired() will be true for any manager.
      std::unique_ptr<ConnectorContext> ctx = GetContext();

      // Acquire spin lock to go through one iteration of sampling and pushing data.
      // Needed to avoid race with main thread update info_class_mgrs_ on new subscription.
      absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

      // Run through every InfoClass being managed.
      for (const auto& mgr : info_class_mgrs_) {
        if (mgr->subscribed()) {
          // Phase 1: Probe each source for its data.
          if (mgr->SamplingRequired()) {
            mgr->SampleData(ctx.get());
          }

          // Phase 2: Push Data upstream.
          if (mgr->PushRequired()) {
            mgr->PushData(data_push_callback_);
          }

          // Optional: Update sampling periods if we are dropping data.
        }
      }

      // Figure out how long to sleep.
      sleep_duration = TimeUntilNextTick(info_class_mgrs_);
    }

    SleepForDuration(sleep_duration);
  }
  running_ = false;
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
                                               source->source_name(), s.ToString());
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
}  // namespace pl
