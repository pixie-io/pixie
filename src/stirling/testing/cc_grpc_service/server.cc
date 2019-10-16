#include "src/common/base/base.h"
#include "src/stirling/testing/greeter_server.h"

using ::pl::stirling::testing::GreeterService;
using ::pl::stirling::testing::ServiceRunner;

DEFINE_int32(port, 50051, "The port to listen.");

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);

  GreeterService greeter_service;
  ServiceRunner runner(FLAGS_port);
  runner.RegisterService(&greeter_service);
  auto server = runner.Run();
  server->Wait();
  return 0;
}
