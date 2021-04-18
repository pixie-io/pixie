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

#include <iostream>

#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/grpc_stub.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto/greet.grpc.pb.h"

using ::px::stirling::protocols::http2::testing::Greeter;
using ::px::stirling::protocols::http2::testing::HelloReply;
using ::px::stirling::protocols::http2::testing::HelloRequest;
using ::px::stirling::testing::CreateInsecureGRPCChannel;
using ::px::stirling::testing::GRPCStub;

DEFINE_string(name, "world", "The name of the party to greet.");
DEFINE_string(remote_endpoint, "127.0.0.1:50051", "The remote endpoint to connect.");

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  auto client_channel = CreateInsecureGRPCChannel(FLAGS_remote_endpoint);
  auto greeter_stub = std::make_unique<GRPCStub<Greeter>>(client_channel);
  HelloRequest req;
  req.set_name(FLAGS_name);
  HelloReply resp;
  ::grpc::Status status = greeter_stub->CallRPC(&Greeter::Stub::SayHello, req, &resp);
  std::string message = status.ok() ? "OK " : status.error_message();
  std::cout << message << resp.DebugString() << std::endl;
  return 0;
}
