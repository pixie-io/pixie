#include <grpcpp/grpcpp.h>
#include <algorithm>

#include "src/common/base/base.h"
#include "src/shared/version/version.h"
#include "src/vizier/services/agent/controller/controller.h"

DEFINE_string(vizier_addr, gflags::StringFromEnv("PL_VIZIER_ADDR", "localhost:40000"),
              "The host address of vizier");

int main(int argc, char **argv) {
  pl::InitEnvironmentOrDie(&argc, argv);
  LOG(INFO) << "Pixie Lab Agent: " << pl::VersionInfo::VersionString();
  auto chan = grpc::CreateChannel(FLAGS_vizier_addr, grpc::InsecureChannelCredentials());
  auto table_store = std::make_shared<pl::table_store::TableStore>();
  auto carnot = pl::carnot::Carnot::Create(table_store).ConsumeValueOrDie();
  auto stirling = pl::stirling::Stirling::Create();

  // Store thr sirling ptr b/c we need a bit later to start the thread.
  auto stirling_ptr = stirling.get();

  auto controller =
      pl::agent::Controller::Create(chan, std::move(carnot), std::move(stirling), table_store)
          .ConsumeValueOrDie();
  PL_CHECK_OK(controller->InitThrowaway());

  // TODO(zasgar): We need to work on cleaning up thread mangement.
  PL_CHECK_OK(stirling_ptr->RunAsThread());

  VLOG(1) << "Starting gRPC client";
  PL_CHECK_OK(controller->Run());
  pl::ShutdownEnvironmentOrDie();
}
