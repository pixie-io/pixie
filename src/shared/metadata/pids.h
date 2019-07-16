#pragma once

#include <string>

#include "src/common/base/base.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace md {

/**
 * Store information about PIDs.
 */
struct PIDInfo {
  PIDInfo() = default;

  UPID upid;
  int64_t pid;

  /**
   * The command line used to start this PID.
   */
  std::string cmdline;

  /**
   * The container running this PID.
   */
  CID container_id;

  /**
   * Start time of this K8s object.
   */
  int64_t start_time_ns = 0;

  /**
   * The time that this container info was last updated.
   * If not updated we assume that the Container has died. We use this to know if
   * a given PID has been deleted.
   */
  int64_t last_update_time_ns = 0;

  void CloneFrom(const PIDInfo& other) { *this = other; }

 private:
  // Private copy constructor to make this class not implicitly copyable.
  PIDInfo(const PIDInfo&) = default;
  PIDInfo& operator=(const PIDInfo&) = default;
};

/*
 * Description of events used to transmit information about
 * PID creation and deletion.
 */

/**
 * The PID status event type.
 */
enum class PIDStatusEventType : uint8_t { kUnknown = 0, kStarted, kStopped };

/**
 * Base class for PID status event.
 */
struct PIDStatusEvent {
  PIDStatusEvent() = delete;
  explicit PIDStatusEvent(PIDStatusEventType type) : type(type) {}
  PIDStatusEventType type = PIDStatusEventType::kUnknown;
};

/**
 * PIDStartEvent has information about new PIDs.
 * It contains a copy of the newly created PID Info.
 */
struct PIDStartedEvent : public PIDStatusEvent {
  explicit PIDStartedEvent(const PIDInfo& other) : PIDStatusEvent(PIDStatusEventType::kStarted) {
    pid_info.CloneFrom(other);
  }

  PIDInfo pid_info;
};

/**
 * PIDStopEvent has information about deleted PIDs.
 * It only contains the unique PID that was deleted.
 */
struct PIDStoppedEvent : public PIDStatusEvent {
  explicit PIDStoppedEvent(UPID stopped_pid)
      : PIDStatusEvent(PIDStatusEventType::kStopped), upid(stopped_pid) {}
  UPID upid;
};

}  // namespace md
}  // namespace pl
