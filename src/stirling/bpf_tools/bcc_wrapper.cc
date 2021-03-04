#ifdef __linux__

#include "src/stirling/bpf_tools/bcc_wrapper.h"

#include <linux/perf_event.h>
#include <sys/mount.h>

#include <unistd.h>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>

#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/system.h"
#include "src/stirling/bpf_tools/task_struct_offset_resolver.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/utils/linux_headers.h"

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

DEFINE_bool(
    stirling_always_infer_task_struct_offsets, false,
    "When true, run the task_struct offset resolver even when local/host headers are found.");

namespace pl {
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
  PL_ASSIGN_OR_RETURN(bool is_empty, fs::IsEmpty(sys_kernel_debug));
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

StatusOr<utils::TaskStructOffsets> ResolveTaskStructOffsets() {
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

Status BCCWrapper::InitBPFProgram(std::string_view bpf_program, std::vector<std::string> cflags,
                                  bool requires_linux_headers) {
  using utils::TaskStructOffsets;

  if (!IsRoot()) {
    return error::PermissionDenied("BCC currently only supported as the root user.");
  }

  if (requires_linux_headers) {
    // This function will setup linux headers for BPF code deployment.
    // If another BCCWrapper has already run this function, it will just return the same location
    // as the previous one.
    // Note: Could also put this in Stirling Init() function, but then some tests which use
    //       BCCWrapper (e.g. connector_bpf_tests), would have to make sure to call this function.
    //       Thus, it is deemed to be better here.
    PL_ASSIGN_OR_RETURN(const std::filesystem::path sys_headers_dir,
                        utils::FindOrInstallLinuxHeaders({utils::kDefaultHeaderSearchOrder}));

    LOG(INFO) << absl::Substitute("Using linux headers found at $0 for BCC runtime.",
                                  sys_headers_dir.string());

    // When using packaged Linux headers, there is a good chance that the `struct task_struct`
    // is not a perfect match with the version on the host OS (despite our best efforts to account
    // for the kernel config). In such cases, try to resolve the location of the fields we care
    // about, and send them in as an override to the BPF code.
    // Note that if we found local host headers, then we do not typically do this step, because
    // we trust the locally installed headers to be a perfect match.
    // There is a flag to force the task struct fields resolution, in case we don't trust the
    // local headers, and for testing purposes.
    bool potentially_mismatched_headers = utils::g_packaged_headers_installed;
    if (potentially_mismatched_headers || FLAGS_stirling_always_infer_task_struct_offsets) {
      LOG(INFO) << "Resolving task_struct offsets.";

      PL_ASSIGN_OR_RETURN(TaskStructOffsets offsets, ResolveTaskStructOffsets());

      LOG(INFO) << absl::Substitute("Task struct offsets: group_leader=$0 real_start_time=$1",
                                    offsets.group_leader_offset, offsets.real_start_time_offset);

      cflags.push_back(
          absl::Substitute("-DGROUP_LEADER_OFFSET_OVERRIDE=$0", offsets.group_leader_offset));
      cflags.push_back(
          absl::Substitute("-DSTART_BOOTTIME_OFFSET_OVERRIDE=$0", offsets.real_start_time_offset));
    }
  }

  PL_RETURN_IF_ERROR(MountDebugFS());

  auto init_res = bpf_.init(std::string(bpf_program), cflags);
  if (init_res.code() != 0) {
    return error::Internal("Unable to initialize BCC BPF program: $0", init_res.msg());
  }
  return Status::OK();
}

Status BCCWrapper::AttachKProbe(const KProbeSpec& probe) {
  VLOG(1) << "Deploying kprobe: " << probe.ToString();
  DCHECK(probe.attach_type != BPFProbeAttachType::kReturnInsts);
  PL_RETURN_IF_ERROR(bpf_.attach_kprobe(
      bpf_.get_syscall_fnname(std::string(probe.kernel_fn)), std::string(probe.probe_fn),
      0 /* offset */, static_cast<bpf_probe_attach_type>(probe.attach_type), kKprobeMaxActive));
  kprobes_.push_back(probe);
  ++num_attached_kprobes_;
  return Status::OK();
}

Status BCCWrapper::AttachUProbe(const UProbeSpec& probe) {
  VLOG(1) << "Deploying uprobe: " << probe.ToString();
  // TODO(oazizi): Natively support this attach type in BCCWrapper.
  DCHECK(probe.attach_type != BPFProbeAttachType::kReturnInsts);
  DCHECK((probe.symbol.empty() && probe.address != 0) ||
         (!probe.symbol.empty() && probe.address == 0))
      << "Exactly one of 'symbol' and 'address' must be specified.";
  PL_RETURN_IF_ERROR(bpf_.attach_uprobe(
      probe.binary_path, probe.symbol, std::string(probe.probe_fn), probe.address,
      static_cast<bpf_probe_attach_type>(probe.attach_type), probe.pid));
  uprobes_.push_back(probe);
  ++num_attached_uprobes_;
  return Status::OK();
}

Status BCCWrapper::AttachSamplingProbe(const SamplingProbeSpec& probe) {
  constexpr uint64_t kNanosPerMilli = 1000 * 1000;
  const uint64_t sample_period = probe.period_millis * kNanosPerMilli;
  // A sampling probe is just a PerfEventProbe, where the perf event is a clock counter.
  // When a requisite number of clock samples occur, the kernel will trigger the BPF code.
  // By specifying a frequency, the kernel will attempt to adjust the threshold to achieve
  // the desired sampling frequency.
  PerfEventSpec perf_event_spec{.type = PERF_TYPE_SOFTWARE,
                                .config = PERF_COUNT_SW_CPU_CLOCK,
                                .probe_fn = probe.probe_fn,
                                .sample_period = sample_period,
                                .sample_freq = 0};

  return AttachPerfEvent(perf_event_spec);
}

Status BCCWrapper::AttachKProbes(const ArrayView<KProbeSpec>& probes) {
  for (const KProbeSpec& p : probes) {
    PL_RETURN_IF_ERROR(AttachKProbe(p));
  }
  return Status::OK();
}

Status BCCWrapper::AttachUProbes(const ArrayView<UProbeSpec>& probes) {
  for (const UProbeSpec& p : probes) {
    PL_RETURN_IF_ERROR(AttachUProbe(p));
  }
  return Status::OK();
}

Status BCCWrapper::AttachSamplingProbes(const ArrayView<SamplingProbeSpec>& probes) {
  for (const SamplingProbeSpec& p : probes) {
    PL_RETURN_IF_ERROR(AttachSamplingProbe(p));
  }
  return Status::OK();
}

// TODO(PL-1294): This can fail in rare cases. See the cited issue. Find the root cause.
Status BCCWrapper::DetachKProbe(const KProbeSpec& probe) {
  VLOG(1) << "Detaching kprobe: " << probe.ToString();
  PL_RETURN_IF_ERROR(bpf_.detach_kprobe(bpf_.get_syscall_fnname(std::string(probe.kernel_fn)),
                                        static_cast<bpf_probe_attach_type>(probe.attach_type)));
  --num_attached_kprobes_;
  return Status::OK();
}

Status BCCWrapper::DetachUProbe(const UProbeSpec& probe) {
  VLOG(1) << "Detaching uprobe " << probe.ToString();

  if (fs::Exists(probe.binary_path).ok()) {
    PL_RETURN_IF_ERROR(bpf_.detach_uprobe(probe.binary_path, probe.symbol, probe.address,
                                          static_cast<bpf_probe_attach_type>(probe.attach_type),
                                          probe.pid));
  }
  --num_attached_uprobes_;
  return Status::OK();
}

void BCCWrapper::DetachKProbes() {
  for (const auto& p : kprobes_) {
    auto res = DetachKProbe(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  kprobes_.clear();
}

void BCCWrapper::DetachUProbes() {
  for (const auto& p : uprobes_) {
    auto res = DetachUProbe(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  uprobes_.clear();
}

Status BCCWrapper::OpenPerfBuffer(const PerfBufferSpec& perf_buffer, void* cb_cookie) {
  VLOG(1) << "Opening perf buffer: " << perf_buffer.name;
  PL_RETURN_IF_ERROR(bpf_.open_perf_buffer(std::string(perf_buffer.name),
                                           perf_buffer.probe_output_fn, perf_buffer.probe_loss_fn,
                                           cb_cookie, FLAGS_stirling_bpf_perf_buffer_page_count));
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
  VLOG(1) << "Closing perf buffer: " << perf_buffer.name;
  PL_RETURN_IF_ERROR(bpf_.close_perf_buffer(std::string(perf_buffer.name)));
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
  VLOG(1) << absl::Substitute("Attaching perf event:\n   type=$0\n   probe_fn=$1",
                              magic_enum::enum_name(perf_event.type), perf_event.probe_fn);
  PL_RETURN_IF_ERROR(bpf_.attach_perf_event(perf_event.type, perf_event.config,
                                            std::string(perf_event.probe_fn),
                                            perf_event.sample_period, perf_event.sample_freq));
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
  VLOG(1) << absl::Substitute("Detaching perf event:\n   type=$0\n   probe_fn=$1",
                              magic_enum::enum_name(perf_event.type), perf_event.probe_fn);
  PL_RETURN_IF_ERROR(bpf_.detach_perf_event(perf_event.type, perf_event.config));
  --num_attached_perf_events_;
  return Status::OK();
}

void BCCWrapper::DetachPerfEvents() {
  // TODO(kgandhi): PL-453  Figure out a fix for below warning.
  // WARNING: Detaching perf events based on type and config might
  // end up removing the perf event if there was another source with the same perf event and
  // config. Should be rare but may still be an issue.

  for (const PerfEventSpec& p : perf_events_) {
    auto res = DetachPerfEvent(p);
    LOG_IF(ERROR, !res.ok()) << res.msg();
  }
  perf_events_.clear();
}

void BCCWrapper::PollPerfBuffer(std::string_view perf_buffer_name, int timeout_ms) {
  auto perf_buffer = bpf_.get_perf_buffer(std::string(perf_buffer_name));
  if (perf_buffer != nullptr) {
    perf_buffer->poll(timeout_ms);
  }
}

void BCCWrapper::PollPerfBuffers(int timeout_ms) {
  for (const auto& spec : perf_buffers_) {
    PollPerfBuffer(spec.name, timeout_ms);
  }
}

void BCCWrapper::Stop() {
  DetachPerfEvents();
  ClosePerfBuffers();
  DetachKProbes();
  DetachUProbes();
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
