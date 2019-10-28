#include <iostream>

#include "src/stirling/http2/testing/grpc_stub.h"
PL_SUPPRESS_WARNINGS_START()
#include "src/stirling/http2/testing/proto/greet.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

using ::pl::stirling::http2::testing::Greeter;
using ::pl::stirling::http2::testing::HelloReply;
using ::pl::stirling::http2::testing::HelloRequest;
using ::pl::stirling::testing::CreateInsecureGRPCChannel;
using ::pl::stirling::testing::GRPCStub;

DEFINE_string(name, "world", "The name of the party to greet.");
DEFINE_string(remote_endpoint, "127.0.0.1:50051", "The remote endpoint to connect.");

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);

  auto client_channel = CreateInsecureGRPCChannel(FLAGS_remote_endpoint);
  auto greeter_stub = std::make_unique<GRPCStub<Greeter>>(client_channel);
  HelloRequest req;
  req.set_name(FLAGS_name);
  HelloReply resp;
  ::grpc::Status status = greeter_stub->CallRPC(&Greeter::Stub::SayHello, req, &resp);
  std::string message = status.ok() ? "OK " : status.error_message();
  std::cout << message << resp.DebugString() << std::endl;
  return 0;
}
