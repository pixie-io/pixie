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

#include "src/stirling/bpf_tools/task_struct_resolver.h"

#include <linux/sched.h>
#include <poll.h>
#include <sys/wait.h>

#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/system/proc_parser.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/obj_tools/address_converter.h"
#include "src/stirling/utils/proc_path_tools.h"

#include "src/stirling/bpf_tools/bcc_bpf_intf/types.h"

// Creates a string_view to the BPF code loaded into memory.
OBJ_STRVIEW(bcc_script, task_struct_mem_read);

// A function which we will uprobe on, to trigger our BPF code.
// The function itself is irrelevant, but it must not be optimized away.
// We declare this with C linkage (extern "C") so it has a simple symbol name.
extern "C" {
NO_OPT_ATTR void StirlingProbeTrigger() { return; }
}

namespace px {
namespace stirling {
namespace utils {

using ::px::stirling::bpf_tools::BPFProbeAttachType;
using ::px::stirling::bpf_tools::UProbeSpec;
using ::px::stirling::bpf_tools::WrappedBCCArrayTable;

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

#if AARCH64
  // On aarch64, the cpu_context struct inside the thread_struct, can contain the same address as
  // the group_leader value of the task_struct. Since the thread_struct is always at the end of the
  // task_struct, we can just stop searching when we get to the thread_struct. To identify the
  // offset of the thread_struct, we search for the address of thread local storage which is always
  // stored `sizeof(cpu_context) + some padding` after the beginning of the thread struct.
  // See:
  // https://github.com/torvalds/linux/blob/3ac88fa4605ec98e545fb3ad0154f575fda2de5f/arch/arm64/include/asm/processor.h#L147
  // for the thread_struct definition.
  uint64_t tls_addr;
  // ARM magic to get the system register TPIDR_EL0, which stores the address of thread local
  // storage.
  asm volatile("mrs %0, TPIDR_EL0" : "=r"(tls_addr));
  VLOG(1) << "TLS address = " << tls_addr;

  // We leave some room for padding. The thread_struct almost always seems to have some padding
  // between cpu_context and tp_value. In the case that there is no padding, it shouldn't affect
  // us since the values we care about are never right before the thread_struct in the
  // task_struct.
  constexpr uint64_t padding = 8;
  // aarch64's cpu_context struct in linux stores 13 registers. I checked this in v4.14 and v6.1
  // and it hasn't changed.
  constexpr uint64_t num_cpu_context_registers = 13;
  constexpr uint64_t tp_value_offset_in_thread_struct =
      padding + num_cpu_context_registers * sizeof(uint64_t);
  // For now we iterate through the buffer twice. At some point we should refactor offsets_manager
  // to eliminate the need to iterate through the buffer twice.
  int stop_at_offset = -1;
  for (uint64_t idx = tp_value_offset_in_thread_struct / sizeof(uint64_t);
       idx < sizeof(buf.u64words) / sizeof(uint64_t); idx++) {
    int current_offset = idx * sizeof(uint64_t);
    if (buf.u64words[idx] == tls_addr) {
      VLOG(1) << absl::Substitute("[offset = $0] Found tp_value", current_offset);
      stop_at_offset = current_offset - tp_value_offset_in_thread_struct;
      VLOG(1) << "Ignoring thread_struct at offset = " << stop_at_offset;
      break;
    }
  }
  if (stop_at_offset == -1) {
    return error::Internal(
        "Failed to find the thread_struct offset within the task_struct. This is required for "
        "resolving task struct offsets on aarch64");
  }
#endif

  for (const auto& [idx, val] : Enumerate(buf.u64words)) {
    int current_offset = idx * sizeof(uint64_t);

#if AARCH64
    if (current_offset == stop_at_offset) {
      break;
    }
#endif

    if (pl_nsec_to_clock_t(val) == proc_pid_start_time) {
      VLOG(1) << absl::Substitute("[offset = $0] Found real_start_time", current_offset);
      PX_RETURN_IF_ERROR(offsets_manager.SetRealStartTimeOffset(current_offset));
    }

    if (val == task_struct_addr) {
      VLOG(1) << absl::Substitute("[offset = $0] Found group_leader.", current_offset);
      PX_RETURN_IF_ERROR(offsets_manager.SetGroupLeaderOffset(current_offset));
    }
  }

  PX_RETURN_IF_ERROR(offsets_manager.CheckPopulated());

  return task_struct_offsets;
}

}  // namespace

StatusOr<TaskStructOffsets> ResolveTaskStructOffsetsCore() {
  // Get the PID start time from /proc.
  PX_ASSIGN_OR_RETURN(uint64_t proc_pid_start_time,
                      ::px::system::GetPIDStartTimeTicks("/proc/self"));

  PX_ASSIGN_OR_RETURN(std::filesystem::path self_path, GetSelfPath());
  PX_ASSIGN_OR_RETURN(auto elf_reader, obj_tools::ElfReader::Create(self_path.string()));
  const int64_t pid = getpid();
  PX_ASSIGN_OR_RETURN(auto converter,
                      obj_tools::ElfAddressConverter::Create(elf_reader.get(), pid));

  // Use address instead of symbol to specify this probe,
  // so that even if debug symbols are stripped, the uprobe can still attach.
  uint64_t symbol_addr =
      converter->VirtualAddrToBinaryAddr(reinterpret_cast<uint64_t>(&StirlingProbeTrigger));

  UProbeSpec uprobe{.binary_path = self_path,
                    .symbol = {},  // Keep GCC happy.
                    .address = symbol_addr,
                    .attach_type = BPFProbeAttachType::kEntry,
                    .probe_fn = "task_struct_probe"};

  // Deploy the BPF program.
  auto bcc = std::make_unique<px::stirling::bpf_tools::BCCWrapper>();
  std::vector<std::string> cflags;
  // Important! Must tell BCCWrapper that we don't need linux headers, otherwise we may
  // enter an infinite loop if BCCWrapper tries to run the TaskStructResolver again.
  bool requires_linux_headers = false;
  PX_RETURN_IF_ERROR(bcc->InitBPFProgram(bcc_script, cflags, requires_linux_headers));
  PX_RETURN_IF_ERROR(bcc->AttachUProbe(uprobe));

  // Trigger our uprobe.
  StirlingProbeTrigger();

  // Retrieve the task struct address from BPF map.
  auto task_struct_addrs =
      WrappedBCCArrayTable<uint64_t>::Create(bcc.get(), "task_struct_address_map");
  PX_ASSIGN_OR_RETURN(const uint64_t task_struct_addr, task_struct_addrs->GetValue(0));

  // Retrieve the raw memory buffer of the task struct.
  auto task_struct_bufs = WrappedBCCArrayTable<struct buf>::Create(bcc.get(), "task_struct_buf");
  PX_ASSIGN_OR_RETURN(const struct buf buf, task_struct_bufs->GetValue(0));

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
  if (retval <= 0) {
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

StatusOr<TaskStructOffsets> ResolveTaskStructStartTimeOffsets() {
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
    PX_RETURN_IF_ERROR(ReadFromChild(fd[0], &result));

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

namespace {

// Returns the offset of task_struct::offset.
StatusOr<uint64_t> ResolveTaskStructExitCodeOffset() {
  // 171 is 0b10101011, which is somewhat a rare pattern.
  constexpr uint8_t exit_code = 171;

  SubProcess proc;
  PX_RETURN_IF_ERROR(proc.Start([]() -> int { return exit_code; }, {.stop_before_exec = true}));

  auto bcc = std::make_unique<px::stirling::bpf_tools::BCCWrapper>();
  PX_RETURN_IF_ERROR(
      bcc->InitBPFProgram(bcc_script, /*cflags*/ {}, /*requires_linux_headers*/ false));
  PX_RETURN_IF_ERROR(bcc->AttachTracepoint({std::string("sched:sched_process_exit"),
                                            std::string("tracepoint__sched__sched_process_exit")}));

  const std::string kProcExitTargetPIDTableName = "proc_exit_target_pid";
  auto proc_exit_target_pid_table =
      WrappedBCCArrayTable<uint32_t>::Create(bcc.get(), kProcExitTargetPIDTableName);

  // Set target PID in BPF map, which only report event triggered by the launched subprocess.
  PX_RETURN_IF_ERROR(proc_exit_target_pid_table->SetValue(0, proc.child_pid()));

  // Resume the child process, allow it to exit and trigger the tracepoint probe.
  proc.Signal(SIGCONT);

  // Wait until the child process exits. The probe on sched:sched_process_exit will be executed
  // fully before the process actually exits. This ensures the data is written to the BPF
  // map after this waiting.
  int status = proc.Wait();

  if (!WIFEXITED(status)) {
    return error::Internal("Child process exited abnormally");
  }

  int actual_exit_code WEXITSTATUS(status);
  if (actual_exit_code != exit_code) {
    return error::Internal(
        "Child process exit code differs form expectation, actual=$0 expected=$1", actual_exit_code,
        exit_code);
  }

  // Retrieve the raw memory buffer of the task struct.
  auto task_struct_bufs = WrappedBCCArrayTable<struct buf>::Create(bcc.get(), "task_struct_buf");
  PX_ASSIGN_OR_RETURN(const struct buf buf, task_struct_bufs->GetValue(0));

  for (const auto& [idx, val] : Enumerate(buf.u32words)) {
    int current_offset = idx * sizeof(uint32_t);
    // Exit code is assigned to the 2nd byte according to
    // https://unix.stackexchange.com/questions/99112/default-exit-code-when-process-is-terminated
    uint32_t expected_task_struct_exit_code = exit_code << 8;
    if (val == expected_task_struct_exit_code) {
      return current_offset;
    }
  }
  return error::Internal("Could not find the requested exit code in the task_struct buffer");
}

}  // namespace

StatusOr<TaskStructOffsets> ResolveTaskStructOffsets() {
  PX_ASSIGN_OR_RETURN(TaskStructOffsets res, ResolveTaskStructStartTimeOffsets());
  PX_ASSIGN_OR_RETURN(uint64_t exit_code_offset, ResolveTaskStructExitCodeOffset());
  res.exit_code_offset = exit_code_offset;
  return res;
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
