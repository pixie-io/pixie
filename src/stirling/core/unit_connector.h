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

  Status ResetConnectorContext(absl::flat_hash_set<md::UPID> upids) {
    // Stop transferring data.
    PX_RETURN_IF_ERROR(StopTransferDataThread());
    if (upids.size() == 0) {
      // The upids set is empty: trace all processes and use automatic context refresh.
      ctx_ = std::make_unique<EverythingLocalContext>();
    } else {
      // The upids set is non-empty: we will trace only processes identified by that set.
      ctx_ = std::make_unique<StandaloneContext>(upids);
    }
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

    source_ = T::Create("source_connector");

    // Compile the eBPF program and create eBPF perf buffers and maps as needed.
    PX_RETURN_IF_ERROR(source_->Init());

    if (upids.size() == 0) {
      // The upids set is empty: trace all processes and use automatic context refresh.
      ctx_ = std::make_unique<EverythingLocalContext>();
    } else {
      // The upids set is non-empty: we will trace only processes identified by that set.
      ctx_ = std::make_unique<StandaloneContext>(upids);
    }

    // Give the source connector data tables to write into.
    source_->set_data_tables(data_tables_.tables());

    // For the socket tracer (only), this triggers a blocking deploy of uprobes.
    // We do this here to support our socket tracer test cases.
    source_->InitContext(ctx_.get());

    return Status::OK();
  }

  Status SetClusterCIDR(std::string_view cidr_str) {
    // Pedantic, but better than bravely carrying on if something is wrong here.
    PX_RETURN_IF_ERROR(VerifyInitted());

    PX_RETURN_IF_ERROR(ctx_->SetClusterCIDR(cidr_str));

    return Status::OK();
  }

  Status BlockAndDeployUProbes() {
    // Pedantic, but better than bravely carrying on if something is wrong here.
    PX_RETURN_IF_ERROR(VerifyInitted());

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
    // Drain the perf buffers before starting the thread.
    // Otherwise, perf buffers may already be full, causing lost events and flaky test results.
    source_->BCC().PollPerfBuffers();

    // Check to ensure that the transfer data thread will run within a human time frame.
    // If this is triggered, please find a new upper bound or implement a special case.
    if (source_->sampling_freq_mgr().period() > std::chrono::seconds{90}) {
      return error::Internal("Source connector has sampling period > 90 seconds.");
    }

    while (transfer_enable_) {
      const auto t0 = std::chrono::steady_clock::now();
      ctx_->RefreshUPIDList();
      source_->TransferData(ctx_.get());
      const auto t1 = std::chrono::steady_clock::now();
      const auto t_elapsed = t1 - t0;
      std::this_thread::sleep_for(source_->sampling_freq_mgr().period() - t_elapsed);
    }
    transfer_exited_ = true;

    // Transfer any remaining data from eBPF to user space.
    source_->TransferData(ctx_.get());

    return Status::OK();
  }

  Status StartTransferDataThread() {
    PX_RETURN_IF_ERROR(BlockAndDeployUProbes());

    // About to start the transfer data thread. Level set our state now.
    transfer_exited_ = false;
    transfer_enable_ = true;

    // Create a thread to periodically read eBPF data.
    transfer_data_thread_ = std::thread(&UnitConnector<T>::TransferDataThread, this);

    return Status::OK();
  }

  Status StopTransferDataThread() {
    if (transfer_exited_) {
      return Status::OK();
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

    return Status::OK();
  }

  // Data tables for the source connector.
  DataTables data_tables_;

  // The underlying data source. It will create an eBPF program that is periodically invoked
  // to collect stack data.
  std::unique_ptr<T> source_ = nullptr;

  // A thread that periodically wakes up to read eBPF perf buffers and maps.
  std::thread transfer_data_thread_;

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
