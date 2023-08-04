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

#include <signal.h>

#include <utility>

#include "src/common/base/base.h"
#include "src/common/metrics/metrics.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/bpf_tools/utils.h"
#include "src/stirling/source_connectors/proc_exit/bcc_bpf_intf/proc_exit.h"
#include "src/stirling/utils/detect_application.h"
#include "src/stirling/utils/proc_tracker.h"

OBJ_STRVIEW(proc_exit_trace_bcc_script, proc_exit_trace);

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

// Use char array to meet the user's interface, which expects std::string.
constexpr char kJavaProcCrashedCounter[] = "java_proc_crashed";
constexpr char kJavaProcCrashedWithProfilerCounter[] = "java_proc_crashed_with_profiler";
constexpr char kJavaProcCrashedWithoutProfilerCounter[] = "java_proc_crashed_without_profiler";

ProcExitConnector::ProcExitConnector(std::string_view name)
    : SourceConnector(name, kTables),
      java_proc_crashed_counter_(
          BuildCounter(kJavaProcCrashedCounter, "Count of the crashed Java processes")),
      java_proc_crashed_with_profiler_counter_(BuildCounter(
          kJavaProcCrashedWithProfilerCounter,
          "Count of the crashed Java processes that also had profiler agent attached")),
      java_proc_crashed_without_profiler_counter_(BuildCounter(
          kJavaProcCrashedWithoutProfilerCounter,
          "Count of the crashed Java processes that hadn't attached profiler agent")) {}

void ProcExitConnector::AcceptProcExitEvent(const struct proc_exit_event_t& event) {
  events_.push_back(event);
}

Status ProcExitConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);

  PX_RETURN_IF_ERROR(InitBPFProgram(proc_exit_trace_bcc_script));

  // Writes exit_code_offset to BPF array. Note that the other offsets are injected into BCC code
  // through macros.
  const auto& offset_opt = BCCWrapper::task_struct_offsets_opt();
  if (offset_opt.has_value()) {
    LOG(INFO) << absl::Substitute(
        "Writing exit_code's offset to BPF array: offset=$0 bpf_array=$1 index=$2",
        offset_opt.value().exit_code_offset, kProcExitControlValuesArrayName,
        TASK_STRUCT_EXIT_CODE_OFFSET_INDEX);
    auto control_table = bpf_tools::WrappedBCCPerCPUArrayTable<uint64_t>::Create(
        this, kProcExitControlValuesArrayName);
    PX_RETURN_IF_ERROR(control_table->SetValues(TASK_STRUCT_EXIT_CODE_OFFSET_INDEX,
                                                offset_opt.value().exit_code_offset));
  }

  const auto perf_buffer_specs = MakeArray<bpf_tools::PerfBufferSpec>({
      {"proc_exit_events", HandleProcExitEvent, HandleProcExitEventLoss, kPerfBufferPerCPUSizeBytes,
       bpf_tools::PerfBufferSizeCategory::kControl},
  });

  PX_RETURN_IF_ERROR(AttachTracepoints(kTracepointSpecs));
  PX_RETURN_IF_ERROR(OpenPerfBuffers(perf_buffer_specs, this));

  return Status::OK();
}

namespace {

uint8_t GetExitSignal(uint32_t exit_code) { return exit_code & 0x7F; }

}  // namespace

void ProcExitConnector::TransferDataImpl(ConnectorContext* ctx) {
  DCHECK(data_tables_.size() == 1) << "Expect only one data table for proc_exit tracer";

  PollPerfBuffers();

  DataTable* data_table = data_tables_[0];
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
    r.Append<proc_exit_tracer::kSignalIdx>(GetExitSignal(event.exit_code));
    r.Append<proc_exit_tracer::kCommIdx>(std::move(event.comm));

    UpdateCrashedJavaProcCounters(ctx->GetASID(), event, ctx->GetPIDInfoMap());
  }
  events_.clear();
}

void ProcExitConnector::UpdateCrashedJavaProcCounters(
    uint32_t asid, const proc_exit_event_t& event,
    const absl::flat_hash_map<md::UPID, md::PIDInfoUPtr>& upid_pid_info_map) {
  const uint8_t exit_signal = GetExitSignal(event.exit_code);

  const bool is_sig_abrt = exit_signal == SIGABRT;
  const bool is_sig_segv = exit_signal == SIGSEGV;
  const bool is_sig_fpe = exit_signal == SIGFPE;
  const bool is_sig_ill = exit_signal == SIGILL;
  const bool is_sig_bus = exit_signal == SIGBUS;
  const bool is_crash = is_sig_abrt || is_sig_segv || is_sig_fpe || is_sig_ill || is_sig_bus;

  if (!is_crash) {
    // Normal exits are ignored.
    return;
  }

  md::UPID md_upid = event.upid.ToMetadataUPID(asid);
  auto iter = upid_pid_info_map.find(md_upid);
  if (
      // Don't track any non-K8s process. upid_pid_info_map was obtained from K8s metadata, which
      // indicates whether or not a process is managed by K8s.
      iter == upid_pid_info_map.end() ||
      // Missing PIDInfo
      iter->second == nullptr) {
    return;
  }
  const md::PIDInfo& pid_info = *iter->second;
  if (DetectApplication(pid_info.exe_path()) != Application::kJava) {
    return;
  }

  java_proc_crashed_counter_.Increment();
  monitor_.NotifyJavaProcessCrashed(event.upid);

  if (JavaProfilingProcTracker::GetSingleton()->upids().contains(event.upid)) {
    java_proc_crashed_with_profiler_counter_.Increment();
  } else {
    java_proc_crashed_without_profiler_counter_.Increment();
  }
}

}  // namespace proc_exit_tracer
}  // namespace stirling
}  // namespace px
