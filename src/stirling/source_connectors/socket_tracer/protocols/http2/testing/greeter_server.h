/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto/greet.grpc.pb.h"

namespace px {
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
}  // namespace px
