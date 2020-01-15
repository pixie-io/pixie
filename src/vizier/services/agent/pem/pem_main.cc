#include <grpcpp/grpcpp.h>
#include <algorithm>

#include <sole.hpp>

#include "src/vizier/services/agent/pem/pem_manager.h"

#include "src/common/base/base.h"
#include "src/common/event/nats.h"
#include "src/common/signal/signal.h"
#include "src/shared/version/version.h"

DEFINE_string(nats_url, gflags::StringFromEnv("PL_NATS_URL", "pl-nats"),
              "The host address of the nats cluster");

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
        LOG(ERROR) << "Failed to gracefull stop agent manager, it will terminate shortly.";
      }
    }
  }

 private:
  Manager* manager_ = nullptr;
};

std::unique_ptr<pl::SignalAction> signal_action;

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);
  signal_action = std::make_unique<pl::SignalAction>();
  AgentDeathHandler err_handler;
  signal_action->RegisterFatalErrorHandler(err_handler);

  sole::uuid agent_id = sole::uuid4();
  LOG(INFO) << absl::Substitute("Pixie PEM. Version: $0, id: $1", pl::VersionInfo::VersionString(),
                                agent_id.str());

  auto manager = PEMManager::Create(agent_id, FLAGS_nats_url).ConsumeValueOrDie();

  err_handler.set_manager(manager.get());

  PL_CHECK_OK(manager->Run());
  PL_CHECK_OK(manager->Stop(std::chrono::seconds{5}));

  // Clear the manager, because it has been stopped.
  err_handler.set_manager(nullptr);
}
