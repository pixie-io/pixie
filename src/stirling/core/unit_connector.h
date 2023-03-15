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

#include <algorithm>
#include <memory>
#include <vector>

#include "src/common/base/statusor.h"
#include "src/stirling/core/connector_context.h"
#include "src/stirling/core/data_tables.h"
#include "src/stirling/core/frequency_manager.h"

DEFINE_uint32(pid, 0, "PID to profile. Use default value, -pid 0, to profile all processes.");

namespace px {
namespace stirling {

template <typename T>
class UnitConnector {
  using time_point = std::chrono::steady_clock::time_point;

 public:
  UnitConnector() : data_tables_(T::kTables) {}

  Status Init() {
    source_ = T::Create("source_connector");

    // Init() compiles the eBPF program and creates the eBPF perf buffer and maps needed
    // to communicate data to/from eBPF.
    PX_RETURN_IF_ERROR(source_->Init());

    // Give the source connector data tables to write into.
    source_->set_data_tables(data_tables_.tables());

    return Status::OK();
  }

  Status VerifyInitted() {
    if (source_ == nullptr) {
      return error::Internal("Source connector has not been initted, or was already deallocated.");
    }
    return Status::OK();
  }

  Status Stop() {
    // Pedantic, but better than bravely carrying on if something is wrong here.
    PX_RETURN_IF_ERROR(VerifyInitted());

    if (!started_) {
      return error::Internal("UnitConnector::Start() has not been called yet.");
    }
    if (stopped_) {
      return Status::OK();
    }

    // Stop transferring data.
    PX_RETURN_IF_ERROR(StopTransferDataThread());

    // Cleanup. Important!
    PX_RETURN_IF_ERROR(source_->Stop());
    stopped_ = true;

    return Status::OK();
  }

  Status Start() {
    // Pedantic, but better than bravely carrying on if something is wrong here.
    PX_RETURN_IF_ERROR(VerifyInitted());

    if (stopped_) {
      return error::Internal("Already stopped.");
    }
    if (started_) {
      return Status::OK();
    }

    PX_RETURN_IF_ERROR(StartTransferDataThread());
    started_ = true;

    return Status::OK();
  }

  ~UnitConnector() {
    const auto status = Stop();
    if (!status.ok()) {
      LOG(FATAL) << "Stop() not ok: " << status.msg();
    }

    // More cleanup (invokes dtor of source connector). Prevent Stop() from being invoked twice.
    source_ = nullptr;
  }

  StatusOr<types::ColumnWrapperRecordBatch> ConsumeRecords(const uint32_t table_num) {
    tablets_ = source_->data_tables()[table_num]->ConsumeRecords();
    if (tablets_.size() != 1) {
      char const* const msg = "Expected exactly one table, found tablets_.size(): $0.";
      return error::Internal(absl::Substitute(msg, tablets_.size()));
    }
    return tablets_[0].records;
  }

  T* RawPtr() { return source_.get(); }

 private:
  std::chrono::milliseconds TimeUntilNextTick(const time_point now)
      ABSL_SHARED_LOCKS_REQUIRED(state_lock_) {
    // Worst case, wake-up every so often.
    constexpr std::chrono::milliseconds kMaxSleepDuration{1000};
    auto wakeup_time = now + kMaxSleepDuration;
    wakeup_time = std::min(wakeup_time, source_->sampling_freq_mgr().next());

    return std::chrono::duration_cast<std::chrono::milliseconds>(wakeup_time - now);
  }

  Status TransferDataThread() {
    constexpr auto kRunWindow = std::chrono::milliseconds{1};
    auto time_until_next_tick = std::chrono::milliseconds::zero();
    auto now = std::chrono::steady_clock::now();

    FrequencyManager ctx_freq_mgr;
    ctx_freq_mgr.set_period(std::chrono::milliseconds{200});

    // This value can also be manipulated by the Stop() method.
    transfer_enable_ = true;

    while (transfer_enable_) {
      // To batch up work, i.e. to do more work per wakeup, we want to run our data
      // transfer or push data if its desired run time is anywhere between
      // time "now" and time "now + window".
      const auto now_plus_run_window = now + kRunWindow;

      if (FLAGS_pid == 0) {
        if (ctx_freq_mgr.Expired(now_plus_run_window)) {
          ctx_ = std::make_unique<SystemWideStandaloneContext>();
          now = std::chrono::steady_clock::now();
          ctx_freq_mgr.Reset(now);
        }
      }

      {
        // Prevent main thread from trying to join or stop the profiler while this is happening.
        absl::base_internal::SpinLockHolder lock(&state_lock_);

        // Transfer data from eBPF to user space.
        if (source_->sampling_freq_mgr().Expired(now_plus_run_window)) {
          // Read the eBPF perf buffer and map that stores the profiling information.
          // Data from eBPF is transferred into member data_table_.
          source_->TransferData(ctx_.get());

          // TransferData() is normally a significant amount of work: update "time now".
          now = std::chrono::steady_clock::now();
          source_->sampling_freq_mgr().Reset(now);
        }

        // Figure the time remaining until the next required data sample or push data.
        time_until_next_tick = TimeUntilNextTick(now);
      }

      // Sleep, only if time_until_next_tick exceeds the "run window," i.e. if that time
      // is long enough that Stirling should go to sleep. Otherwise, don't sleep and loop back
      // through the sources, with the expectation that one of the sources triggers a call to
      // either TransferData() or to PushData().
      if (time_until_next_tick >= kRunWindow) {
        std::this_thread::sleep_for(time_until_next_tick);

        // We just went to sleep: update time now.
        now = std::chrono::steady_clock::now();
      }
    }

    source_->TransferData(ctx_.get());
    return Status::OK();
  }

  Status StartTransferDataThread() {
    if (FLAGS_pid == 0) {
      // No PID specified. Go with "system wide" context.
      ctx_ = std::make_unique<SystemWideStandaloneContext>();
    } else {
      // If FLAGS_pid is set, the user wants to collect data from a specific process.

      // Get the process start time, used to construct the "UPID" or unique pid.
      // UPID is conceptually useful when Pixie is running on multiple hosts:
      // it includes a start timestamp (for recycled pids on a given host) and an address
      // space id to distinguish between pids on different hosts.
      PX_ASSIGN_OR_RETURN(const uint64_t ts, system::ProcParser().GetPIDStartTimeTicks(FLAGS_pid));

      // Here, we use zero because the UnitSourceConnector is not meant for a multi-host env.
      constexpr uint32_t kASID = 0;

      // The stand alone context requires a set of UPIDs. We have just one in that set.
      const absl::flat_hash_set<md::UPID> upids = {md::UPID(kASID, FLAGS_pid, ts)};

      // Create the context to filter by pid.
      ctx_ = std::make_unique<StandaloneContext>(upids);
    }

    // Create a thread to periodically read eBPF data.
    transfer_data_thread_ = std::thread(&UnitConnector<T>::TransferDataThread, this);

    return Status::OK();
  }

  Status StopTransferDataThread() {
    if (transfer_data_thread_.joinable()) {
      transfer_enable_ = false;
      {
        // Prevent transfer_data_thread_ from invoking TransferData().
        absl::base_internal::SpinLockHolder lock(&state_lock_);

        // Join the thread.
        transfer_data_thread_.join();

        // Transfer any remaining data from eBPF to user space.
        source_->TransferData(ctx_.get());
      }
    }

    return Status::OK();
  }

  // Data tables for the source connector.
  DataTables data_tables_;

  // The underlying data source. It will create an eBPF program that is periodically invoked
  // to collect stack data.
  std::unique_ptr<T> source_ = nullptr;

  // A thread that periodically wakes up to read eBPF perf buffers and maps.
  std::thread transfer_data_thread_;

  // The lock and transfer enable flag are used by transfer_data_thread_.
  absl::base_internal::SpinLock state_lock_;
  std::atomic<bool> transfer_enable_ = false;
  std::atomic<bool> started_ = false;
  std::atomic<bool> stopped_ = false;

  // The context can specify a set of processes to collect data from.
  // Here, we either create a "system wide" context (all processes) or use just one PID.
  std::unique_ptr<StandaloneContext> ctx_;

  // Once data is collected, i.e. after StopTransferDataThread is called,
  // the invoking program will call ConsumeRecords() and data will be aggregated into the
  // data table schema(s) and this vector of tablets will be populated.
  std::vector<TaggedRecordBatch> tablets_;
};

}  // namespace stirling
}  // namespace px
