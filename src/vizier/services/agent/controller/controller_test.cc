#include "src/vizier/services/agent/controller/controller.h"

#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>

#include <utility>

namespace pl {
namespace vizier {
namespace agent {

// Tests that NATS and query broker can be omitted, and controller can run without problem.
TEST(ControllerTest, NatsConnectorAndQueryBrokerCanBeOmitted) {
  sole::uuid agent_id = sole::uuid4();
  auto table_store = std::make_shared<pl::table_store::TableStore>();
  auto channel_creds = grpc::InsecureChannelCredentials();
  auto stub_generator = [channel_creds](const std::string& remote_addr)
      -> std::unique_ptr<pl::carnotpb::KelvinService::StubInterface> {
    return pl::carnotpb::KelvinService::NewStub(grpc::CreateChannel(remote_addr, channel_creds));
  };

  auto carnot = pl::carnot::Carnot::Create(table_store, stub_generator).ConsumeValueOrDie();
  auto stirling = pl::stirling::Stirling::Create(pl::stirling::CreateProdSourceRegistry());

  auto controller = Controller::Create(agent_id, nullptr, std::move(carnot), std::move(stirling),
                                       table_store, nullptr)
                        .ConsumeValueOrDie();
  auto* controller_ptr = controller.get();
  std::thread controller_run_thread([controller_ptr]() { EXPECT_OK(controller_ptr->Run()); });
  std::this_thread::sleep_for(std::chrono::seconds(2));
  static const std::chrono::milliseconds kTimeout{1000};
  EXPECT_OK(controller->Stop(kTimeout));
  controller_run_thread.join();
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
