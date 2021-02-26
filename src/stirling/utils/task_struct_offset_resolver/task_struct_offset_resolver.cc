#include "src/stirling/utils/task_struct_offset_resolver/task_struct_offset_resolver.h"

#include <poll.h>

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

// Define NO_OPT_ATTR that specifies that function should not be optimized away.
// Used on StirlingProbeTrigger below.
// Note that the attributes are different depending on the compiler.
#if defined(__clang__)
#define NO_OPT_ATTR __attribute__((noinline, optnone))
#elif defined(__GNUC__) || defined(__GNUG__)
#define NO_OPT_ATTR __attribute__((noinline, optimize("O0")))
#endif

// A function which we will uprobe on, to trigger our BPF code.
// The function itself is irrelevant.
// However, it must not be optimized away.
// We also leave this outside of any namespaces so it has a simple symbol name.
extern "C" {
NO_OPT_ATTR int StirlingProbeTrigger(int x) { return x; }
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

// A helper class for populating the TaskStructOffsets struct.
// Maintains some state to detect invalid/ambiguous cases.
class TaskStructOffsetsManager {
 public:
  explicit TaskStructOffsetsManager(TaskStructOffsets* offsets) : offsets_(offsets) {}

  Status SetRealStartTimeOffset(uint64_t offset) {
    if (offsets_->real_start_time_offset != 0) {
      const uint64_t prev_word_offset = offset - sizeof(uint64_t);
      // Check if we had one previous match, and that match was the previous word,
      // if so, this second match is the one we're looking for. Why?
      // Linux task struct has two time fields which appear back-to-back:
      //   u64 start_time; // Monotonic time in nsecs
      //   u64 start_boottime; // Boot based time in nsecs
      // If the machine has never been suspended, then these two times will not match.
      // In such cases, we may see a duplicate, and we want to take the second value.
      if (num_start_time_matches_ == 1 && offsets_->real_start_time_offset == prev_word_offset) {
        // This is the expected duplicate. Nothing to do here.
      } else {
        return error::Internal(
            "Location of real_start_time is ambiguous. Found multiple possible offsets. "
            "[previous=$0 current=$1]",
            offsets_->real_start_time_offset, offset);
      }
    }
    offsets_->real_start_time_offset = offset;
    ++num_start_time_matches_;
    return Status::OK();
  }

  Status SetGroupLeaderOffset(uint64_t offset) {
    if (offsets_->group_leader_offset != 0) {
      return error::Internal(
          "Location of group_leader is ambiguous. Found multiple possible offsets. "
          "[previous=$0 current=$1]",
          offsets_->group_leader_offset, offset);
    }
    offsets_->group_leader_offset = offset;
    return Status::OK();
  }

  Status CheckPopulated() {
    if (offsets_->real_start_time_offset == 0) {
      return error::Internal("Could not find offset for real_start_time/start_boottime.");
    }

    if (offsets_->group_leader_offset == 0) {
      return error::Internal("Could not find offset for group_leader.");
    }

    return Status::OK();
  }

 private:
  TaskStructOffsets* offsets_;
  int num_start_time_matches_ = 0;
};

// Analyze the raw buffer for the proc pid start time and the task struct address.
//  - proc_pid_start_time is used to look for the real_start_time/start_boottime member.
//    Note that the name of the member changed across linux versions.
//  - task_struct_addr is used to look for a pointer to self, indicating the group_leader member.
//    This works since we are tracing a single-threaded program, so the main thread's leader is
//    itself.
StatusOr<TaskStructOffsets> ScanBufferForFields(const struct buf& buf,
                                                const uint64_t proc_pid_start_time,
                                                const uint64_t task_struct_addr) {
  VLOG(1) << absl::Substitute("task_struct_address = $0", task_struct_addr);
  VLOG(1) << absl::Substitute("/proc/self/stat:start_time = $0", proc_pid_start_time);

  TaskStructOffsets task_struct_offsets;

  TaskStructOffsetsManager offsets_manager(&task_struct_offsets);

  for (const auto& [idx, val] : Enumerate(buf.u64words)) {
    int current_offset = idx * sizeof(uint64_t);

    if (pl_nsec_to_clock_t(val) == proc_pid_start_time) {
      VLOG(1) << absl::Substitute("[offset = $0] Found real_start_time", current_offset);
      PL_RETURN_IF_ERROR(offsets_manager.SetRealStartTimeOffset(current_offset));
    }

    if (val == task_struct_addr) {
      VLOG(1) << absl::Substitute("[offset = $0] Found group_leader.", current_offset);
      PL_RETURN_IF_ERROR(offsets_manager.SetGroupLeaderOffset(current_offset));
    }
  }

  PL_RETURN_IF_ERROR(offsets_manager.CheckPopulated());

  return task_struct_offsets;
}

}  // namespace

StatusOr<TaskStructOffsets> ResolveTaskStructOffsetsCore() {
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

  // Use address instead of symbol to specify this probe,
  // so that even if debug symbols are stripped, the uprobe can still attach.
  uint64_t symbol_addr = reinterpret_cast<uint64_t>(&StirlingProbeTrigger);

  UProbeSpec uprobe{.binary_path = self_path,
                    .symbol = {},  // Keep GCC happy.
                    .address = symbol_addr,
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
  return ScanBufferForFields(buf, proc_pid_start_time, task_struct_addr);
}

namespace {
Status ReadFromChild(int fd, TaskStructOffsets* result) {
  // We don't expect to fail to receive data from the child,
  // but use poll to make sure we don't block indefinitely.
  struct pollfd fds;
  fds.fd = fd;
  fds.events = POLLIN;

  constexpr int kTimeoutMillis = 5000;
  int retval = poll(&fds, 1, kTimeoutMillis);
  if (retval == -1) {
    return error::Internal("Failed to receive data from child.");
  }

  retval = read(fd, result, sizeof(*result));
  if (retval != sizeof(*result)) {
    return error::Internal("Failed to receive data from child.");
  }

  return Status::OK();
}

Status WriteToParent(int fd, const TaskStructOffsets& result) {
  ssize_t bytes_written = write(fd, &result, sizeof(result));

  // We don't expect this to happen on this pipe, but check just in case.
  if (bytes_written != sizeof(result)) {
    return error::Internal("Failed to write data to parent.");
  }

  return Status::OK();
}
}  // namespace

StatusOr<TaskStructOffsets> ResolveTaskStructOffsets() {
  const TaskStructOffsets kSentinelValue;

  // Create pipe descriptors.
  int fd[2];
  int retval = pipe(fd);
  if (retval == -1) {
    return error::Internal("Resolution failed. Unable to create pipe: $0", std::strerror(errno));
  }

  pid_t child_pid = fork();
  if (child_pid != 0) {
    // Parent process: Wait for results from child.

    // Blocking read data from child.
    TaskStructOffsets result;

    PL_RETURN_IF_ERROR(ReadFromChild(fd[0], &result));

    // We can't transfer StatusOr through the pipe,
    // so we have to check manually.
    if (result == kSentinelValue) {
      return error::Internal(
          "Resolution failed in subprocess. Check subprocess logs for the error.");
    }

    close(fd[0]);
    close(fd[1]);

    return result;
  } else {
    // Child process: Run ResolveTaskStructOffsets(),
    // and send the result to the parent through pipes.

    // On error, we send kSentinelValue.
    StatusOr<TaskStructOffsets> result_status = ResolveTaskStructOffsetsCore();

    LOG_IF(ERROR, !result_status.ok()) << result_status.ToString();

    TaskStructOffsets result = result_status.ValueOr(kSentinelValue);

    // Send the value on the write-descriptor.
    Status s = WriteToParent(fd[1], result);
    LOG_IF(ERROR, !s.ok()) << s.ToString();

    // Close FDs.
    close(fd[0]);
    close(fd[1]);

    exit(0);
  }
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
