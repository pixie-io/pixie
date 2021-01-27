#include <sole.hpp>

#include "src/vizier/services/agent/pem/pem_manager.h"

#include "src/common/base/base.h"
#include "src/common/signal/signal.h"
#include "src/shared/version/version.h"

DEFINE_string(nats_url, gflags::StringFromEnv("PL_NATS_URL", "pl-nats"),
              "The host address of the nats cluster");
DEFINE_string(pod_name, gflags::StringFromEnv("PL_POD_NAME", ""),
              "The name of the POD the PEM is running on");
DEFINE_string(host_ip, gflags::StringFromEnv("PL_HOST_IP", ""),
              "The IP of the host this service is running on");

using ::pl::stirling::Stirling;
using ::pl::vizier::agent::Manager;
using ::pl::vizier::agent::PEMManager;

class AgentDeathHandler : public pl::FatalErrorHandlerInterface {
 public:
  AgentDeathHandler() = default;
  void set_manager(Manager* manager) { manager_ = manager; }
  void OnFatalError() const override {
    if (manager_ != nullptr) {
      LOG(INFO) << "Trying to gracefully stop agent manager";
      auto s = manager_->Stop(std::chrono::seconds{5});
      if (!s.ok()) {
        LOG(ERROR) << "Failed to gracefully stop agent manager, it will terminate shortly.";
      }
    }
  }

 private:
  Manager* manager_ = nullptr;
};

std::unique_ptr<pl::SignalAction> signal_action;

int main(int argc, char** argv) {
  pl::EnvironmentGuard env_guard(&argc, argv);

  signal_action = std::make_unique<pl::SignalAction>();
  AgentDeathHandler err_handler;
  signal_action->RegisterFatalErrorHandler(err_handler);

  sole::uuid agent_id = sole::uuid4();
  LOG(INFO) << absl::Substitute("Pixie PEM. Version: $0, id: $1", pl::VersionInfo::VersionString(),
                                agent_id.str());

  if (FLAGS_host_ip.length() == 0) {
    LOG(FATAL) << "The HOST_IP must be specified";
  }
  auto manager = PEMManager::Create(agent_id, FLAGS_pod_name, FLAGS_host_ip, FLAGS_nats_url)
                     .ConsumeValueOrDie();

  err_handler.set_manager(manager.get());

  PL_CHECK_OK(manager->Run());
  PL_CHECK_OK(manager->Stop(std::chrono::seconds{5}));

  // Clear the manager, because it has been stopped.
  err_handler.set_manager(nullptr);

  return 0;
}
