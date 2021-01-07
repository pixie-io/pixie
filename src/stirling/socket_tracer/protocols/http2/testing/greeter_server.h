#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <string>
#include <vector>

#include <absl/strings/str_cat.h>
#include <absl/strings/substitute.h>
#include <grpcpp/grpcpp.h>

#include "src/common/base/macros.h"
#include "src/stirling/socket_tracer/protocols/http2/testing/proto/greet.grpc.pb.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace http2 {
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

class StreamingGreeterService final : public StreamingGreeter::Service {
 public:
  ::grpc::Status SayHelloServerStreaming(::grpc::ServerContext* /*context*/,
                                         const HelloRequest* request,
                                         ::grpc::ServerWriter<HelloReply>* writer) override {
    HelloReply reply;
    for (int i = 0; i < request->count(); ++i) {
      reply.set_message(absl::Substitute("Hello $0 for no. $1!", request->name(), i));
      writer->Write(reply);
    }
    return ::grpc::Status::OK;
  }

  ::grpc::Status SayHelloBidirStreaming(
      ::grpc::ServerContext* /*context*/,
      ::grpc::ServerReaderWriter<HelloReply, HelloRequest>* stream) override {
    HelloReply reply;
    HelloRequest request;
    while (stream->Read(&request)) {
      for (int i = 0; i < request.count(); ++i) {
        reply.set_message(absl::Substitute("Hello $0 for no. $1!", request.name(), i));
        stream->Write(reply);
      }
    }
    return ::grpc::Status::OK;
  }
};

class ServiceRunner {
 public:
  explicit ServiceRunner(int port = 0) : port_(port) {}

  std::unique_ptr<::grpc::Server> Run() {
    if (port_ == 0) {
      server_builder_.AddListeningPort("localhost:0", ::grpc::InsecureServerCredentials(), &port_);
    } else {
      server_builder_.AddListeningPort(absl::StrCat("localhost:", port_),
                                       ::grpc::InsecureServerCredentials());
    }
    return server_builder_.BuildAndStart();
  }

  void RegisterService(::grpc::Service* service) { server_builder_.RegisterService(service); }

  int port() const { return port_; }

 private:
  ::grpc::ServerBuilder server_builder_;
  int port_;
};

}  // namespace testing
}  // namespace http2
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
