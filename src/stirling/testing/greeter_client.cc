#include "src/stirling/testing/greeter_client.h"

#include <grpcpp/grpcpp.h>

#include "src/common/base/logging.h"

namespace pl {
namespace stirling {
namespace testing {

using ::grpc::ClientContext;
using ::grpc::CreateChannel;
using ::grpc::InsecureChannelCredentials;
using ::grpc::Status;

GreeterClient::GreeterClient(std::string endpoint) {
  stub_ = Greeter::NewStub(CreateChannel(endpoint, InsecureChannelCredentials()));
  CHECK(stub_ != nullptr) << "Failed to create Greeter stub.";
}

Status GreeterClient::SayHello(const HelloRequest& request, HelloReply* reply) {
  ClientContext context;
  return stub_->SayHello(&context, request, reply);
}

}  // namespace testing
}  // namespace stirling
}  // namespace pl
