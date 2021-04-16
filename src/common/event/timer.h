#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace px {
namespace event {
/**
 * Callback for timer after expiration.
 */
using TimerCB = std::function<void()>;

/**
 * Timer is an abstract timer event.
 */
class Timer {
 public:
  virtual ~Timer() = default;

  /**
   * Disable pending timeout without destroying the timer.
   */
  virtual void DisableTimer() = 0;

  /**
   * Enable a pending timeout. If a timeout is already pending, it will be reset to the new timeout.
   * @param ms supplies the duration of the alarm in milliseconds.
   */
  virtual void EnableTimer(const std::chrono::milliseconds& ms) = 0;

  /**
   * Return whether the timer is currently armed.
   */
  virtual bool Enabled() = 0;
};

using TimerUPtr = std::unique_ptr<Timer>;

}  // namespace event
}  // namespace px
