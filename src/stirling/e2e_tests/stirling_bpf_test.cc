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

#include <functional>
#include <thread>
#include <utility>

#include <absl/functional/bind_front.h>

#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/stirling.h"

namespace px {
namespace stirling {

class StirlingBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();
    registry->RegisterOrDie<SocketTraceConnector>();
    stirling_ = Stirling::Create(std::move(registry));

    // Set callback function which receives the pushed data.
    stirling_->RegisterDataPushCallback(absl::bind_front(&StirlingBPFTest::AppendData, this));
  }

  Status AppendData(uint64_t table_id, types::TabletID tablet_id,
                    std::unique_ptr<types::ColumnWrapperRecordBatch> record_batch) {
    PX_UNUSED(table_id);
    PX_UNUSED(tablet_id);
    record_batches_.push_back(std::move(record_batch));
    return Status::OK();
  }

  std::unique_ptr<Stirling> stirling_;
  std::vector<std::unique_ptr<types::ColumnWrapperRecordBatch>> record_batches_;
};

// Stop Stirling. Meant to be called asynchronously, via a thread.
void AsyncKill(Stirling* stirling) {
  if (stirling != nullptr) {
    stirling->Stop();
  }
}

TEST_F(StirlingBPFTest, CleanupTest) {
  ASSERT_OK(stirling_->RunAsThread());

  // Wait for thread to initialize.
  ASSERT_OK(stirling_->WaitUntilRunning(/* timeout */ std::chrono::seconds(5)));

  EXPECT_GT(bpf_tools::BCCWrapper::num_attached_probes(), 0);
  EXPECT_GT(bpf_tools::BCCWrapper::num_open_perf_buffers(), 0);

  std::thread killer_thread = std::thread(&AsyncKill, stirling_.get());

  ASSERT_TRUE(killer_thread.joinable());
  killer_thread.join();

  EXPECT_EQ(bpf_tools::BCCWrapper::num_attached_probes(), 0);
  EXPECT_EQ(bpf_tools::BCCWrapper::num_open_perf_buffers(), 0);
}

}  // namespace stirling
}  // namespace px
