#include <algorithm>
#include <chrono>
#include <utility>

#include "src/common/common.h"
#include "src/stirling/bpftrace_connector.h"
#include "src/stirling/pub_sub_manager.h"
#include "src/stirling/source_connector.h"
#include "src/stirling/stirling.h"

namespace pl {
namespace stirling {

// TODO(oazizi/kgandhi): Is there a better place for this function?
stirlingpb::Subscribe SubscribeToAllInfoClasses(const stirlingpb::Publish& publish_proto) {
  stirlingpb::Subscribe subscribe_proto;

  for (int i = 0; i < publish_proto.published_info_classes_size(); ++i) {
    auto sub_info_class = subscribe_proto.add_subscribed_info_classes();
    sub_info_class->MergeFrom(publish_proto.published_info_classes(i));
    sub_info_class->set_subscribed(true);
  }
  return subscribe_proto;
}

Status Stirling::Init() {
  PL_RETURN_IF_ERROR(CreateSourceConnectors());
  return Status::OK();
}

Status Stirling::CreateSourceConnectors() {
  if (!registry_) {
    return error::NotFound("Source registry doesn't exist");
  }
  auto sources = registry_->sources();
  for (const auto& [name, registry_element] : sources) {
    Status s = AddSourceFromRegistry(name, registry_element);

    if (!s.ok()) {
      LOG(WARNING) << absl::StrFormat("Source Connector (registry name=%s) not instantiated", name);
      LOG(WARNING) << s.status().ToString();
    }
  }
  return Status::OK();
}

Status Stirling::AddSourceFromRegistry(const std::string& name,
                                       SourceRegistry::RegistryElement registry_element) {
  // Step 1: Create and init the source.
  auto source = registry_element.create_source_fn(name);
  PL_RETURN_IF_ERROR(source->Init());

  // Step 2: Create the info class manager.
  // TODO(oazizi): What if a Source has multiple InfoClasses?
  auto mgr = std::make_unique<InfoClassManager>(name);
  auto mgr_ptr = mgr.get();
  mgr->SetSourceConnector(source.get());

  // Step 3: Setup the manager.
  PL_CHECK_OK(mgr_ptr->PopulateSchemaFromSource());
  mgr_ptr->SetSamplingPeriod(registry_element.sampling_period);
  mgr_ptr->SetPushPeriod(registry_element.push_period);

  // Step 4: Keep pointers to all the objects
  sources_.push_back(std::move(source));
  info_class_mgrs_.push_back(std::move(mgr));

  return Status::OK();
}

void Stirling::GetPublishProto(stirlingpb::Publish* publish_pb) {
  config_->GeneratePublishProto(publish_pb, info_class_mgrs_);
}

Status Stirling::SetSubscription(const stirlingpb::Subscribe& subscribe_proto) {
  // Acquire lock to update info_class_mgrs_.
  absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

  // Last append before clearing tables from old subscriptions.
  for (const auto& mgr : info_class_mgrs_) {
    if (mgr->subscribed()) {
      PL_CHECK_OK(mgr->PushData(agent_callback_));
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
      // TODO(kgandhi): PL-426
      // Set sampling frequency based on input from Vizer.
      tables_.push_back(std::move(data_table));
    }
  }

  return Status::OK();
}

// Main call to start the data collection.
Status Stirling::RunAsThread() {
  bool prev_run_enable_ = run_enable_.exchange(true);

  if (prev_run_enable_) {
    return error::AlreadyExists("A Stirling thread is already running.");
  }

  run_thread_ = std::thread(&Stirling::RunCore, this);

  return Status::OK();
}

void Stirling::WaitForThreadJoin() { run_thread_.join(); }

void Stirling::Run() {
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
void Stirling::RunCore() {
  while (run_enable_) {
    {
      // Acquire spin lock to go through one iteration of sampling and pushing data.
      // Needed to avoid race with main thread update info_class_mgrs_ on new subscription.
      absl::base_internal::SpinLockHolder lock(&info_class_mgrs_lock_);

      // Run through every InfoClass being managed.
      for (const auto& mgr : info_class_mgrs_) {
        if (mgr->subscribed()) {
          // Phase 1: Probe each source for its data.
          if (mgr->SamplingRequired()) {
            PL_CHECK_OK(mgr->SampleData());
          }

          // Phase 2: Push Data upstream.
          if (mgr->PushRequired()) {
            PL_CHECK_OK(mgr->PushData(agent_callback_));
          }

          // Optional: Update sampling periods if we are dropping data.
        }
      }
    }
    // Figure out how long to sleep.
    SleepUntilNextTick();
  }
}

void Stirling::Stop() { run_enable_ = false; }

// Helper function: Figure out when to wake up next.
void Stirling::SleepUntilNextTick() {
  // The amount to sleep depends on when the earliest Source needs to be sampled again.
  // Do this to avoid burning CPU cycles unnecessarily

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now().time_since_epoch());

  auto wakeup_time = std::chrono::milliseconds::max();

  for (const auto& mgr : info_class_mgrs_) {
    // TODO(oazizi): Make implementation of NextPushTime/NextSamplingTime low cost.
    wakeup_time = std::min(wakeup_time, mgr->NextPushTime());
    wakeup_time = std::min(wakeup_time, mgr->NextSamplingTime());
  }

  auto sleep_duration = wakeup_time - now;

  if (sleep_duration > kMinSleepDuration) {
    std::this_thread::sleep_for(sleep_duration);
  }
}

}  // namespace stirling
}  // namespace pl
