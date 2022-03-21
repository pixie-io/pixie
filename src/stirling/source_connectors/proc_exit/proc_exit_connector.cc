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

#include "src/stirling/source_connectors/proc_exit/proc_exit_connector.h"

#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/bpf_tools/utils.h"
#include "src/stirling/source_connectors/proc_exit/bcc_bpf_intf/proc_exit.h"

BPF_SRC_STRVIEW(proc_exit_trace_bcc_script, proc_exit_trace);

namespace px {
namespace stirling {
namespace proc_exit_tracer {

constexpr uint32_t kPerfBufferPerCPUSizeBytes = 5 * 1024 * 1024;

const auto kTracepointSpecs =
    MakeArray<bpf_tools::TracepointSpec>({{std::string("sched:sched_process_exit"),
                                           std::string("tracepoint__sched__sched_process_exit")}});

void HandleProcExitEvent(void* cb_cookie, void* data, int /*data_size*/) {
  auto* connector = reinterpret_cast<ProcExitConnector*>(cb_cookie);
  auto* event = reinterpret_cast<struct proc_exit_event_t*>(data);
  connector->AcceptProcExitEvent(*event);
}

void HandleProcExitEventLoss(void* /*cb_cookie*/, uint64_t /*lost*/) {
  // TODO(yzhao): Add stats counter.
}

const auto kPerfBufferSpecs = MakeArray<bpf_tools::PerfBufferSpec>({
    {"proc_exit_events", HandleProcExitEvent, HandleProcExitEventLoss, kPerfBufferPerCPUSizeBytes,
     bpf_tools::PerfBufferSizeCategory::kControl},
});

void ProcExitConnector::AcceptProcExitEvent(const struct proc_exit_event_t& event) {
  events_.push_back(event);
}

Status ProcExitConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);

  PL_RETURN_IF_ERROR(InitBPFProgram(proc_exit_trace_bcc_script));

  // Writes exit_code_offset to BPF array. Note that the other offsets are injected into BCC code
  // through macros.
  const auto& offset_opt = BCCWrapper::task_struct_offsets_opt();
  if (offset_opt.has_value()) {
    LOG(INFO) << absl::Substitute(
        "Writing exit_code's offset to BPF array: offset=$0 bpf_array=$1 index=$2",
        offset_opt.value().exit_code_offset, kProcExitControlValuesArrayName,
        TASK_STRUCT_EXIT_CODE_OFFSET_INDEX);
    auto control_values_table_handle =
        GetPerCPUArrayTable<uint64_t>(kProcExitControlValuesArrayName);
    PL_RETURN_IF_ERROR(bpf_tools::UpdatePerCPUArrayValue(TASK_STRUCT_EXIT_CODE_OFFSET_INDEX,
                                                         offset_opt.value().exit_code_offset,
                                                         &control_values_table_handle));
  }

  PL_RETURN_IF_ERROR(AttachTracepoints(kTracepointSpecs));
  PL_RETURN_IF_ERROR(OpenPerfBuffers(kPerfBufferSpecs, this));

  return Status::OK();
}

void ProcExitConnector::TransferDataImpl(ConnectorContext* ctx,
                                         const std::vector<DataTable*>& data_tables) {
  DCHECK(data_tables.size() == 1) << "Expect only one data table for proc_exit tracer";

  PollPerfBuffers();

  DataTable* data_table = data_tables[0];
  for (auto& event : events_) {
    event.timestamp_ns = ConvertToRealTime(event.timestamp_ns);
    DataTable::RecordBuilder<&kProcExitEventsTable> r(data_table, event.timestamp_ns);
    r.Append<proc_exit_tracer::kTimeIdx>(event.timestamp_ns);
    md::UPID upid(ctx->GetASID(), event.upid.pid, event.upid.start_time_ticks);
    r.Append<proc_exit_tracer::kUPIDIdx>(upid.value());
    // Exit code and signals are encoded in the exit_code field of the task_struct of the process.
    // See the description below:
    // https://unix.stackexchange.com/questions/99112/default-exit-code-when-process-is-terminated
    r.Append<proc_exit_tracer::kExitCodeIdx>(event.exit_code >> 8);
    r.Append<proc_exit_tracer::kSignalIdx>(event.exit_code & 0x7F);
    r.Append<proc_exit_tracer::kCommIdx>(std::move(event.comm));
  }
  events_.clear();
}

}  // namespace proc_exit_tracer
}  // namespace stirling
}  // namespace px
