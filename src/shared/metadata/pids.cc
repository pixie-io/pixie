#include "src/shared/metadata/pids.h"

namespace px {
namespace md {

std::string PIDInfo::DebugString() const {
  return absl::Substitute("upid: $0, cmdline=$1, cid=$2", upid().String(), cmdline(), cid());
}

std::string PIDStartedEvent::DebugString() const {
  return absl::Substitute("PIDStartedEvent<$0>", pid_info.DebugString());
}

std::string PIDTerminatedEvent::DebugString() const {
  return absl::Substitute("PIDTerminatedEvent<upid=$0, stop_time_ns=$1>", upid.String(),
                          stop_time_ns);
}

std::ostream& operator<<(std::ostream& os, const PIDInfo& info) { return os << info.DebugString(); }

std::ostream& operator<<(std::ostream& os, const PIDStatusEvent& ev) {
  return os << ev.DebugString();
}

std::ostream& operator<<(std::ostream& os, const PIDStartedEvent& ev) {
  return os << ev.DebugString();
}

bool operator==(const PIDStartedEvent& lhs, const PIDStartedEvent& rhs) {
  return lhs.pid_info == rhs.pid_info;
}

bool operator==(const PIDTerminatedEvent& lhs, const PIDTerminatedEvent& rhs) {
  return lhs.upid == rhs.upid && lhs.stop_time_ns == rhs.stop_time_ns;
}
}  // namespace md
}  // namespace px
