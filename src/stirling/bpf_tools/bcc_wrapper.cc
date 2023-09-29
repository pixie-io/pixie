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

#include "src/stirling/bpf_tools/bcc_wrapper.h"

#include <linux/perf_event.h>
#include <sys/mount.h>

#include <iostream>
#include <string>

#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/perf/scoped_timer.h"
#include "src/common/system/config.h"
#include "src/common/system/kernel_version.h"
#include "src/stirling/bpf_tools/rr/rr.h"
#include "src/stirling/bpf_tools/task_struct_resolver.h"
#include "src/stirling/utils/linux_headers.h"

namespace px {
namespace stirling {
namespace bpf_tools {

// TODO(yzhao): Read CPU count during runtime and set maxactive to Multiplier * N_CPU. That way, we
// can be relatively more secure against increase of CPU count. Note the default multiplier is 2,
// which is not sufficient, as indicated in Hipster shop.
//
// AWS offers VMs with 96 vCPUs. We bump the number from 2X to 4X, and round up to 2's exponential,
// which gives 4 * 96 == 384 => 512.
//
// The implication of this parameter is explained in the "How Does a Return Probe Work?" section of
// https://www.kernel.org/doc/Documentation/kprobes.txt. In short, this controls the memory space
// used for bookkeeping, which translate to equal number of struct kretprobe in memory.
constexpr int kKprobeMaxActive = 512;

// BCC requires debugfs to be mounted to deploy BPF programs.
// Most kernels already have this mounted, but some do not.
// See https://github.com/iovisor/bcc/blob/master/INSTALL.md.
Status MountDebugFS() {
  std::filesystem::path sys_kernel_debug("/sys/kernel/debug");

  // If the directory is empty, debugfs needs to be mounted.
  PX_ASSIGN_OR_RETURN(bool is_empty, fs::IsEmpty(sys_kernel_debug));
  if (is_empty) {
    LOG(INFO) << absl::Substitute("Debugfs not mounted at $0. Attempting to mount now.",
                                  sys_kernel_debug.string());
    int status = mount("debugfs", sys_kernel_debug.c_str(), "debugfs", /* mountflags */ 0,
                       /* data */ nullptr);
    if (status == -1) {
      return error::Internal("Mount of debugfs failed (required for BCC): $0", strerror(errno));
    }
  }

  return Status::OK();
}

StatusOr<utils::TaskStructOffsets> ResolveTaskStructOffsetsWithRetry() {
  constexpr int kNumAttempts = 3;

  StatusOr<utils::TaskStructOffsets> offsets_status;
  for (int attempt = 0; attempt < kNumAttempts; ++attempt) {
    offsets_status = utils::ResolveTaskStructOffsets();
    if (offsets_status.ok()) {
      break;
    }
  }
  return offsets_status;
}

StatusOr<utils::TaskStructOffsets> BCCWrapper::ComputeTaskStructOffsets() {
  if (task_struct_offsets_opt_.has_value()) {
    LOG(INFO) << "Returning the previously resolved TaskStructOffsets object";
    return task_struct_offsets_opt_.value();
  }

  LOG(INFO) << "Resolving task_struct offsets.";
  PX_ASSIGN_OR_RETURN(task_struct_offsets_opt_, ResolveTaskStructOffsetsWithRetry());

  LOG(INFO) << absl::Substitute("Successfully resolved task_struct offsets: $0",
                                task_struct_offsets_opt_.value().ToString());
  return task_struct_offsets_opt_.value();
}

Status BCCWrapperImpl::InitBPFProgram(std::string_view bpf_program, std::vector<std::string> cflags,
                                      bool requires_linux_headers,
                                      bool always_infer_task_struct_offsets) {
  using utils::TaskStructOffsets;

  if (!IsRoot()) {
    return error::PermissionDenied("BCC currently only supported as the root user.");
  }

  if (requires_linux_headers) {
    auto kernel_version = system::GetCachedKernelVersion();

    // This function will setup linux headers for BPF code deployment. If another BCCWrapper has
    // already run this function, it will find the same headers as found or installed previously.
    // NOTE: Considered calling from  Stirling Init(), but this requires test cases to explicitly
    // call FindOrInstallLinuxHeaders(), thus it is deemed to be better here.
    PX_RETURN_IF_ERROR(utils::FindOrInstallLinuxHeaders());

    // When Linux headers are requested, the BPF code requires various defines to compile:
    //  - START_BOOTTIME_VARNAME: The name of the task_struct variable containing the boottime.
    //                            Prior to Linux 5.5, it was called real_start_time.
    //                            Linux 5.5+ calls it start_boottime.
    //  - GROUP_LEADER_OFFSET_OVERRIDE: When non-zero, this tells BPF how to access
    //                                  task_struct->group_leader via offsets.
    //  - START_BOOTTIME_OFFSET_OVERRIDE: When non-zero, this tells BPF how to access
    //                                    task_struct->start_boottime via offsets.

    constexpr uint32_t kLinux5p5VersionCode = 328960;
    std::string_view boottime_varname =
        kernel_version.code() >= kLinux5p5VersionCode ? "start_boottime" : "real_start_time";

    // Defaults to zero offsets, which tells BPF not to use the offset overrides, and instead to use
    // the offset implied in the included Linux headers.
    utils::TaskStructOffsets offsets = {};
    // When using packaged Linux headers, there is a good chance that the `struct task_struct`
    // is not a perfect match with the version on the host OS (despite our best efforts to account
    // for the kernel config). In such cases, try to resolve the location of the fields we care
    // about, and send them in as an override to the BPF code.
    // Note that if we found local host headers, then we do not typically do this step, because
    // we trust the locally installed headers to be a perfect match.
    // There is a flag to force the task struct fields resolution, in case we don't trust the
    // local headers, and for testing purposes.
    if (utils::g_packaged_headers_installed || always_infer_task_struct_offsets) {
      auto offsets_or = ComputeTaskStructOffsets();
      if (offsets_or.ok()) {
        offsets = offsets_or.ConsumeValueOrDie();
      } else {
        LOG(WARNING) << absl::Substitute(
            "Failed to obtain task_struct offsets, will not override the task_struct offsets, "
            "error: $0",
            offsets_or.ToString());
      }
    }

    cflags.push_back(absl::Substitute("-DSTART_BOOTTIME_VARNAME=$0", boottime_varname));
    cflags.push_back(
        absl::Substitute("-DGROUP_LEADER_OFFSET_OVERRIDE=$0", offsets.group_leader_offset));
    cflags.push_back(
        absl::Substitute("-DSTART_BOOTTIME_OFFSET_OVERRIDE=$0", offsets.real_start_time_offset));
  }

  PX_RETURN_IF_ERROR(MountDebugFS());

  {
    LOG(INFO) << "Initializing BPF program ...";
    ScopedTimer timer("init_bpf_program");
    auto init_res = bpf_.init(std::string(bpf_program), cflags);
    if (!init_res.ok()) {
      return error::Internal("Unable to initialize BCC BPF program: $0", init_res.msg());
    }
  }
  return Status::OK();
}

Status BCCWrapperImpl::AttachKProbe(const KProbeSpec& probe) {
  VLOG(1) << "Deploying kprobe: " << probe.ToString();
  DCHECK(probe.attach_type != BPFProbeAttachType::kReturnInsts);

  auto status =
      bpf_.attach_kprobe(GetKProbeTargetName(probe), std::string(probe.probe_fn), 0 /* offset */,
                         static_cast<bpf_probe_attach_type>(probe.attach_type), kKprobeMaxActive);

  // Don't return error if the probe is optional.
  if (!probe.is_optional) {
    PX_RETURN_IF_ERROR(status);
  }

  if (status.ok()) {
    kprobes_.push_back(probe);
    ++num_attached_kprobes_;
  }
  return Status::OK();
}

Status BCCWrapperImpl::AttachTracepoint(const TracepointSpec& probe) {
  VLOG(1) << "Deploying tracepoint: " << probe.ToString();

  PX_RETURN_IF_ERROR(bpf_.attach_tracepoint(probe.tracepoint, probe.probe_fn));
  tracepoints_.push_back(probe);
  ++num_attached_tracepoints_;
  return Status::OK();
}

Status BCCWrapperImpl::AttachUProbe(const UProbeSpec& probe) {
  VLOG(1) << "Deploying uprobe: " << probe.ToString();
  // TODO(oazizi): Natively support this attach type in BCCWrapper.
  DCHECK(probe.attach_type != BPFProbeAttachType::kReturnInsts);
  DCHECK((probe.symbol.empty() && probe.address != 0) ||
         (!probe.symbol.empty() && probe.address == 0))
      << "Exactly one of 'symbol' and 'address' must be specified.";
  auto status = bpf_.attach_uprobe(
      probe.binary_path, probe.symbol, std::string(probe.probe_fn), probe.address,
      static_cast<bpf_probe_attach_type>(probe.attach_type), probe.pid);
  if (!probe.is_optional) {
    PX_RETURN_IF_ERROR(status);
  }

  if (status.ok()) {
    uprobes_.push_back(probe);
    ++num_attached_uprobes_;
  }
  return Status::OK();
}

Status BCCWrapperImpl::AttachSamplingProbe(const SamplingProbeSpec& probe) {
  constexpr uint64_t kNanosPerMilli = 1000 * 1000;
  const uint64_t sample_period = probe.period_millis * kNanosPerMilli;
  // A sampling probe is just a PerfEventProbe, where the perf event is a clock counter.
  // When a requisite number of clock samples occur, the kernel will trigger the BPF code.
  // By specifying a frequency, the kernel will attempt to adjust the threshold to achieve
  // the desired sampling frequency.
  PerfEventSpec perf_event_spec{.type = PERF_TYPE_SOFTWARE,
                                .config = PERF_COUNT_SW_CPU_CLOCK,
                                .probe_fn = probe.probe_fn,
                                .sample_period = sample_period};

  return AttachPerfEvent(perf_event_spec);
}

Status BCCWrapperImpl::AttachKProbes(const ArrayView<KProbeSpec>& probes) {
  for (const KProbeSpec& p : probes) {
    PX_RETURN_IF_ERROR(AttachKProbe(p));
  }
  return Status::OK();
}

Status BCCWrapperImpl::AttachTracepoints(const ArrayView<TracepointSpec>& probes) {
  for (const TracepointSpec& spec : probes) {
    PX_RETURN_IF_ERROR(AttachTracepoint(spec));
  }
  return Status::OK();
}

Status BCCWrapperImpl::AttachUProbes(const ArrayView<UProbeSpec>& probes) {
  for (const UProbeSpec& p : probes) {
    PX_RETURN_IF_ERROR(AttachUProbe(p));
  }
  return Status::OK();
}

Status BCCWrapperImpl::AttachSamplingProbes(const ArrayView<SamplingProbeSpec>& probes) {
  for (const SamplingProbeSpec& p : probes) {
    PX_RETURN_IF_ERROR(AttachSamplingProbe(p));
  }
  return Status::OK();
}

// This will replace the XDP program previously-attached on the the same device.
// Newer kernel allows attaching multiple XDP programs on the same device:
// https://lwn.net/Articles/801478/
Status BCCWrapperImpl::AttachXDP(const std::string& dev_name, const std::string& fn_name) {
  int fn_fd = -1;
  ebpf::StatusTuple load_status = bpf_.load_func(fn_name, BPF_PROG_TYPE_XDP, fn_fd);

  if (!load_status.ok()) {
    return StatusAdapter(load_status);
  }

  int res_fd = bpf_attach_xdp(dev_name.c_str(), fn_fd, /*flags*/ 0);

  if (res_fd < 0) {
    bpf_.unload_func(fn_name);
    return error::Internal("Unable to attach xdp program for device $0 using $1, errorno: $2",
                           dev_name, fn_name, res_fd);
  }

  return Status::OK();
}

// TODO(PL-1294): This can fail in rare cases. See the cited issue. Find the root cause.
Status BCCWrapperImpl::DetachKProbe(const KProbeSpec& probe) {
  VLOG(1) << "Detaching kprobe: " << probe.ToString();
  PX_RETURN_IF_ERROR(bpf_.detach_kprobe(GetKProbeTargetName(probe),
                                        static_cast<bpf_probe_attach_type>(probe.attach_type)));
  --num_attached_kprobes_;
  return Status::OK();
}

Status BCCWrapperImpl::DetachUProbe(const UProbeSpec& probe) {
  VLOG(1) << "Detaching uprobe " << probe.ToString();

  if (fs::Exists(probe.binary_path)) {
    PX_RETURN_IF_ERROR(bpf_.detach_uprobe(probe.binary_path, probe.symbol, probe.address,
                                          static_cast<bpf_probe_attach_type>(probe.attach_type),
                                          probe.pid));
  }
  --num_attached_uprobes_;
  return Status::OK();
}

Status BCCWrapperImpl::DetachTracepoint(const TracepointSpec& probe) {
  VLOG(1) << "Detaching tracepoint " << probe.ToString();

  PX_RETURN_IF_ERROR(bpf_.detach_tracepoint(probe.tracepoint));

  --num_attached_tracepoints_;
  return Status::OK();
}

void BCCWrapperImpl::DetachKProbes() {
  for (const auto& p : kprobes_) {
    auto res = DetachKProbe(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  kprobes_.clear();
}

void BCCWrapperImpl::DetachUProbes() {
  for (const auto& p : uprobes_) {
    auto res = DetachUProbe(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  uprobes_.clear();
}

void BCCWrapperImpl::DetachTracepoints() {
  for (const auto& t : tracepoints_) {
    auto res = DetachTracepoint(t);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  tracepoints_.clear();
}

int BCCWrapperImpl::CommonPerfBufferSetup(const PerfBufferSpec& perf_buffer_spec) {
  DCHECK(perf_buffer_spec.cb_cookie != nullptr) << "perf_buffer_spec.cb_cookie must be non-null.";
  DCHECK(perf_buffer_spec.size_bytes > 0) << "perf_buffer_spec.cb_cookie must greater than zero.";
  perf_buffer_specs_.push_back(perf_buffer_spec);

  const int page_size_bytes = system::Config::GetInstance().PageSizeBytes();
  const int required_num_pages = IntRoundUpDivide(perf_buffer_spec.size_bytes, page_size_bytes);

  // Perf buffers must be sized to a power of 2.
  const int num_pages = IntRoundUpToPow2(required_num_pages);

  VLOG(1) << absl::Substitute(
      "Opening perf buffer: [$0] [allocated_num_pages=$1 allocated_size_bytes=$2] (per cpu)",
      perf_buffer_spec.ToString(), num_pages, num_pages * page_size_bytes);

  return num_pages;
}

Status BCCWrapperImpl::OpenPerfBuffer(const PerfBufferSpec& perf_buffer_spec) {
  const int num_pages = CommonPerfBufferSetup(perf_buffer_spec);

  const std::string& name = perf_buffer_spec.name;
  void* cb_cookie = perf_buffer_spec.cb_cookie;
  auto& data_fn = perf_buffer_spec.probe_output_fn;
  auto& loss_fn = perf_buffer_spec.probe_loss_fn;

  PX_RETURN_IF_ERROR(bpf_.open_perf_buffer(name, data_fn, loss_fn, cb_cookie, num_pages));

  ++num_open_perf_buffers_;
  return Status::OK();
}

Status BCCWrapperImpl::OpenPerfBuffers(const ArrayView<PerfBufferSpec>& perf_buffers) {
  for (const PerfBufferSpec& p : perf_buffers) {
    PX_RETURN_IF_ERROR(OpenPerfBuffer(p));
  }
  return Status::OK();
}

Status BCCWrapperImpl::ClosePerfBuffer(const PerfBufferSpec& perf_buffer) {
  VLOG(1) << "Closing perf buffer: " << perf_buffer.name;
  PX_RETURN_IF_ERROR(bpf_.close_perf_buffer(std::string(perf_buffer.name)));
  --num_open_perf_buffers_;
  return Status::OK();
}

void BCCWrapperImpl::ClosePerfBuffers() {
  for (const auto& p : perf_buffer_specs_) {
    auto res = ClosePerfBuffer(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  perf_buffer_specs_.clear();
}

Status BCCWrapperImpl::AttachPerfEvent(const PerfEventSpec& perf_event) {
  VLOG(1) << absl::Substitute("Attaching perf event:\n   type=$0\n   probe_fn=$1",
                              magic_enum::enum_name(perf_event.type), perf_event.probe_fn);
  PX_RETURN_IF_ERROR(bpf_.attach_perf_event(perf_event.type, perf_event.config,
                                            std::string(perf_event.probe_fn),
                                            perf_event.sample_period, 0));
  perf_events_.push_back(perf_event);
  ++num_attached_perf_events_;
  return Status::OK();
}

Status BCCWrapperImpl::AttachPerfEvents(const ArrayView<PerfEventSpec>& perf_events) {
  for (const PerfEventSpec& p : perf_events) {
    PX_RETURN_IF_ERROR(AttachPerfEvent(p));
  }
  return Status::OK();
}

Status BCCWrapperImpl::DetachPerfEvent(const PerfEventSpec& perf_event) {
  VLOG(1) << absl::Substitute("Detaching perf event:\n   type=$0\n   probe_fn=$1",
                              magic_enum::enum_name(perf_event.type), perf_event.probe_fn);
  PX_RETURN_IF_ERROR(bpf_.detach_perf_event(perf_event.type, perf_event.config));
  --num_attached_perf_events_;
  return Status::OK();
}

void BCCWrapperImpl::DetachPerfEvents() {
  for (const PerfEventSpec& p : perf_events_) {
    auto res = DetachPerfEvent(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  perf_events_.clear();
}

std::string BCCWrapperImpl::GetKProbeTargetName(const KProbeSpec& probe) {
  auto target = std::string(probe.kernel_fn);
  if (probe.is_syscall) {
    target = bpf_.get_syscall_fnname(target);
  }
  return target;
}

Status BCCWrapperImpl::PollPerfBuffer(const std::string& name, const int timeout_ms) {
  auto perf_buffer = bpf_.get_perf_buffer(name);
  if (perf_buffer == nullptr) {
    return error::NotFound(absl::Substitute("Perf buffer \"$0\" not found.", name));
  }
  perf_buffer->poll(timeout_ms);
  return Status::OK();
}

void BCCWrapperImpl::PollPerfBuffers(const int timeout_ms) {
  for (const auto& spec : perf_buffer_specs_) {
    const auto s = PollPerfBuffer(spec.name, timeout_ms);
    LOG_IF(ERROR, !s.ok()) << s.msg();
  }
}

void BCCWrapperImpl::Close() {
  DetachPerfEvents();
  ClosePerfBuffers();
  DetachKProbes();
  DetachUProbes();
  DetachTracepoints();
}

Status RecordingBCCWrapperImpl::OpenPerfBuffer(const PerfBufferSpec& perf_buffer_spec) {
  PerfBufferSpec pbs(perf_buffer_spec);
  pbs.recorder = recorder_.get();

  const int num_pages = CommonPerfBufferSetup(pbs);

  const std::string name = std::string(pbs.name);
  void* cb_cookie = &perf_buffer_specs_[num_open_perf_buffers_];
  auto data_fn = &RecordPerfBufferEvent;
  auto loss_fn = &RecordPerfBufferLoss;

  PX_RETURN_IF_ERROR(bpf_.open_perf_buffer(name, data_fn, loss_fn, cb_cookie, num_pages));
  ++num_open_perf_buffers_;

  return Status::OK();
}

std::unique_ptr<BCCWrapper> CreateBCC() { return std::make_unique<BCCWrapperImpl>(); }

std::unique_ptr<WrappedBCCStackTable> WrappedBCCStackTable::Create(bpf_tools::BCCWrapper* bcc,
                                                                   const std::string& name) {
  using BaseT = WrappedBCCStackTable;
  using ImplT = WrappedBCCStackTableImpl;
  using RecordingT = RecordingWrappedBCCStackTableImpl;
  using ReplayingT = ReplayingWrappedBCCStackTableImpl;
  return CreateBCCWrappedMapOrArray<BaseT, ImplT, RecordingT, ReplayingT>(bcc, name);
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
