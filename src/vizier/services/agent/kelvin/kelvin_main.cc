#include <grpcpp/grpcpp.h>
#include <algorithm>

#include <sole.hpp>

#include "src/vizier/services/agent/kelvin/kelvin_manager.h"

#include "src/common/base/base.h"
#include "src/common/event/nats.h"
#include "src/common/signal/signal.h"
#include "src/shared/version/version.h"

DEFINE_string(nats_url, gflags::StringFromEnv("PL_NATS_URL", "pl-nats"),
              "The host address of the nats cluster");

DEFINE_string(query_broker_addr,
              gflags::StringFromEnv("PL_QUERY_BROKER_ADDR", "vizier-query-broker.pl.svc:50300"),
              "The host address of Query Broker");

DEFINE_string(mds_addr, gflags::StringFromEnv("PL_MDS_ADDR", "vizier-metadata.pl.svc:50400"),
              "The host address of the MDS");

DEFINE_string(pod_ip, gflags::StringFromEnv("PL_POD_IP", ""),
              "The IP address of the pod this controller is running on");

DEFINE_int32(rpc_port, gflags::Int32FromEnv("PL_RPC_PORT", 59300), "The port of the RPC server");

using ::pl::vizier::agent::KelvinManager;
using ::pl::vizier::agent::Manager;

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
  LOG(INFO) << absl::Substitute("Pixie Kelvin. Version: $0, id: $1",
                                pl::VersionInfo::VersionString(), agent_id.str());
  if (FLAGS_pod_ip.length() == 0) {
    LOG(FATAL) << "The POD_IP must be specified";
  }
  std::string addr = absl::Substitute("$0:$1", FLAGS_pod_ip, FLAGS_rpc_port);

  auto manager = KelvinManager::Create(agent_id, addr, FLAGS_rpc_port, FLAGS_nats_url,
                                       FLAGS_query_broker_addr, FLAGS_mds_addr)
                     .ConsumeValueOrDie();

  err_handler.set_manager(manager.get());

  PL_CHECK_OK(manager->Run());
  PL_CHECK_OK(manager->Stop(std::chrono::seconds{5}));

  // Clear the manager, because it has been stopped.
  err_handler.set_manager(nullptr);
}
