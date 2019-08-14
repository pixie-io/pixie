#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "src/common/base/macros.h"
PL_SUPPRESS_WARNINGS_START()
#include "src/stirling/testing/proto/greet.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace stirling {
namespace testing {

class GreeterService final : public Greeter::Service {
 public:
  ::grpc::Status SayHello(::grpc::ServerContext* /*context*/, const HelloRequest* request,
                          HelloReply* response) override {
    response->set_message(absl::StrCat("Hello ", request->name(), "!"));
    return ::grpc::Status::OK;
  }
  ::grpc::Status SayHelloAgain(::grpc::ServerContext* /*context*/, const HelloRequest* request,
                               HelloReply* response) override {
    response->set_message(absl::StrCat("Hello ", request->name(), "!"));
    return ::grpc::Status::OK;
  }
};

class Greeter2Service final : public Greeter2::Service {
 public:
  ::grpc::Status SayHi(::grpc::ServerContext* /*context*/, const HelloRequest* request,
                       HelloReply* response) override {
    response->set_message(absl::StrCat("Hi ", request->name(), "!"));
    return ::grpc::Status::OK;
  }
  ::grpc::Status SayHiAgain(::grpc::ServerContext* /*context*/, const HelloRequest* request,
                            HelloReply* response) override {
    response->set_message(absl::StrCat("Hi ", request->name(), "!"));
    return ::grpc::Status::OK;
  }
};

class ServiceRunner {
 public:
  std::unique_ptr<::grpc::Server> Run() {
    server_builder_.AddListeningPort("localhost:0", ::grpc::InsecureServerCredentials(), &port_);
    return server_builder_.BuildAndStart();
  }

  void RegisterService(::grpc::Service* service) { server_builder_.RegisterService(service); }

  int port() const { return port_; }

 private:
  ::grpc::ServerBuilder server_builder_;
  int port_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
