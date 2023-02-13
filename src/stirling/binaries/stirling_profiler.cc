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

#include <csignal>
#include <iostream>
#include <thread>

#include "src/common/base/base.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/perf_profiler/stack_traces_table.h"

using ::px::Status;

DEFINE_uint32(time, 30, "Number of seconds to run the profiler.");
DEFINE_uint32(pid, 0, "PID to profile. Use default value, -pid 0, to profile all processes.");

namespace px {
namespace stirling {

class Profiler {
 public:
  Profiler() : data_table_(kDataTableId, kStackTraceTable) {}

  Status Init() {
    // Instantiate the perf profile source connector.
    source_ = PerfProfileConnector::Create("perf_profile_connector");

    // Init() compiles the eBPF program and creates the eBPF perf buffer and maps needed
    // to communicate data to/from eBPF.
    PX_RETURN_IF_ERROR(source_->Init());

    // Sanity check.
    if (std::chrono::seconds(FLAGS_time) < source_->SamplingPeriod()) {
      return error::InvalidArgument(
          "Profiler will stop before any profiles are collected. Try increasing arg. -time or "
          "reducing arg. -stirling_profiler_table_update_period_seconds.");
    }

    // We give the source connector a data table to write data into. Note that in the context of
    // the full "Pixie Edge Module (pem)" (with multiple source connectors) it makes more sense
    // to have the data tables owned outside of the source connectors.
    source_->set_data_tables({&data_table_});

    return Status::OK();
  }

  Status Stop() {
    if (source_ != nullptr) {
      // Cleanup. Important!
      PX_RETURN_IF_ERROR(source_->Stop());
    }

    // More cleanup (invokes dtor of source connector). Prevent Stop() from being invoked twice.
    source_ = nullptr;

    return Status::OK();
  }

  Status PrintData() {
    // This prints a long spew of counts and stack traces.
    // We will replace this with a pprof proto file writer.
    // 15x: libc.so;main;foo;bar
    // 12x: libc.so;main;foo;qux
    for (const auto& [str, count] : histo_) {
      LOG(INFO) << count << "x: " << str;
    }
    return Status::OK();
  }

  Status BuildHistogram() {
    const std::vector<TaggedRecordBatch> tablets = data_table_.ConsumeRecords();
    const uint64_t num_rows = tablets[0].records[0]->Size();

    auto stack_traces_column = tablets[0].records[kStackTraceStackTraceStrIdx];
    auto counts_column = tablets[0].records[kStackTraceCountIdx];

    // Build the stack traces histogram.
    for (uint64_t row_idx = 0; row_idx < num_rows; ++row_idx) {
      const std::string stack_trace_str = stack_traces_column->Get<types::StringValue>(row_idx);
      const int64_t count = counts_column->Get<types::Int64Value>(row_idx).val;
      histo_[stack_trace_str] += count;
    }
    return Status::OK();
  }

  Status StopTransferDataThread() {
    if (transfer_data_thread_.joinable()) {
      transfer_enable_ = false;
      {
        // Prevent transfer_data_thread_ from invoking TransferData().
        absl::base_internal::SpinLockHolder lock(&perf_profiler_state_lock_);

        // Join the thread.
        transfer_data_thread_.join();

        // Transfer any remaining data from eBPF to user space.
        source_->TransferData(ctx_.get());
      }
    }

    return Status::OK();
  }

  Status StartTransferDataThread() {
    if (FLAGS_pid == 0) {
      // No PID specified. Go with "system wide" context.
      ctx_ = std::make_unique<SystemWideStandaloneContext>();
    } else {
      // FLAGS_pid is set meaning the user wants to profile only a specific process.
      // We need to create a PID context for the profiler. The Pixie/Stirling project identifies
      // processes using UPID, meaning unique PID. A UPID is the tuple of the process start time,
      // the pid, and its address space id (i.e. which k8s host).

      // Get the process start time.
      PX_ASSIGN_OR_RETURN(const uint64_t ts, system::ProcParser().GetPIDStartTimeTicks(FLAGS_pid));

      // This profiler will run on whatever host the user runs it on, i.e. no need to disambiguate
      // using the address space id field in the UPID (thus we can use zero, or any other value).
      constexpr uint32_t kASID = 0;

      // The stand alone context requires a set of UPIDs. We have just one in that set.
      const absl::flat_hash_set<md::UPID> upids = {md::UPID(kASID, FLAGS_pid, ts)};

      // Create the context (which will result in filtering our stack traces by PID).
      ctx_ = std::make_unique<StandaloneContext>(upids);
    }

    // Create a thread to periodically read eBPF data.
    transfer_data_thread_ = std::thread([this]() {
      // This value can also be manipulated by the Stop() method.
      transfer_enable_ = true;

      while (transfer_enable_) {
        {
          // Prevent main thread from trying to join or stop the profiler while this is happening.
          absl::base_internal::SpinLockHolder lock(&perf_profiler_state_lock_);

          // Read the eBPF perf buffer and map that stores the profiling information.
          // Data from eBPF is transferred into member data_table_.
          source_->TransferData(ctx_.get());
        }

        // The eBPF maps are sized based on the the data transfer sampling period (with a little
        // margin). Thus, we wake up based on this periodicity to drain data from eBPF.
        std::this_thread::sleep_for(source_->SamplingPeriod());
      }
    });

    return Status::OK();
  }

 private:
  // The underlying profiler is a "Stirling Source Connector" i.e. a data source for the
  // Stirling data connector, part of the Pixie Kubernetes observability platform.
  // In specific, it creates an eBPF program that is periodically invoked to collect stack traces,
  // and it symbolizes those stack traces.
  std::unique_ptr<PerfProfileConnector> source_;

  // A data table for the profiler to populate. A full "Pixie Edge Module (pem)" has multiple
  // source connectors and connects them to data tables that are owned at the pem wrapper level.
  // To use a source connector, this program needs to re-create some of that pem functionality.
  DataTable data_table_;

  // Used in the ctor args for DataTable. Use id zero because profiler is the only thing here.
  static constexpr uint32_t kDataTableId = 0;

  // A local stack trace histo (for convenience, to be populated after all samples are collected).
  absl::flat_hash_map<std::string, uint64_t> histo_;

  // A thread that periodically wakes up to read the eBPF perf buffer and maps.
  std::thread transfer_data_thread_;

  // The lock and transfer enable flag are used by transfer_data_thread_.
  absl::base_internal::SpinLock perf_profiler_state_lock_;
  std::atomic<bool> transfer_enable_ = false;

  // The context (used by a Stirling source connector) can specify a set of processes to collect
  // data from. Here, we either create a "system wide" context (all processes) or use just one PID.
  std::unique_ptr<StandaloneContext> ctx_;
};

}  // namespace stirling
}  // namespace px

std::unique_ptr<px::stirling::Profiler> g_profiler;

void SignalHandler(int signum) {
  std::cerr << "\n\nStopping, might take a few seconds ..." << std::endl;

  // Important to call Stop(), because it releases eBPF resources,
  // which would otherwise leak.
  if (g_profiler != nullptr) {
    PX_UNUSED(g_profiler->StopTransferDataThread());
    PX_UNUSED(g_profiler->Stop());
  }

  exit(signum);
}

Status RunProfiler() {
  // Bring up eBPF.
  PX_RETURN_IF_ERROR(g_profiler->Init());

  // Separate thread to periodically wake up and read the eBPF perf buffer & maps.
  PX_RETURN_IF_ERROR(g_profiler->StartTransferDataThread());

  // Collect data for the user specified amount of time.
  std::this_thread::sleep_for(std::chrono::seconds(FLAGS_time));

  // Stop collecting data and do a final read out of eBPF perf buffer & maps.
  PX_RETURN_IF_ERROR(g_profiler->StopTransferDataThread());

  // Build the stack traces histogram.
  PX_RETURN_IF_ERROR(g_profiler->BuildHistogram());

  // Print the info. We will replace this with a pprof proto file write out.
  PX_RETURN_IF_ERROR(g_profiler->PrintData());

  // Cleanup.
  PX_RETURN_IF_ERROR(g_profiler->Stop());

  // Phew. We are outta here.
  return Status::OK();
}

int main(int argc, char** argv) {
  // Register signal handlers to clean-up on exit.
  signal(SIGHUP, SignalHandler);
  signal(SIGINT, SignalHandler);
  signal(SIGQUIT, SignalHandler);
  signal(SIGTERM, SignalHandler);

  px::EnvironmentGuard env_guard(&argc, argv);

  // Need to do this after env setup.
  g_profiler = std::make_unique<px::stirling::Profiler>();

  // Run the profiler (in more detail: setup, collect data, and tear down).
  const auto status = RunProfiler();

  // Something happened, log that.
  LOG_IF(WARNING, !status.ok()) << status.msg();

  return status.ok() ? 0 : -1;
}
