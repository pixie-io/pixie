#include "src/stirling/testing/greeter_server.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

#include "absl/strings/str_cat.h"
#include "src/common/base/logging.h"

namespace pl {
namespace stirling {
namespace testing {

using ::grpc::Server;
using ::grpc::ServerContext;
using ::grpc::Service;
using ::grpc::Status;

Status GreeterService::SayHello(ServerContext*, const HelloRequest* request, HelloReply* response) {
  response->set_message(absl::StrCat("Hello ", request->name(), "!"));
  return Status::OK;
}

std::unique_ptr<Server> ServiceRunner::RunService(Service* service) {
  ports_.push_back(0);
  int* port_ptr = &ports_.back();
  server_builder_.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(), port_ptr);
  server_builder_.RegisterService(service);
  return server_builder_.BuildAndStart();
}

}  // namespace testing
}  // namespace stirling
}  // namespace pl
