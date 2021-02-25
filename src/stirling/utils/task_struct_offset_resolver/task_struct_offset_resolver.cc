#include "src/stirling/utils/task_struct_offset_resolver/task_struct_offset_resolver.h"

#include <memory>
#include <string>

#include "src/common/base/base.h"
#include "src/common/system/proc_parser.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/utils/proc_path_tools.h"

#include "src/stirling/utils/task_struct_offset_resolver/bcc_bpf_intf/types.h"

// Creates a string_view to the BPF code loaded into memory.
BPF_SRC_STRVIEW(bcc_script, task_struct_mem_read);

// A function which we will uprobe on, to trigger our BPF code.
// The function itself is irrelevant.
// However, it must not be optimized away.
// We also leave this outside of any namespaces so it has a simple symbol name.
extern "C" {
__attribute__((noinline, optnone)) int StirlingProbeTrigger(int x) { return x; }
}

namespace pl {
namespace stirling {
namespace utils {

using ::pl::stirling::bpf_tools::BPFProbeAttachType;
using ::pl::stirling::bpf_tools::UProbeSpec;

namespace {

// This is how Linux converts nanoseconds to clock ticks.
// Used to report PID start times in clock ticks, just like /proc/<pid>/stat does.
uint64_t pl_nsec_to_clock_t(uint64_t x) {
  constexpr uint64_t NSEC_PER_SEC = 1000000000L;
  constexpr uint64_t USER_HZ = 100;
  return x / (NSEC_PER_SEC / USER_HZ);
}

// Analyze the raw buffer for the proc pid start time and the task struct address.
//  - proc_pid_start_time is used to look for the real_start_time/start_boottime member.
//    Note that the name of the member changed across linux versions.
//  - task_struct_addr is used to look for a pointer to self, indicating the group_leader member.
//    This works since we are tracing a single-threaded program, so the main thread's leader is
//    itself.
StatusOr<TaskStructOffsets> Analyze(const struct buf& buf, const uint64_t proc_pid_start_time,
                                    const uint64_t task_struct_addr) {
  VLOG(1) << absl::Substitute("task_struct_address = $0", task_struct_addr);
  VLOG(1) << absl::Substitute("/proc/self/stat:start_time = $0", proc_pid_start_time);

  TaskStructOffsets task_struct_offsets;

  for (const auto& [idx, val] : Enumerate(buf.u64words)) {
    int current_offset = idx * sizeof(uint64_t);
    if (pl_nsec_to_clock_t(val) == proc_pid_start_time) {
      VLOG(1) << absl::Substitute("[offset = $0] Found real_start_time", current_offset);
      if (task_struct_offsets.real_start_time != 0) {
        return error::Internal(
            "Location of real_start_time is ambiguous. Found multiple possible offsets. "
            "[previous=$0 current=$1]",
            task_struct_offsets.real_start_time, current_offset);
      }
      task_struct_offsets.real_start_time = current_offset;
    }
    if (val == task_struct_addr) {
      VLOG(1) << absl::Substitute("[offset = $0] Found group_leader.", current_offset);
      if (task_struct_offsets.group_leader != 0) {
        return error::Internal(
            "Location of group_leader is ambiguous. Found multiple possible offsets. [previous=$0 "
            "current=$1]",
            task_struct_offsets.group_leader, current_offset);
      }
      task_struct_offsets.group_leader = current_offset;
    }
  }

  if (task_struct_offsets.real_start_time == 0) {
    return error::Internal("Could not find offset for real_start_time/start_boottime.");
  }

  if (task_struct_offsets.group_leader == 0) {
    return error::Internal("Could not find offset for group_leader.");
  }

  return task_struct_offsets;
}

}  // namespace

StatusOr<TaskStructOffsets> ResolveTaskStructOffsets() {
  // Get the PID's start time from /proc.
  // TODO(oazizi): This only works if run as the main thread of a process.
  //               Fix to support threaded environments.
  PL_ASSIGN_OR_RETURN(uint64_t proc_pid_start_time,
                      ::pl::system::GetPIDStartTimeTicks("/proc/self"));

  auto bcc = std::make_unique<pl::stirling::bpf_tools::BCCWrapper>();

  const system::Config& sysconfig = system::Config::GetInstance();
  ::pl::system::ProcParser proc_parser(sysconfig);
  std::filesystem::path self_path = proc_parser.GetExePath(getpid());
  PL_ASSIGN_OR_RETURN(std::unique_ptr<FilePathResolver> fp_resolver,
                      FilePathResolver::Create(getpid()));
  PL_ASSIGN_OR_RETURN(self_path, fp_resolver->ResolvePath(self_path));
  self_path = sysconfig.ToHostPath(self_path);

  UProbeSpec uprobe{.binary_path = self_path,
                    .symbol = "StirlingProbeTrigger",
                    .attach_type = BPFProbeAttachType::kEntry,
                    .probe_fn = "task_struct_probe"};

  // Deploy the BPF program.
  PL_RETURN_IF_ERROR(bcc->InitBPFProgram(bcc_script));
  PL_RETURN_IF_ERROR(bcc->AttachUProbe(uprobe));

  // Trigger our uprobe.
  PL_UNUSED(StirlingProbeTrigger(1));

  // Retrieve the task struct address from BPF map.
  uint64_t task_struct_addr;
  {
    // TODO(oazizi): Find systematic way to convert ebpf::StatusTuple to pl::Status.
    ebpf::StatusTuple bpf_status = bcc->bpf()
                                       .get_array_table<uint64_t>("task_struct_address_map")
                                       .get_value(0, task_struct_addr);
    if (bpf_status.code() != 0) {
      return error::Internal("Failed to read task_struct_address_map");
    }
  }

  // Retrieve the raw memory buffer of the task struct.
  struct buf buf;
  {
    // TODO(oazizi): Find systematic way to convert ebpf::StatusTuple to pl::Status.
    ebpf::StatusTuple bpf_status =
        bcc->bpf().get_array_table<struct buf>("task_struct_buf").get_value(0, buf);
    if (bpf_status.code() != 0) {
      return error::Internal("Failed to read task_struct_buf");
    }
  }

  // Analyze the raw data buffer for the patterns we are looking for.
  return Analyze(buf, proc_pid_start_time, task_struct_addr);
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
