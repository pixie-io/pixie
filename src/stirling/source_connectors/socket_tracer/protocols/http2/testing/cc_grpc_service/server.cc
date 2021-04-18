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

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/greeter_server.h"

using ::px::stirling::protocols::http2::testing::GreeterService;
using ::px::stirling::protocols::http2::testing::ServiceRunner;

DEFINE_int32(port, 50051, "The port to listen.");

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  GreeterService greeter_service;
  ServiceRunner runner(FLAGS_port);
  runner.RegisterService(&greeter_service);
  auto server = runner.Run();
  server->Wait();
  return 0;
}
