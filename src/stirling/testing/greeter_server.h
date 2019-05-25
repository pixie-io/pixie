#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <vector>

#include "src/common/base/macros.h"
PL_SUPPRESS_WARNINGS_START()
#include "src/stirling/testing/proto/greet.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace stirling {
namespace testing {

class GreeterService final : public Greeter::Service {
 public:
  grpc::Status SayHello(grpc::ServerContext* context, const HelloRequest* request,
                        HelloReply* response) override;
};

class ServiceRunner {
 public:
  std::unique_ptr<grpc::Server> RunService(grpc::Service* service);
  const std::vector<int>& ports() const { return ports_; }

 private:
  grpc::ServerBuilder server_builder_;
  std::vector<int> ports_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
