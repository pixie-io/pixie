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

// Kafka broker packaged by Confluent (confluentinc/cp-kafka). Runs in KRaft mode
// (no ZooKeeper). The CLI tools (kafka-topics, kafka-console-producer, ...) are on the PATH.
class KafkaContainer : public ContainerRunner {
 public:
  KafkaContainer()
      : ContainerRunner(::px::testing::BazelRunfilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

  // Directory containing the CLI tools. Empty means they are on the PATH.
  static constexpr std::string_view kBinPath = "";
  // Suffix on the CLI tool names (e.g. kafka-topics vs kafka-topics.sh).
  static constexpr std::string_view kToolSuffix = "";

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/kafka_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "kafka_server";
  static constexpr std::string_view kReadyMessage = "Kafka Server started";
};

// Kafka broker packaged by the Apache Kafka project (apache/kafka). Runs in KRaft mode.
// The CLI tools live under /opt/kafka/bin and carry a .sh suffix.
class ApacheKafkaContainer : public ContainerRunner {
 public:
  ApacheKafkaContainer()
      : ContainerRunner(::px::testing::BazelRunfilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

  static constexpr std::string_view kBinPath = "/opt/kafka/bin/";
  static constexpr std::string_view kToolSuffix = ".sh";

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/apache_kafka_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "apache_kafka_server";
  static constexpr std::string_view kReadyMessage = "Kafka Server started";
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
