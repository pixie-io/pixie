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
    if (sub_info_class->name() == name) {
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

  int64_t RegisterDynamicTrace(
      std::unique_ptr<dynamic_tracing::ir::logical::Program> program) override;
  Status CheckDynamicTraceStatus(uint64_t trace_id) override;
  void GetPublishProto(stirlingpb::Publish* publish_pb) override;
  Status SetSubscription(const stirlingpb::Subscribe& subscribe_proto) override;
  void RegisterDataPushCallback(DataPushCallback f) override { data_push_callback_ = f; }
  void RegisterAgentMetadataCallback(AgentMetadataCallback f) override {
    DCHECK(f != nullptr);
    agent_metadata_callback_ = f;
  }
  std::unique_ptr<ConnectorContext> GetContext();

  std::unordered_map<uint64_t, std::string> TableIDToNameMap() const override;
  void Run() override;
  Status RunAsThread() override;
  void Stop() override;
  void WaitForThreadJoin() override;

 private:
  // Create data source connectors from the registered sources.
  Status CreateSourceConnectors();

  // Adds a source to Stirling, and updates all state accordingly.
  Status AddSource(std::unique_ptr<SourceConnector> source);

  // Creates and deploys dynamic tracing source.
  void DeployDynamicTraceConnector(int64_t trace_id,
                                   std::unique_ptr<dynamic_tracing::ir::logical::Program> program);

  // Main run implementation.
  void RunCore();

  // Wait for Stirling to stop its main loop.
  void WaitForStop();

  // Helper function to figure out how much to sleep between polling iterations.
  std::chrono::milliseconds TimeUntilNextTick();

  // Sleeps for the specified duration, as long as it is above some threshold.
  void SleepForDuration(std::chrono::milliseconds sleep_duration);

  // Main thread used to spawn off RunThread().
  std::thread run_thread_;

  std::atomic<bool> run_enable_ = false;
  std::atomic<bool> running_ = false;
  std::vector<std::unique_ptr<SourceConnector>> sources_;
  std::vector<std::unique_ptr<DataTable>> tables_;

  InfoClassManagerVec info_class_mgrs_;

  // Lock to protect both info_class_mgrs_ and sources_.
  absl::base_internal::SpinLock info_class_mgrs_lock_;

  std::unique_ptr<PubSubManager> config_;

  std::unique_ptr<SourceRegistry> registry_;

  static constexpr std::chrono::milliseconds kMinSleepDuration{1};
  static constexpr std::chrono::milliseconds kMaxSleepDuration{1000};

  /**
   * Function to call to push data to the agent.
   * Function signature is:
   *   uint64_t table_id
   *   std::unique_ptr<ColumnWrapperRecordBatch> data
   */
  DataPushCallback data_push_callback_ = nullptr;

  AgentMetadataCallback agent_metadata_callback_ = nullptr;
  AgentMetadataType agent_metadata_;

  // The index can be assigned to the next registered probe.
  int64_t dynamic_trace_index_ = 0;

  absl::flat_hash_map<int64_t, Status> dynamic_trace_status_map_;
  absl::base_internal::SpinLock dynamic_trace_status_map_lock_;
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

std::unordered_map<uint64_t, std::string> StirlingImpl::TableIDToNameMap() const {
  std::unordered_map<uint64_t, std::string> map;

  for (auto& mgr : info_class_mgrs_) {
    map.insert({mgr->id(), std::string(mgr->name())});
  }

  return map;
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
    auto mgr_ptr = mgr.get();
    mgr->SetSourceConnector(source.get(), i);

    // Step 3: Setup the manager.
    mgr_ptr->SetSamplingPeriod(schema.default_sampling_period());
    mgr_ptr->SetPushPeriod(schema.default_push_period());

    // Step 4: Keep pointers to all the objects
    info_class_mgrs_.push_back(std::move(mgr));
  }

  sources_.push_back(std::move(source));

  return Status::OK();
}

void StirlingImpl::DeployDynamicTraceConnector(
    int64_t trace_id, std::unique_ptr<dynamic_tracing::ir::logical::Program> program) {
  auto timer = ElapsedTimer();
  timer.Start();

  // Try creating the DynamicTraceConnector--which compiles BCC code.
  // On failure, set status and exit.
  PL_ASSIGN_OR(std::unique_ptr<SourceConnector> source, DynamicTraceConnector::Create(*program),
               absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
               dynamic_trace_status_map_[trace_id] = __s__.status(); return;);

  LOG(INFO) << absl::Substitute("DynamicTrace: Program compiled to BCC code in $0 ms.",
                                timer.ElapsedTime_us() / 1000.0);

  timer.Start();
  // Next, try adding the source (this actually tries to deploy BPF code).
  // On failure, set status and exit, but do this outside the lock for efficiency reasons.
  Status s = AddSource(std::move(source));
  LOG_IF(INFO, s.ok()) << absl::Substitute("DynamicTrace: Deployed BCC code in $0 ms.",
                                           timer.ElapsedTime_us() / 1000.0);

  absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
  dynamic_trace_status_map_[trace_id] = s;
}

int64_t StirlingImpl::RegisterDynamicTrace(
    std::unique_ptr<dynamic_tracing::ir::logical::Program> program) {
  int64_t trace_id = dynamic_trace_index_++;

  // Initialize the status of this trace to pending.
  {
    absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);
    dynamic_trace_status_map_[trace_id] =
        error::ResourceUnavailable("Probe deployment in progress.");
  }

  auto t =
      std::thread(&StirlingImpl::DeployDynamicTraceConnector, this, trace_id, std::move(program));
  t.detach();

  return trace_id;
}

#undef ASSIGN_OR_RETURN

Status StirlingImpl::CheckDynamicTraceStatus(uint64_t trace_id) {
  absl::base_internal::SpinLockHolder lock(&dynamic_trace_status_map_lock_);

  auto iter = dynamic_trace_status_map_.find(trace_id);
  if (iter == dynamic_trace_status_map_.end()) {
    return error::NotFound("");
  }

  Status s = iter->second;

  // Destructive read of tracer state on errors, so that we don't leak the map.
  // Only error that is excluded is RESOURCE_UNAVAILABLE, because that means
  // the final state is not known and the caller should retry.
  if (!s.ok() && s.code() != pl::statuspb::Code::RESOURCE_UNAVAILABLE) {
    dynamic_trace_status_map_.erase(iter);
  }

  return s;
}

void StirlingImpl::GetPublishProto(stirlingpb::Publish* publish_pb) {
  config_->GeneratePublishProto(publish_pb, info_class_mgrs_);
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
      sleep_duration = TimeUntilNextTick();
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
  for (auto& source : sources_) {
    Status s = source->Stop();

    // Forge on, because death is imminent!
    LOG_IF(ERROR, !s.ok()) << absl::Substitute("Failed to stop source connector '$0', error: $1",
                                               source->source_name(), s.ToString());
  }
}

// Helper function: Figure out when to wake up next.
std::chrono::milliseconds StirlingImpl::TimeUntilNextTick() {
  // The amount to sleep depends on when the earliest Source needs to be sampled again.
  // Do this to avoid burning CPU cycles unnecessarily

  auto now = std::chrono::steady_clock::now();

  // Worst case, wake-up every so often.
  // This is important if there are no subscribed info classes, to avoid sleeping eternally.
  auto wakeup_time = now + kMaxSleepDuration;

  for (const auto& mgr : info_class_mgrs_) {
    // TODO(oazizi): Make implementation of NextPushTime/NextSamplingTime low cost.
    if (mgr->subscribed()) {
      wakeup_time = std::min(wakeup_time, mgr->NextPushTime());
      wakeup_time = std::min(wakeup_time, mgr->NextSamplingTime());
    }
  }

  return std::chrono::duration_cast<std::chrono::milliseconds>(wakeup_time - now);
}

void StirlingImpl::SleepForDuration(std::chrono::milliseconds sleep_duration) {
  if (sleep_duration > kMinSleepDuration) {
    std::this_thread::sleep_for(sleep_duration);
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
