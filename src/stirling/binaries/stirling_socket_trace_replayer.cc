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
#include "src/stirling/core/output.h"
#include "src/stirling/core/pub_sub_manager.h"
#include "src/stirling/core/unit_connector.h"
#include "src/stirling/proto/stirling.pb.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"

using ::px::Status;

DEFINE_uint32(time, 30, "Number of seconds to run the profiler.");
DEFINE_string(events_pb_file, "bpf_events_profile.pb", "Recorded BPF events.");
DECLARE_uint32(stirling_profiler_stack_trace_sample_period_ms);

namespace px {
namespace stirling {

class SocketTracerReplayer : public UnitConnector<SocketTraceConnector> {};

}  // namespace stirling
}  // namespace px

std::unique_ptr<px::stirling::SocketTracerReplayer> g_socket_tracer;

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

absl::flat_hash_map<uint64_t, px::stirling::stirlingpb::InfoClass> g_table_info_map;

#include "src/stirling/source_connectors/socket_tracer/cass_table.h"

Status StirlingWrapperCallback(uint64_t table_id, px::types::TabletID,
                               std::unique_ptr<px::types::ColumnWrapperRecordBatch> record_batch) {
  auto iter = g_table_info_map.find(table_id);
  if (iter == g_table_info_map.end()) {
    return px::error::Internal("Encountered unknown table id $0", table_id);
  }
  const px::stirling::stirlingpb::InfoClass& table_info = iter->second;

  std::cout << px::stirling::ToString(table_info.schema().name(), table_info.schema(),
                                      *record_batch);

  return Status::OK();
}

px::stirling::InfoClassManagerVec info_class_mgrs_;

Status RunSocketTracer() {
  // Set replaying mode.
  px::stirling::bpf_tools::RRSingleton::GetInstance().SetReplaying(FLAGS_events_pb_file);

  // Bring up eBPF.
  PX_RETURN_IF_ERROR(g_socket_tracer->Init());
  PX_RETURN_IF_ERROR(g_socket_tracer->SetSamplingPeriod(std::chrono::milliseconds{0}));

  //////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////
  auto underyling_source = g_socket_tracer->RawPtr();
  for (const auto& schema : underyling_source->table_schemas()) {
    LOG(INFO) << absl::Substitute("Adding info class: [$0/$1]", underyling_source->name(),
                                  schema.name());
    auto mgr = std::make_unique<px::stirling::InfoClassManager>(schema);
    mgr->SetSourceConnector(underyling_source);
    info_class_mgrs_.push_back(std::move(mgr));
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Get a publish proto message to subscribe from.
  px::stirling::stirlingpb::Publish publish_pb;
  PopulatePublishProto(&publish_pb, info_class_mgrs_);
  px::stirling::IndexPublication(publish_pb, &g_table_info_map);
  //////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////

  PX_RETURN_IF_ERROR(g_socket_tracer->RegisterDataPushCallback(StirlingWrapperCallback));
  //////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////

  PX_RETURN_IF_ERROR(g_socket_tracer->Start());

  while (!px::stirling::bpf_tools::RRSingleton::GetInstance().playback_complete()) {
    sleep(1);
  }

  // Stop collecting data and do a final read out of eBPF perf buffer & maps.
  PX_RETURN_IF_ERROR(g_socket_tracer->Stop());

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
  g_socket_tracer = std::make_unique<px::stirling::SocketTracerReplayer>();

  // Run the profiler (in more detail: setup, collect data, and tear down).
  const auto status = RunSocketTracer();

  // Something happened, log that.
  LOG_IF(WARNING, !status.ok()) << status.msg();

  // Cleanup.
  g_socket_tracer = nullptr;

  return status.ok() ? 0 : -1;
}
