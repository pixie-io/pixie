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

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/base/internal/spinlock.h>

#include "src/common/testing/testing.h"
#include "src/stirling/core/data_tables.h"
#include "src/stirling/core/unit_connector.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {
namespace testing {

template <bool EnableClientSideTracing = false>
class SocketTraceBPFTestFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_stirling_disable_self_tracing = false;

    // If client-side tracing is enabled, treat loopback as outside the cluster.
    // This will interpret localhost connections as leaving the cluster and tracing will apply.
    // WARNING: Do not use an if statement, because flags don't reset between successive tests.
    // TODO(oazizi): Setting flags in the test is a bad idea because of the pitfall above.
    //               Change this paradigm.
    FLAGS_treat_loopback_as_in_cluster = !EnableClientSideTracing;
    ASSERT_OK(source_.Init());

    source_.RawPtr()->sampling_freq_mgr().set_period(std::chrono::milliseconds{100});

    if constexpr (EnableClientSideTracing) {
      // This makes the Stirling interpret all traffic as leaving the cluster,
      // which means client-side tracing will also apply.
      ASSERT_OK(source_.SetClusterCIDR("1.2.3.4/32"));
    }
  }

  void TearDown() override { ASSERT_OK(source_.Stop()); }

  void ConfigureBPFCapture(traffic_protocol_t protocol, uint64_t role) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.RawPtr());
    ASSERT_OK(socket_trace_connector->UpdateBPFProtocolTraceRole(protocol, role));
  }

  void TestOnlySetTargetPID(int64_t pid) {
    FLAGS_test_only_socket_trace_target_pid = pid;
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.RawPtr());
    ASSERT_OK(socket_trace_connector->TestOnlySetTargetPID());
  }

  static constexpr int kCQLTableNum = SocketTraceConnector::kCQLTableNum;
  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr int kPGSQLTableNum = SocketTraceConnector::kPGSQLTableNum;
  static constexpr int kRedisTableNum = SocketTraceConnector::kRedisTableNum;
  static constexpr int kKafkaTableNum = SocketTraceConnector::kKafkaTableNum;
  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;
  static constexpr int kConnStatsTableNum = SocketTraceConnector::kConnStatsTableNum;

  UnitConnector<SocketTraceConnector> source_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
