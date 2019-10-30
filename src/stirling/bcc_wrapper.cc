#ifdef __linux__

#include <linux/perf_event.h>
#include <linux/sched.h>

#include <unistd.h>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>

#include "src/common/base/base.h"
#include "src/stirling/bcc_wrapper.h"

// TODO(yzhao): Do we need make this flag able to specify size for individual perf buffers?
// At least, we should have different values for ones used for transferring data, and metadata.
//
// Assume a moderate network bandwidth peak of 10MiB/s for any socket connection,
// and a sampling cycle of 10ms, that translate to 25.6 4KiB pages.
// And our current sampling cycle is 100ms, and requires 10*25.6==256 pages.
// Then the perf buffer consumes 1MiB each, and we have 4 of them, which is 4MiB in total,
// that should be OK for our current beta customers.
DEFINE_uint32(stirling_bpf_perf_buffer_page_count, 256,
              "The size of the perf buffers, in number of memory pages.");

DEFINE_bool(stirling_bpf_enable_logging, false, "If true, BPF logging facilities are enabled.");

namespace pl {
namespace stirling {

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

Status BCCWrapper::InitBPFCode(const std::vector<std::string>& cflags) {
  if (!IsRoot()) {
    return error::PermissionDenied("BCC currently only supported as the root user.");
  }
  auto init_res = bpf_.init(std::string(bpf_program_), cflags);
  if (init_res.code() != 0) {
    return error::Internal("Unable to initialize BCC BPF program: $0", init_res.msg());
  }
  if (FLAGS_stirling_bpf_enable_logging) {
    PL_RETURN_IF_ERROR(InitLogging());
  }
  return Status::OK();
}

Status BCCWrapper::AttachProbe(const KProbeSpec& probe) {
  ebpf::StatusTuple attach_status = bpf_.attach_kprobe(
      bpf_.get_syscall_fnname(std::string(probe.kernel_fn_short_name)),
      std::string(probe.trace_fn_name), 0 /* offset */, probe.attach_type, kKprobeMaxActive);
  if (attach_status.code() != 0) {
    return error::Internal("Failed to attach kprobe to kernel function: $0, error message: $1",
                           probe.kernel_fn_short_name, attach_status.msg());
  }
  probes_.push_back(probe);
  ++num_attached_probes_;
  return Status::OK();
}

Status BCCWrapper::AttachProbes(const ArrayView<KProbeSpec>& probes) {
  // TODO(yzhao): We need to clean the already attached probes after encountering a failure.
  for (const KProbeSpec& p : probes) {
    PL_RETURN_IF_ERROR(AttachProbe(p));
  }
  return Status::OK();
}

Status BCCWrapper::DetachProbe(const KProbeSpec& probe) {
  ebpf::StatusTuple detach_status = bpf_.detach_kprobe(
      bpf_.get_syscall_fnname(std::string(probe.kernel_fn_short_name)), probe.attach_type);

  if (detach_status.code() != 0) {
    return error::Internal("Failed to detach kprobe to kernel function: $0, error message: $1",
                           probe.kernel_fn_short_name, detach_status.msg());
  }
  --num_attached_probes_;
  return Status::OK();
}

void BCCWrapper::DetachProbes() {
  for (const KProbeSpec& p : probes_) {
    auto res = DetachProbe(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  probes_.clear();
}

Status BCCWrapper::OpenPerfBuffer(const PerfBufferSpec& perf_buffer, void* cb_cookie) {
  ebpf::StatusTuple open_status = bpf_.open_perf_buffer(
      std::string(perf_buffer.name), perf_buffer.probe_output_fn, perf_buffer.probe_loss_fn,
      cb_cookie, FLAGS_stirling_bpf_perf_buffer_page_count);
  if (open_status.code() != 0) {
    return error::Internal("Failed to open perf buffer: $0, error message: $1", perf_buffer.name,
                           open_status.msg());
  }
  perf_buffers_.push_back(perf_buffer);
  ++num_open_perf_buffers_;
  return Status::OK();
}

Status BCCWrapper::OpenPerfBuffers(const ArrayView<PerfBufferSpec>& perf_buffers, void* cb_cookie) {
  for (const PerfBufferSpec& p : perf_buffers) {
    PL_RETURN_IF_ERROR(OpenPerfBuffer(p, cb_cookie));
  }
  return Status::OK();
}

Status BCCWrapper::ClosePerfBuffer(const PerfBufferSpec& perf_buffer) {
  ebpf::StatusTuple close_status = bpf_.close_perf_buffer(std::string(perf_buffer.name));
  if (close_status.code() != 0) {
    return error::Internal("Failed to close perf buffer: $0, error message: $1", perf_buffer.name,
                           close_status.msg());
  }
  --num_open_perf_buffers_;
  return Status::OK();
}

void BCCWrapper::ClosePerfBuffers() {
  for (const PerfBufferSpec& p : perf_buffers_) {
    auto res = ClosePerfBuffer(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  perf_buffers_.clear();
}

Status BCCWrapper::AttachPerfEvent(const PerfEventSpec& perf_event) {
  auto attach_res = bpf_.attach_perf_event(perf_event.event_type, perf_event.event_config,
                                           std::string(perf_event.probe_func),
                                           perf_event.sample_period, perf_event.sample_freq);
  if (attach_res.code() != 0) {
    return error::Internal("Unable to attach perf event, error message $0", attach_res.msg());
  }
  perf_events_.push_back(perf_event);
  ++num_attached_perf_events_;
  return Status::OK();
}

Status BCCWrapper::AttachPerfEvents(const ArrayView<PerfEventSpec>& perf_events) {
  for (const PerfEventSpec& p : perf_events) {
    PL_RETURN_IF_ERROR(AttachPerfEvent(p));
  }
  return Status::OK();
}

Status BCCWrapper::DetachPerfEvent(const PerfEventSpec& perf_event) {
  auto detach_res = bpf_.detach_perf_event(perf_event.event_type, perf_event.event_config);
  if (detach_res.code() != 0) {
    return error::Internal("Unable to detach perf event, error_message $0", detach_res.msg());
  }
  --num_attached_perf_events_;
  return Status::OK();
}

void BCCWrapper::DetachPerfEvents() {
  // TODO(kgandhi): PL-453  Figure out a fix for below warning.
  // WARNING: Detaching perf events based on event_type_ and event_config_ might
  // end up removing the perf event if there was another source with the same perf event and
  // config. Should be rare but may still be an issue.

  for (const PerfEventSpec& p : perf_events_) {
    auto res = DetachPerfEvent(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  perf_events_.clear();
}

void BCCWrapper::PollPerfBuffer(std::string_view perf_buffer_name, int timeout_ms) {
  auto perf_buffer = bpf().get_perf_buffer(std::string(perf_buffer_name));
  if (perf_buffer != nullptr) {
    perf_buffer->poll(timeout_ms);
  }
}

namespace {

void HandleLog(void* /*cb_cookie*/, void* data, int /*data_size*/) {
  static constexpr std::string_view kBPFLogEventsPreamble = "BPF ";
  const auto* event = static_cast<const log_event_t*>(data);
  LOG(INFO) << absl::StrCat(kBPFLogEventsPreamble,
                            std::string_view(event->msg, event->attr.msg_size));
}

static constexpr PerfBufferSpec kLogPerfBufferSpec = {"log_events", &HandleLog, nullptr};

}  // namespace

Status BCCWrapper::InitLogging() {
  PL_RETURN_IF_ERROR(OpenPerfBuffer(kLogPerfBufferSpec, nullptr));
  logging_enabled_ = true;
  return Status::OK();
}

void BCCWrapper::DumpBPFLog() {
  if (logging_enabled_) {
    PollPerfBuffer(kLogPerfBufferSpec.name);
  }
}

}  // namespace stirling
}  // namespace pl

#endif
