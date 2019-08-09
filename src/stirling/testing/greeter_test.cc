#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <thread>

#include "absl/strings/str_cat.h"
#include "src/stirling/testing/greeter_server.h"
#include "src/stirling/testing/grpc_stub.h"

namespace pl {
namespace stirling {
namespace testing {

TEST(GreeterTest, SayHelloWorks) {
  GreeterService service;
  ServiceRunner runner;
  std::unique_ptr<grpc::Server> server = runner.RunService(&service);
  auto* server_ptr = server.get();
  std::thread server_thread([server_ptr]() { server_ptr->Wait(); });

  auto stub = std::make_unique<GRPCStub<Greeter>>(absl::StrCat("127.0.0.1:", runner.port()));

  HelloRequest req;
  HelloReply resp;

  req.set_name("pixielabs");
  grpc::Status st = stub->CallRPC(&Greeter::Stub::SayHello, req, &resp);
  EXPECT_TRUE(st.ok()) << st.error_message();
  std::string resp_text;
  EXPECT_TRUE(google::protobuf::TextFormat::PrintToString(resp, &resp_text));
  EXPECT_EQ("message: \"Hello pixielabs!\"\n", resp_text);
  server->Shutdown();
  server_thread.join();
}

}  // namespace testing
}  // namespace stirling
}  // namespace pl
