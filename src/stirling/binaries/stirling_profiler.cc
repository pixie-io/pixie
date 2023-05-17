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

#include <sys/sysinfo.h>

#include <csignal>
#include <iostream>
#include <thread>

#include "src/common/base/base.h"
#include "src/shared/pprof/pprof.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/core/unit_connector.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"

using ::px::Status;

DEFINE_uint32(time, 30, "Number of seconds to run the profiler.");
DEFINE_string(pprof_pb_file, "profile.pb", "File path for pprof protobuf output.");
DECLARE_uint32(stirling_profiler_stack_trace_sample_period_ms);

namespace px {
namespace stirling {

class Profiler : public UnitConnector<PerfProfileConnector> {
 public:
  Status WritePProf() {
    // Build stack traces histogram.
    PX_RETURN_IF_ERROR(BuildHistogram());

    // Create the pprof profile.
    const uint32_t period_ms = FLAGS_stirling_profiler_stack_trace_sample_period_ms;
    const auto pprof_pb = px::shared::CreatePProfProfile(period_ms, histo_);

    // Write the pprof profile to disk.
    std::fstream outfile(FLAGS_pprof_pb_file, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!outfile.is_open()) {
      char const* const err_msg = "Failed to open output file: $0.";
      return error::Internal(absl::Substitute(err_msg, FLAGS_pprof_pb_file));
    }

    if (!pprof_pb.SerializeToOstream(&outfile)) {
      char const* const err_msg = "Failed to write pprof protobuf to file: $0.";
      return error::Internal(absl::Substitute(err_msg, FLAGS_pprof_pb_file));
    }
    return Status::OK();
  }

 private:
  Status BuildHistogram() {
    PX_ASSIGN_OR_RETURN(const auto& records, ConsumeRecords(0));

    const uint64_t num_rows = records[kStackTraceStackTraceStrIdx]->Size();
    const auto traces_column = records[kStackTraceStackTraceStrIdx];
    const auto counts_column = records[kStackTraceCountIdx];

    // Build the stack traces histogram.
    for (uint64_t row_idx = 0; row_idx < num_rows; ++row_idx) {
      const std::string& stack_trace_str = traces_column->Get<types::StringValue>(row_idx);
      const int64_t count = counts_column->Get<types::Int64Value>(row_idx).val;
      histo_[stack_trace_str] += count;
    }
    return Status::OK();
  }

  // A local stack trace histo (for convenience, to be populated after all samples are collected).
  absl::flat_hash_map<std::string, uint64_t> histo_;
};

}  // namespace stirling
}  // namespace px

std::unique_ptr<px::stirling::Profiler> g_profiler;

void SignalHandler(int signum) {
  std::cerr << "\n\nStopping, might take a few seconds ..." << std::endl;

  // Important to call Stop(), because it releases eBPF resources,
  // which would otherwise leak.
  if (g_profiler != nullptr) {
    PX_UNUSED(g_profiler->Stop());
    g_profiler = nullptr;
  }

  exit(signum);
}

Status RunProfiler() {
  // Bring up eBPF.
  PX_RETURN_IF_ERROR(g_profiler->Init());

  // Separate thread to periodically wake up and read the eBPF perf buffer & maps.
  PX_RETURN_IF_ERROR(g_profiler->Start());

  // Collect data for the user specified amount of time.
  sleep(FLAGS_time);

  // Stop collecting data and do a final read out of eBPF perf buffer & maps.
  PX_RETURN_IF_ERROR(g_profiler->Stop());

  // Write a pprof proto file.
  PX_RETURN_IF_ERROR(g_profiler->WritePProf());

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

  // Cleanup.
  g_profiler = nullptr;

  return status.ok() ? 0 : -1;
}
