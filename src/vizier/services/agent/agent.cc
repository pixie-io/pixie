#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <fstream>

#include "src/vizier/services/agent/controller/controller.h"

#include <sole.hpp>

#include "absl/strings/str_format.h"
#include "src/common/base/base.h"
#include "src/common/nats/nats.h"
#include "src/shared/version/version.h"

DEFINE_string(nats_url, gflags::StringFromEnv("PL_NATS_URL", "pl-nats"),
              "The host address of the nats cluster");

DEFINE_string(query_broker_addr,
              gflags::StringFromEnv("PL_QUERY_BROKER_ADDR",
                                    "vizier-query-broker.pl.svc.cluster.local:50300"),
              "The host address of Query Broker");

DEFINE_string(client_tls_cert,
              gflags::StringFromEnv("PL_CLIENT_TLS_CERT", "../../services/certs/client.crt"),
              "The GRPC client TLS cert");

DEFINE_string(client_tls_key,
              gflags::StringFromEnv("PL_CLIENT_TLS_KEY", "../../services/certs/client.key"),
              "The GRPC client TLS key");

DEFINE_string(tls_ca_crt, gflags::StringFromEnv("PL_TLS_CA_CERT", "../../services/certs/ca.crt"),
              "The GRPC CA cert");

DEFINE_bool(disable_SSL, gflags::BoolFromEnv("PL_DISABLE_SSL", false), "Disable GRPC SSL");

using pl::vizier::agent::Controller;

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);
  LOG(INFO) << "Pixie Lab Agent: " << pl::VersionInfo::VersionString();

  auto table_store = std::make_shared<pl::table_store::TableStore>();
  auto carnot = pl::carnot::Carnot::Create(table_store).ConsumeValueOrDie();
  auto stirling = pl::stirling::Stirling::Create();

  auto channel_creds = grpc::InsecureChannelCredentials();
  if (!FLAGS_disable_SSL) {
    auto ssl_opts = grpc::SslCredentialsOptions();
    ssl_opts.pem_root_certs = pl::FileContentsOrDie(FLAGS_tls_ca_crt);
    ssl_opts.pem_cert_chain = pl::FileContentsOrDie(FLAGS_client_tls_cert);
    ssl_opts.pem_private_key = pl::FileContentsOrDie(FLAGS_client_tls_key);
    channel_creds = grpc::SslCredentials(ssl_opts);
  }
  // Store the sirling ptr b/c we need a bit later to start the thread.
  auto stirling_ptr = stirling.get();

  // We need to move the channel here since GRPC mocking is done by the stub.
  auto chan = grpc::CreateChannel(FLAGS_query_broker_addr, channel_creds);
  // Try to connect to vizier.
  grpc_connectivity_state state = chan->GetState(true);
  while (state != grpc_connectivity_state::GRPC_CHANNEL_READY) {
    LOG(ERROR) << "Failed to connect to query broker";
    // Do a small sleep to avoud busy loop.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    state = chan->GetState(true);
  }
  LOG(INFO) << "Connected to query broker";
  auto stub =
      std::make_unique<pl::vizier::services::query_broker::querybrokerpb::QueryBrokerService::Stub>(
          chan);

  sole::uuid agent_id = sole::uuid4();

  auto agent_sub_topic = absl::StrFormat("/agent/%s", agent_id.str());
  std::unique_ptr<Controller::VizierNATSTLSConfig> tls_config;
  if (!FLAGS_disable_SSL) {
    tls_config = std::make_unique<Controller::VizierNATSTLSConfig>();
    tls_config->ca_cert = FLAGS_tls_ca_crt;
    tls_config->tls_cert = FLAGS_client_tls_cert;
    tls_config->tls_key = FLAGS_client_tls_key;
  }

  auto nats_connector = std::make_unique<Controller::VizierNATSConnector>(
      FLAGS_nats_url, "update_agent" /*pub_topic*/, agent_sub_topic, std::move(tls_config));

  auto controller = Controller::Create(agent_id, std::move(stub), std::move(carnot),
                                       std::move(stirling), table_store, std::move(nats_connector))
                        .ConsumeValueOrDie();

  PL_CHECK_OK(controller->InitThrowaway());
  PL_CHECK_OK(stirling_ptr->RunAsThread());
  PL_CHECK_OK(controller->Run());

  pl::ShutdownEnvironmentOrDie();
}
