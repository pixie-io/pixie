#include <grpcpp/grpcpp.h>
#include <algorithm>

#include <sole.hpp>

#include "src/vizier/services/agent/controller/controller.h"
#include "src/vizier/services/agent/controller/ssl.h"

#include "absl/strings/str_format.h"
#include "src/common/base/base.h"
#include "src/common/nats/nats.h"
#include "src/common/signal/signal.h"
#include "src/shared/version/version.h"

DEFINE_string(nats_url, gflags::StringFromEnv("PL_NATS_URL", "pl-nats"),
              "The host address of the nats cluster");

DEFINE_string(query_broker_addr,
              gflags::StringFromEnv("PL_QUERY_BROKER_ADDR", "vizier-query-broker.pl.svc:50300"),
              "The host address of Query Broker");

DEFINE_string(cloud_connector_addr,
              gflags::StringFromEnv("PL_CLOUD_CONNECTOR_ADDR",
                                    "vizier-cloud-connector.pl.svc:50800"),
              "The host address of the Cloud Connector");

using ::pl::SignalAction;
using ::pl::stirling::Stirling;
using ::pl::vizier::agent::Controller;
using ::pl::vizier::agent::SSL;
using ::pl::vizier::services::query_broker::querybrokerpb::QueryBrokerService;

std::unique_ptr<SignalAction> signal_action;

class AgentDeathHandler : public pl::FatalErrorHandlerInterface {
 public:
  AgentDeathHandler() = delete;
  explicit AgentDeathHandler(Controller* controller) : controller_(controller) {}

  void OnFatalError() const override {
    // Give a limited amount of time for the signal to stop,
    // since our death is imminent.
    static const std::chrono::milliseconds kTimeout{1000};
    pl::Status s = controller_->Stop(kTimeout);

    // Log and forge on, since our death is imminent.
    LOG_IF(ERROR, !s.ok()) << s.msg();
  }

 private:
  Controller* controller_;
};

std::unique_ptr<QueryBrokerService::Stub> ConnectToQueryBroker(
    const std::string query_broker_addr, std::shared_ptr<grpc::ChannelCredentials> channel_creds) {
  // We need to move the channel here since gRPC mocking is done by the stub.
  auto chan = grpc::CreateChannel(query_broker_addr, channel_creds);
  // Try to connect to the query broker.
  grpc_connectivity_state state = chan->GetState(true);
  while (state != grpc_connectivity_state::GRPC_CHANNEL_READY) {
    LOG(ERROR) << "Failed to connect to query broker at: " << query_broker_addr;
    // Do a small sleep to avoid busy loop.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    state = chan->GetState(true);
  }
  LOG(INFO) << "Connected to query broker at: " << query_broker_addr;
  return std::make_unique<QueryBrokerService::Stub>(chan);
}

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);
  signal_action = std::make_unique<SignalAction>();

  LOG(INFO) << "Pixie Lab Agent: " << pl::VersionInfo::VersionString();

  auto channel_creds = grpc::InsecureChannelCredentials();
  if (SSL::Enabled()) {
    channel_creds = grpc::SslCredentials(SSL::DefaultGRPCClientCreds());
  }

  auto table_store = std::make_shared<pl::table_store::TableStore>();
  auto stub_generator = [channel_creds](const std::string& remote_addr)
      -> std::unique_ptr<pl::carnotpb::KelvinService::StubInterface> {
    return pl::carnotpb::KelvinService::NewStub(grpc::CreateChannel(remote_addr, channel_creds));
  };

  auto carnot = pl::carnot::Carnot::Create(table_store, stub_generator).ConsumeValueOrDie();
  auto stirling = pl::stirling::Stirling::Create(pl::stirling::CreateProdSourceRegistry());

  // Store the sirling ptr b/c we need a bit later to start the thread.
  auto stirling_ptr = stirling.get();

  std::unique_ptr<QueryBrokerService::Stub> query_broker_stub;
  if (FLAGS_query_broker_addr.empty()) {
    LOG(WARNING) << "--query_broker_addr is empty, skip connecting to query broker.";
  } else {
    query_broker_stub = ConnectToQueryBroker(FLAGS_query_broker_addr, channel_creds);
  }

  sole::uuid agent_id = sole::uuid4();

  auto agent_sub_topic = absl::StrFormat("/agent/%s", agent_id.str());
  std::unique_ptr<Controller::VizierNATSTLSConfig> tls_config;
  if (SSL::Enabled()) {
    tls_config = SSL::DefaultNATSCreds();
  }

  std::unique_ptr<Controller::VizierNATSConnector> nats_connector;
  if (FLAGS_nats_url.empty()) {
    LOG(WARNING) << "--nats_url is empty, skip connecting to NATS.";
  } else {
    nats_connector = std::make_unique<Controller::VizierNATSConnector>(
        FLAGS_nats_url, "update_agent" /*pub_topic*/, agent_sub_topic, std::move(tls_config));
  }

  auto controller = Controller::Create(agent_id, std::move(query_broker_stub), std::move(carnot),
                                       std::move(stirling), table_store, std::move(nats_connector))
                        .ConsumeValueOrDie();

  // TODO(zasgar): When we clean this up make sure we better handle object lifetimes.
  AgentDeathHandler death_handler(controller.get());
  signal_action->RegisterFatalErrorHandler(death_handler);

  PL_CHECK_OK(controller->InitThrowaway());
  PL_CHECK_OK(stirling_ptr->RunAsThread());
  PL_CHECK_OK(controller->Run());

  pl::ShutdownEnvironmentOrDie();
}
