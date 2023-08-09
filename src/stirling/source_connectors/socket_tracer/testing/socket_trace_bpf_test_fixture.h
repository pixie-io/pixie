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

    auto source_connector = SocketTraceConnector::Create("socket_trace_connector");

    source_.reset(dynamic_cast<SocketTraceConnector*>(source_connector.release()));
    ASSERT_OK(source_->Init());

    source_->set_data_tables(data_tables_.tables());

    // Cause Uprobes to deploy in a blocking manner.
    // We don't return until the first set of uprobes has successfully deployed.
    RefreshContext(/* blocking_deploy_uprobes */ true);
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  void ConfigureBPFCapture(traffic_protocol_t protocol, uint64_t role) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->UpdateBPFProtocolTraceRole(protocol, role));
  }

  void TestOnlySetTargetPID(int64_t pid) {
    FLAGS_test_only_socket_trace_target_pid = pid;
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->TestOnlySetTargetPID());
  }

  void RefreshContext(bool blocking_deploy_uprobes = false) {
    absl::base_internal::SpinLockHolder lock(&socket_tracer_state_lock_);

    RefreshContextCore();

    if (blocking_deploy_uprobes) {
      source_->InitContext(ctx_.get());
    }
  }

  void StartTransferDataThread() {
    // Drain the perf buffers before starting the thread.
    // Otherwise, perf buffers may already be full, causing lost events and flaky test results.
    source_->BCC().PollPerfBuffers();

    transfer_data_thread_ = std::thread([this]() {
      transfer_enable_ = true;
      while (transfer_enable_) {
        {
          absl::base_internal::SpinLockHolder lock(&socket_tracer_state_lock_);
          RefreshContextCore();
          source_->TransferData(ctx_.get());
        }
        std::this_thread::sleep_for(kTransferDataPeriod);
      }
    });

    while (!transfer_enable_) {
    }

    // Wait for at least one TransferData() call before returning.
    std::this_thread::sleep_for(kTransferDataPeriod);
  }

  void StopTransferDataThread() {
    // Give enough time for one more TransferData call by transfer_data_thread_,
    // so we make sure we've captured everything.
    std::this_thread::sleep_for(2 * kTransferDataPeriod);

    absl::base_internal::SpinLockHolder lock(&socket_tracer_state_lock_);
    CHECK(transfer_data_thread_.joinable());
    transfer_enable_ = false;
    transfer_data_thread_.join();
  }

  std::vector<TaggedRecordBatch> ConsumeRecords(int table_num) {
    return source_->data_tables()[table_num]->ConsumeRecords();
  }

  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;

  absl::base_internal::SpinLock socket_tracer_state_lock_;

  std::unique_ptr<SocketTraceConnector> source_;
  std::unique_ptr<SystemWideStandaloneContext> ctx_;
  std::atomic<bool> transfer_enable_ = false;
  std::thread transfer_data_thread_;
  DataTables data_tables_{SocketTraceConnector::kTables};

  static constexpr std::chrono::milliseconds kTransferDataPeriod{100};

 private:
  void RefreshContextCore() {
    ctx_ = std::make_unique<SystemWideStandaloneContext>();

    if (EnableClientSideTracing) {
      // This makes the Stirling interpret all traffic as leaving the cluster,
      // which means client-side tracing will also apply.
      PX_CHECK_OK(ctx_->SetClusterCIDR("1.2.3.4/32"));
    }
  }
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
