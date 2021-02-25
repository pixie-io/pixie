#pragma once

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace utils {

struct TaskStructOffsets {
  uint64_t real_start_time = 0;
  uint64_t group_leader = 0;
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

}  // namespace utils
}  // namespace stirling
}  // namespace pl
