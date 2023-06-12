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

#include <string>

#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"

namespace px {
namespace stirling {
namespace testing {

class GoBoringCryptoGRPCServerContainer : public ContainerRunner {
 public:
  GoBoringCryptoGRPCServerContainer()
      : ContainerRunner(::px::testing::BazelRunfilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_grpc_tls_pl/server/"
      "golang_boringcrypto_grpc_tls_server.tar";
  static constexpr std::string_view kContainerNamePrefix = "grpc_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTP/2 server";
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
