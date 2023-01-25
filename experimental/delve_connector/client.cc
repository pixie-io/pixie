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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <jsonrpcpp.hpp>

#include "experimental/delve_connector/dlv_driver.h"
#include "src/common/base/base.h"

DEFINE_int32(port, 0, "Remote server port");
DEFINE_bool(quiet, false, "Suppress output (for benchmarking)");

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  DelveDriver d;
  ::px::Status s = d.Connect("127.0.0.1", FLAGS_port);
  PX_CHECK_OK(s);

  d.Init();

  // Create a breakpoint.
  d.CreateBreakpoint("net/http.(*http2Framer).WriteDataPadded");
  d.CreateBreakpoint("golang.org/x/net/http2.(*Framer).WriteDataPadded");

  while (true) {
    d.Continue();

    auto stream_id = d.Eval("streamID");
    CHECK(!stream_id.is_null());

    LOG_IF(INFO, !FLAGS_quiet) << absl::Substitute("stream_id=$0", stream_id.get<std::string>());
  }

  d.Close();
}
