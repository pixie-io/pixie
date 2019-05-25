#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#include "src/common/base/macros.h"
PL_SUPPRESS_WARNINGS_START()
#include "src/stirling/testing/proto/greet.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace stirling {
namespace testing {

class GreeterClient {
 public:
  explicit GreeterClient(std::string endpoint);
  grpc::Status SayHello(const HelloRequest& request, HelloReply* reply);

 private:
  std::unique_ptr<Greeter::Stub> stub_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
