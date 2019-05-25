#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <thread>

#include "absl/strings/str_cat.h"
#include "src/stirling/testing/greeter_client.h"
#include "src/stirling/testing/greeter_server.h"

namespace pl {
namespace stirling {
namespace testing {

TEST(GreeterTest, SayHelloWorks) {
  GreeterService service;
  ServiceRunner runner;
  std::unique_ptr<grpc::Server> server = runner.RunService(&service);
  auto* server_ptr = server.get();
  std::thread server_thread([server_ptr]() { server_ptr->Wait(); });

  GreeterClient client(absl::StrCat("0.0.0.0:", runner.ports().back()));

  HelloRequest req;
  HelloReply resp;

  req.set_name("pixielabs");
  grpc::Status st = client.SayHello(req, &resp);
  EXPECT_OK(st) << st.error_message();
  std::string resp_text;
  EXPECT_TRUE(google::protobuf::TextFormat::PrintToString(resp, &resp_text));
  EXPECT_EQ("message: \"Hello pixielabs!\"\n", resp_text);
  server->Shutdown();
  server_thread.join();
}

}  // namespace testing
}  // namespace stirling
}  // namespace pl
