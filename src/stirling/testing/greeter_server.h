#pragma once

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
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
    WaitForSignalIfEnabled();
    response->set_message(absl::StrCat("Hello ", request->name(), "!"));
    return ::grpc::Status::OK;
  }
  ::grpc::Status SayHelloAgain(::grpc::ServerContext* /*context*/, const HelloRequest* request,
                               HelloReply* response) override {
    WaitForSignalIfEnabled();
    response->set_message(absl::StrCat("Hello ", request->name(), "!"));
    return ::grpc::Status::OK;
  }

  void set_enable_cond_wait(bool v) { enable_cond_wait_ = v; }
  void Notify() { cond_var_.notify_all(); }
  // Wait for external signal, to simulate timeout.
  void WaitForSignalIfEnabled() {
    if (enable_cond_wait_) {
      std::unique_lock<std::mutex> lock(mutex_);
      cond_var_.wait(lock);
    }
  }

 private:
  bool enable_cond_wait_ = false;
  std::mutex mutex_;
  std::condition_variable cond_var_;
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
  ::grpc::Status SayHello(::grpc::ServerContext* /*context*/, const HelloRequest* request,
                          ::grpc::ServerWriter<HelloReply>* writer) override {
    HelloReply reply;
    for (int i = 0; i < request->count(); ++i) {
      reply.set_message(absl::Substitute("Hello $0 for no. $1!", request->name(), i));
      writer->Write(reply);
    }
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
