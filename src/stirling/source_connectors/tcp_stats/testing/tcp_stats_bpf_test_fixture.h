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
#include "src/stirling/source_connectors/tcp_stats/tcp_stats_connector.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {
namespace testing {

class TcpTraceBPFTestFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    auto source_connector = TCPStatsConnector::Create("tcp_stats");
    source_.reset(dynamic_cast<TCPStatsConnector*>(source_connector.release()));
    ASSERT_OK(source_->Init());
    source_->set_data_tables(data_tables_.tables());
    ctx_ = std::make_unique<SystemWideStandaloneContext>();
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  void StartTransferDataThread() {
    // Drain the perf buffers before starting the thread.
    // Otherwise, perf buffers may already be full, causing lost events and flaky test results.
    ASSERT_TRUE(source_ != nullptr);

    source_->BCC().PollPerfBuffers();

    transfer_data_thread_ = std::thread([this]() {
      transfer_enable_ = true;
      while (transfer_enable_) {
        {
          RefreshContext();
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
    CHECK(transfer_data_thread_.joinable());
    transfer_enable_ = false;
    transfer_data_thread_.join();
    source_->TransferData(ctx_.get());
  }

  std::vector<TaggedRecordBatch> ConsumeRecords(int table_num) {
    return source_->data_tables()[table_num]->ConsumeRecords();
  }

  void RefreshContext() {
    absl::base_internal::SpinLockHolder lock(&tcp_stats_state_lock_);
    ctx_ = std::make_unique<SystemWideStandaloneContext>();
  }

  static constexpr int kTcpstatsTableNum = TCPStatsConnector::kTCPStatsTableNum;
  DataTables data_tables_{TCPStatsConnector::kTables};
  DataTable* tcp_stats_table_ = data_tables_[kTcpstatsTableNum];
  absl::base_internal::SpinLock tcp_stats_state_lock_;
  std::unique_ptr<TCPStatsConnector> source_;
  std::atomic<bool> transfer_enable_ = false;
  std::unique_ptr<SystemWideStandaloneContext> ctx_;
  std::thread transfer_data_thread_;
  static constexpr std::chrono::milliseconds kTransferDataPeriod{100};
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
