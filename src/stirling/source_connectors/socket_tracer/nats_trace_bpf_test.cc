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

#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"

namespace px {
namespace stirling {

using ::px::testing::BazelBinTestFilePath;

class NATSServerContainer : public ContainerRunner {
 public:
  NATSServerContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix, kReadyMessage) {
  }

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/nats_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "nats_server";
  static constexpr std::string_view kReadyMessage = "Server is ready";
};

class NATSClientContainer : public ContainerRunner {
 public:
  NATSClientContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kContainerNamePrefix, kReadyMessage) {
  }

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/protocols/nats/testing/"
      "nats_test_client_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "nats_test_client";
  static constexpr std::string_view kReadyMessage = "";
};

class NATSTraceBPFTest : public ::testing::Test {
 protected:
  NATSTraceBPFTest() { PL_CHECK_OK(server_container_.Run(std::chrono::seconds{150})); }

  NATSServerContainer server_container_;
  NATSClientContainer client_container_;
};

TEST_F(NATSTraceBPFTest, VerifyBatchedCommands) {
  client_container_.Run(
      std::chrono::seconds{10},
      {absl::Substitute("--network=container:$0", server_container_.container_name())});
  client_container_.Wait();
}

}  // namespace stirling
}  // namespace px
