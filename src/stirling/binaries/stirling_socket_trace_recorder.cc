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
#include "src/shared/upid/upid.h"
#include "src/stirling/bpf_tools/rr/rr.h"
#include "src/stirling/core/unit_connector.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"

using ::px::Status;

DEFINE_uint32(time, 30, "Number of seconds to run the profiler.");
DEFINE_string(events_pb_file, "bpf_events_profile.pb", "Recorded BPF events.");
DECLARE_uint32(stirling_profiler_stack_trace_sample_period_ms);

namespace px {
namespace stirling {

class SocketTracerRecorder : public UnitConnector<SocketTraceConnector> {
 public:
 private:
};

}  // namespace stirling
}  // namespace px

std::unique_ptr<px::stirling::SocketTracerRecorder> g_socket_tracer;

void SignalHandler(int signum) {
  std::cerr << "\n\nStopping, might take a few seconds ..." << std::endl;

  // Important to call Stop(), because it releases eBPF resources,
  // which would otherwise leak.
  if (g_socket_tracer != nullptr) {
    PX_UNUSED(g_socket_tracer->Stop());
    g_socket_tracer = nullptr;
  }

  exit(signum);
}

Status RunSocketTracer() {
  // Set recording mode.
  px::stirling::bpf_tools::RRSingleton::GetInstance().SetRecording();

  // Bring up eBPF.
  PX_RETURN_IF_ERROR(g_socket_tracer->Init());

  // Separate thread to periodically wake up and read the eBPF perf buffer & maps.
  PX_RETURN_IF_ERROR(g_socket_tracer->Start());

  // Collect data for the user specified amount of time.
  sleep(FLAGS_time);

  // Stop collecting data and do a final read out of eBPF perf buffer & maps.
  PX_RETURN_IF_ERROR(g_socket_tracer->Stop());

  // Write a pprof proto file.
  px::stirling::bpf_tools::RRSingleton::GetInstance().WriteProto(FLAGS_events_pb_file);

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
  g_socket_tracer = std::make_unique<px::stirling::SocketTracerRecorder>();

  // Run the profiler (in more detail: setup, collect data, and tear down).
  const auto status = RunSocketTracer();

  // Something happened, log that.
  LOG_IF(WARNING, !status.ok()) << status.msg();

  // Cleanup.
  g_socket_tracer = nullptr;

  return status.ok() ? 0 : -1;
}
