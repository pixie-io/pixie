#pragma once

#include <string>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace utils {

struct TaskStructOffsets {
  uint64_t real_start_time_offset = 0;
  uint64_t group_leader_offset = 0;

  std::string ToString() {
    return absl::Substitute("{real_start_time=$0, group_leader=$1}", real_start_time_offset,
                            group_leader_offset);
  }

  bool operator==(const TaskStructOffsets& other) const {
    return real_start_time_offset == other.real_start_time_offset &&
           group_leader_offset == other.group_leader_offset;
  }
};

/**
 * Uses BPF probe to try to resolve the task struct offsets of certain fields.
 * This method does not require Linux headers.
 *
 * It determines the values by searching task_struct's memory for predictable values
 * that identify the location of the desired members.
 *
 * In particular, we look for the offset of two values:
 * 1) real_start_time/start_boottime (name changed in Linux 5.5)
 * 2) group_leader
 *
 * The reason we need these offsets is so we can access task->group_leader->real_start_time.
 *
 * We find real_start_time by cross-checking the time value against the value under /proc
 *
 * We find the group_leader by probing the main thread and looking for a pointer to self.
 * This works because the leader of a task with a single thread is itself.
 */
StatusOr<TaskStructOffsets> ResolveTaskStructOffsets();

/**
 * The core logic for ResolveTaskStructOffsets.
 * This is exposed for testing purposes only.
 */
StatusOr<TaskStructOffsets> ResolveTaskStructOffsetsCore();

}  // namespace utils
}  // namespace stirling
}  // namespace pl
