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

DEFINE_string(pids, "", "PIDs to profile, e.g. -pids 132,133. All processes profiled by default.");

namespace px {
namespace stirling {

template <typename T>
class UnitConnector {
  using time_point = std::chrono::steady_clock::time_point;

 public:
  UnitConnector() : data_tables_(T::kTables) {}

  ~UnitConnector() {
    if (started_ && !stopped_) {
      const auto status = Stop();
      if (!status.ok()) {
        LOG(FATAL) << "Stop() not ok: " << status.msg();
      }
    }

    // Invokes dtor of the underlying source connector.
    source_ = nullptr;
  }

  Status Stop() {
    // Pedantic, but better than bravely carrying on if something is wrong here.
    PX_RETURN_IF_ERROR(VerifyInitted());

    if (!started_) {
      // Some test cases use the TransferData method directly. In such cases,
      // Start will not be called. It is ok to leave here with status "OK".
      return Status::OK();
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

  Status Flush() {
    // Pedantic, but better than bravely carrying on if something is wrong here.
    PX_RETURN_IF_ERROR(VerifyInitted());

    if (!started_) {
      return error::Internal("Not yet started.");
    }
    if (stopped_) {
      return error::Internal("Already stopped.");
    }

    // Stop transferring data.
    PX_RETURN_IF_ERROR(StopTransferDataThread());

    // Restart.
    PX_RETURN_IF_ERROR(StartTransferDataThread());

    return Status::OK();
  }

  Status Init(absl::flat_hash_set<md::UPID> upids = {}) {
    if (upids.size() == 0) {
      // Enter this branch if Init() is called with no arguments (i.e. with a default empty set).
      // ParsePidsFlag() inspects the value in FLAGS_pids to find any pids the user specified
      // on the command line.
      PX_ASSIGN_OR_RETURN(upids, ParsePidsFlag());
    }

    if (upids.size() == 0) {
      // The upids set is empty: trace all processes and use automatic context refresh.
      ctx_refresh_enabled_ = true;
      ctx_ = std::make_unique<SystemWideStandaloneContext>();
    } else {
      // The upids set is non-empty: we will trace only processes identified by that set.
      // Disable automatic context refresh.
      ctx_refresh_enabled_ = false;
      ctx_ = std::make_unique<StandaloneContext>(upids);
    }

    source_ = T::Create("source_connector");

    // Compile the eBPF program and create eBPF perf buffers and maps as needed.
    PX_RETURN_IF_ERROR(source_->Init());

    // Give the source connector data tables to write into.
    source_->set_data_tables(data_tables_.tables());

    // For the socket tracer (only), this triggers a blocking deploy of uprobes.
    // We do this here to support our socket tracer test cases.
    source_->InitContext(ctx_.get());

    return Status::OK();
  }

  Status RefreshContextAndDeployUProbes() {
    // Pedantic, but better than bravely carrying on if something is wrong here.
    PX_RETURN_IF_ERROR(VerifyInitted());

    // This method manipulates state normally managed by the transfer data thread. If the transfer
    // data thread is running, it is an error to try this. If we want to enable this method in
    // parallel with the transfer data thread, then an we can bring back the shared state lock.
    if (transfer_enable_) {
      return error::Internal("Context is being managed by the transfer data thread.");
    }

    if (ctx_refresh_enabled_) {
      // Pick up new processes.
      ctx_ = std::make_unique<SystemWideStandaloneContext>();
    }

    // This will block and deploy uprobes.
    source_->InitContext(ctx_.get());

    return Status::OK();
  }

  Status TransferData() {
    // Pedantic, but better than bravely carrying on if something is wrong here.
    PX_RETURN_IF_ERROR(VerifyInitted());

    if (started_ || transfer_enable_) {
      return error::Internal("Transfer data thread is running.");
    }

    source_->TransferData(ctx_.get());
    return Status::OK();
  }

  StatusOr<types::ColumnWrapperRecordBatch> ConsumeRecords(const uint32_t table_num) {
    // Pedantic, but better than bravely carrying on if something is wrong here.
    PX_RETURN_IF_ERROR(VerifyInitted());

    tablets_ = source_->data_tables()[table_num]->ConsumeRecords();
    if (tablets_.size() != 1) {
      char const* const msg = "Expected exactly one tablet, found tablets_.size(): $0.";
      return error::Internal(absl::Substitute(msg, tablets_.size()));
    }
    return tablets_[0].records;
  }

  T* RawPtr() { return source_.get(); }

 private:
  StatusOr<absl::flat_hash_set<md::UPID> > ParsePidsFlag() {
    const std::vector<std::string_view> pids = absl::StrSplit(FLAGS_pids, ",", absl::SkipEmpty());

    if (pids.size() == 0) {
      return absl::flat_hash_set<md::UPID>({});
    }

    // For the UPIDs, we use ASID=0 because UnitConnector is not meant for a multi-host env.
    constexpr uint32_t kASID = 0;

    // A memory location that will be targeted by absl::SimpleAtoi; see below.
    uint32_t int_pid;

    // Eventually, this will be the return value from this fn.
    absl::flat_hash_set<md::UPID> upids;

    // Convert each string view pid to an int, then form a UPID.
    for (const auto& str_pid : pids) {
      const bool parsed_ok = absl::SimpleAtoi(str_pid, &int_pid);

      if (!parsed_ok) {
        return error::Internal(absl::Substitute("Could not parse pid $0 to integer.", str_pid));
      }
      PX_ASSIGN_OR_RETURN(const uint64_t ts, system::ProcParser().GetPIDStartTimeTicks(int_pid));

      // Create a upid and add it to the upids set.
      upids.insert(md::UPID(kASID, int_pid, ts));
    }
    return upids;
  }

  Status VerifyInitted() {
    if (source_ == nullptr) {
      return error::Internal("Source connector has not been initted, or was already deallocated.");
    }
    if (ctx_ == nullptr) {
      return error::Internal("Context has not been initted, or was already deallocated.");
    }
    return Status::OK();
  }

  Status TransferDataThread() {
    // Invoke source_->TransferData() ASAP to drain maps & perf buffers, i.e. because some time may
    // have elapsed between starting a BPF program and starting the transfer data thread.
    source_->TransferData(ctx_.get());

    // The run window prevents this thread from going to sleep for very short time frames.
    // If the thread fails to go to sleep, it will do all the work necessary on the next loop
    // iteration such that it can go to sleep for a reasonably longer amount of time.
    constexpr auto kRunWindow = std::chrono::milliseconds{1};

    // In stirling.cc, the max sleep duration is 1 second. That makes sense in a context where
    // every source connector is running in parallel, and we know that there may be a good reason
    // to periodically wake up. The various source connectors do have significantly different
    // sampling periods, e.g. perf profiler (long) vs. socket tracer (short).
    // This is a pedantic check to ensure that the transfer data thread will run within a human
    // time frame. If it is triggered, someone is working on an interesting case anyway, they
    // can figure out a new upper bound or a special case somehow.
    if (source_->sampling_freq_mgr().period() > std::chrono::seconds{90}) {
      return error::Internal("Source connector has sampling period > 90 seconds.");
    }

    // If the source connector sampling period is less than the "run window," then
    // the transfer data thread will never sleep.
    if (source_->sampling_freq_mgr().period() <= kRunWindow) {
      return error::Internal("Source connector sampling period is less than run window.");
    }

    // Time "now". Updated after sleeping or after doing some amount of work.
    auto now = std::chrono::steady_clock::now();

    // Create a frequency manager that will cause the context to get refreshed periodically.
    FrequencyManager ctx_freq_mgr;
    ctx_freq_mgr.set_period(std::chrono::milliseconds{200});

    while (transfer_enable_) {
      // To batch up work, i.e. to do more work per wakeup, we want to run our data
      // transfer or push data if its desired run time is anywhere between
      // time "now" and time "now + window".
      const auto now_plus_run_window = now + kRunWindow;

      // Transfer data from eBPF to user space.
      if (source_->sampling_freq_mgr().Expired(now_plus_run_window)) {
        // Read the eBPF perf buffer and map that stores the profiling information.
        // Data from eBPF is transferred into member data_table_.
        source_->TransferData(ctx_.get());

        // TransferData() is normally a significant amount of work: update "time now".
        now = std::chrono::steady_clock::now();
        source_->sampling_freq_mgr().Reset(now);
      }

      // Check if we need to refresh the system wide context. This is not necessary if the user
      // specified a certain PID to trace.
      if (ctx_refresh_enabled_) {
        if (ctx_freq_mgr.Expired(now_plus_run_window)) {
          ctx_ = std::make_unique<SystemWideStandaloneContext>();
          now = std::chrono::steady_clock::now();
          ctx_freq_mgr.Reset(now);
        }
      }

      // Figure the time of next required data sample.
      const auto next = source_->sampling_freq_mgr().next();

      // Compute the amount of time to sleep.
      const auto sleep_time = std::chrono::duration_cast<std::chrono::milliseconds>(next - now);

      // Sleep, only if sleep_time exceeds the "run window." In other words, do not sleep if within
      // the next millisecond we will wakeup to sample data or refresh the context.
      if (sleep_time >= kRunWindow) {
        std::this_thread::sleep_for(sleep_time);

        // We just went to sleep: update time now.
        now = std::chrono::steady_clock::now();
      }
    }
    transfer_exited_ = true;

    return Status::OK();
  }

  Status StartTransferDataThread() {
    PX_RETURN_IF_ERROR(RefreshContextAndDeployUProbes());

    // About to start the transfer data thread. Level set our state now.
    transfer_exited_ = false;
    transfer_enable_ = true;

    // Create a thread to periodically read eBPF data.
    transfer_data_thread_ = std::thread(&UnitConnector<T>::TransferDataThread, this);

    return Status::OK();
  }

  Status StopTransferDataThread() {
    if (transfer_exited_) {
      return error::Internal("Transfer data thread already stopped.");
    }
    if (!transfer_data_thread_.joinable()) {
      return error::Internal("Transfer data thread not joinable; strange!!");
    }

    transfer_enable_ = false;

    while (!transfer_exited_) {
      std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }

    // This is unlikely, the thread will normally have terminated already.
    if (transfer_data_thread_.joinable()) {
      transfer_data_thread_.join();
    }

    // Transfer any remaining data from eBPF to user space.
    source_->TransferData(ctx_.get());

    return Status::OK();
  }

  // Data tables for the source connector.
  DataTables data_tables_;

  // The underlying data source. It will create an eBPF program that is periodically invoked
  // to collect stack data.
  std::unique_ptr<T> source_ = nullptr;

  // A thread that periodically wakes up to read eBPF perf buffers and maps.
  std::thread transfer_data_thread_;

  // To enable/disable automatic system wide context refresh.
  std::atomic<bool> ctx_refresh_enabled_ = false;

  // State of the transfer data thread: enabled, or exited.
  std::atomic<bool> transfer_enable_ = false;
  std::atomic<bool> transfer_exited_ = false;

  // Top level state, i.e. whether Start() and Stop() methods were called and succeeded.
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
