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
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {
namespace testing {

template <bool TEnableClientSideTracing = false>
class SocketTraceBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_stirling_disable_self_tracing = false;
    auto source_connector = SocketTraceConnector::Create("socket_trace_connector");

    source_.reset(dynamic_cast<SocketTraceConnector*>(source_connector.release()));
    ASSERT_OK(source_->Init());

    // Cause Uprobes to deploy in a blocking manner.
    // We don't return until the first set of uprobes has successfully deployed.
    RefreshContext(/* blocking_deploy_uprobes */ true);

    // Drain the perf buffers before stimulus activity.
    // Otherwise, perf buffers may fill up, causing lost events and flaky test results.
    source_->PollPerfBuffers();
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  void ConfigureBPFCapture(TrafficProtocol protocol, uint64_t role) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->UpdateBPFProtocolTraceRole(protocol, role));
  }

  void TestOnlySetTargetPID(int64_t pid) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->TestOnlySetTargetPID(pid));
  }

  void RefreshContext(bool blocking_deploy_uprobes = false) {
    absl::base_internal::SpinLockHolder lock(&socket_tracer_state_lock_);

    ctx_ = std::make_unique<StandaloneContext>();

    // Normally, Stirling will be setup to think that all traffic is within the cluster,
    // which means only server-side tracing will kick in.
    if (TEnableClientSideTracing) {
      // This makes the Stirling interpret all traffic as leaving the cluster,
      // which means client-side tracing will also apply.
      PL_CHECK_OK(ctx_->SetClusterCIDR("1.2.3.4/32"));

      // Treat loopback as outside the cluster so we also interpret localhost connections
      // as leaving the cluste, which means client-side tracing will also apply.
      FLAGS_treat_loopback_as_in_cluster = false;
    }

    if (blocking_deploy_uprobes) {
      source_->InitContext(ctx_.get());
    }
  }

  void StartTransferDataThread(int table_num, const DataTableSchema& schema) {
    data_table_ = std::make_unique<DataTable>(schema);
    transfer_data_thread_ = std::thread(
        [this](int table_num, DataTable* data_table) {
          transfer_enable_ = true;
          while (transfer_enable_) {
            {
              absl::base_internal::SpinLockHolder lock(&socket_tracer_state_lock_);
              source_->TransferData(ctx_.get(), table_num, data_table);
            }
            std::this_thread::sleep_for(kTransferDataPeriod);
          }
        },
        table_num, data_table_.get());

    while (!transfer_enable_) {
    }

    // Wait for at least one TransferData() call before returning.
    std::this_thread::sleep_for(kTransferDataPeriod);
  }

  std::vector<TaggedRecordBatch> StopTransferDataThread() {
    // Give enough time for one more TransferData call by transfer_data_thread_,
    // so we make sure we've captured everything.
    std::this_thread::sleep_for(2 * kTransferDataPeriod);

    absl::base_internal::SpinLockHolder lock(&socket_tracer_state_lock_);
    CHECK(data_table_ != nullptr);
    CHECK(transfer_data_thread_.joinable());
    transfer_enable_ = false;
    transfer_data_thread_.join();
    return data_table_->ConsumeRecords();
  }

  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;

  absl::base_internal::SpinLock socket_tracer_state_lock_;

  std::unique_ptr<SocketTraceConnector> source_;
  std::unique_ptr<StandaloneContext> ctx_;
  std::atomic<bool> transfer_enable_ = false;
  std::thread transfer_data_thread_;
  std::unique_ptr<DataTable> data_table_;

  static constexpr std::chrono::milliseconds kTransferDataPeriod{100};
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
