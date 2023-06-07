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

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <set>
#include <thread>

#include <absl/base/thread_annotations.h>
#include <absl/synchronization/mutex.h>
#include <gtest/gtest_prod.h>

#include "src/common/event/dispatcher.h"
#include "src/common/event/time_system.h"

namespace px {
namespace event {

class SimulatedTimer;

constexpr auto kDefaultSimulatedTime = std::chrono::seconds(1577865601);

class SimulatedTimeSystem final : public TimeSystem {
 public:
  SimulatedTimeSystem()
      : index_(0), monotonic_time_(kDefaultSimulatedTime), system_time_(kDefaultSimulatedTime) {}
  SimulatedTimeSystem(MonotonicTimePoint monotonic_time, SystemTimePoint system_time)
      : index_(0), monotonic_time_(monotonic_time), system_time_(system_time) {}
  ~SimulatedTimeSystem();

  /**
   * Public functions required for Timesystem.
   */
  SchedulerUPtr CreateScheduler(Scheduler* base_scheduler) override;
  SystemTimePoint SystemTime() const override;
  MonotonicTimePoint MonotonicTime() const override;

  /**
   * Public functions used for testing.
   */

  /**
   * Sleep for the given duration. This updates the monotonic and system time.
   */
  void Sleep(const Duration& duration);

 private:
  friend class SimulatedTimer;
  friend class SimulatedTimeSystemTest;

  FRIEND_TEST(SimulatedTimeSystemTest, Monotonic);
  FRIEND_TEST(SimulatedTimeSystemTest, System);
  FRIEND_TEST(SimulatedTimeSystemTest, SystemTimeOrdering);

  bool IsCorrectThread() {
    // Make sure that only one thread is advancing time during tests.
    if (run_tid_ == std::thread::id()) {  // No thread ID assigned yet.
      run_tid_ = std::this_thread::get_id();
      return true;
    }

    return run_tid_ == std::this_thread::get_id();
  }

  std::thread::id run_tid_;

  /**
   * Sets the time forward monotonically. If the supplied argument moves
   * backward in time, the call is a no-op. If the supplied argument moves
   * forward, any applicable timers are fired, and system-time is also moved
   * forward by the same delta.
   *
   * @param monotonic_time The desired new current time.
   */
  void SetMonotonicTime(const MonotonicTimePoint& monotonic_time) {
    {
      CHECK(IsCorrectThread());
      SetMonotonicTimeAndActivateTimers(monotonic_time);
    }
  }

  /**
   * Sets the system-time, whether forward or backward. If time moves forward,
   * applicable timers are fired and monotonic time is also increased by the
   * same delta.
   *
   * @param system_time The desired new system time.
   */
  void SetSystemTime(const SystemTimePoint& system_time);

  /**
   * Sets the time forward monotonically. If the supplied argument moves
   * backward in time, the call is a no-op. If the supplied argument moves
   * forward, any applicable timers are fired, and system-time is also moved
   * forward by the same delta.
   *
   * @param monotonic_time The desired new current time.
   */
  void SetMonotonicTimeAndActivateTimers(MonotonicTimePoint monotonic_time);

  struct CompareSimulatedTimer {
    bool operator()(const SimulatedTimer* a, const SimulatedTimer* b) const;
  };
  // This is a set of timers, ordered by activation time.
  using TimerSet = std::set<SimulatedTimer*, CompareSimulatedTimer>;
  TimerSet timers_;
  // Adds/removes a timer from the timer set.
  void AddTimerWithLock(SimulatedTimer*, const std::chrono::milliseconds& duration);
  void RemoveTimerWithLock(SimulatedTimer*);

  // The simulation keeps a unique ID for each timer to act as a deterministic
  // tie-breaker for timer-ordering.
  uint64_t index_ ABSL_GUARDED_BY(timers_lock_);
  int64_t NextIndex();

  absl::Mutex* GetTimeLock() { return &timers_lock_; }
  absl::Mutex timers_lock_;

  MonotonicTimePoint monotonic_time_;
  SystemTimePoint system_time_;
};

/**
 * A timer to be used with a SimulatedTimeSystem.
 */
class SimulatedTimer : public Timer {
 public:
  SimulatedTimer(SimulatedTimeSystem* time_system, Scheduler* base_scheduler, TimerCB cb,
                 Dispatcher* dispatcher)
      : index_(time_system->NextIndex()),
        base_timer_(base_scheduler->CreateTimer([this, cb] { runTimer(cb); }, dispatcher)),
        time_system_(time_system) {}
  ~SimulatedTimer();

  /**
   * Public functions required for timer.
   */
  void DisableTimer() override;
  void EnableTimer(const std::chrono::milliseconds& ms) override;
  bool Enabled() override;

 private:
  friend class SimulatedTimeSystem;

  uint64_t index() const { return index_; }
  MonotonicTimePoint time() const { return time_; }
  void SetTime(MonotonicTimePoint time) { time_ = time; }

  void runTimer(TimerCB cb) { cb(); }

  // The following functions should only be called if a lock on the timer's timesystem mutex is
  // held.
  void DisableTimerWithLock();
  void EnableTimerWithLock(const std::chrono::milliseconds& ms);
  void ActivateTimerWithLock();

  // The index of the timer, assigned by the time system.
  const uint64_t index_;
  // The time at which the timer should be triggered.
  MonotonicTimePoint time_;
  // Whether the timer is enabled.
  bool armed_ = false;

  TimerUPtr base_timer_;
  SimulatedTimeSystem* time_system_;
};

/**
 * An event scheduler to be used with a SimulatedTimeSystem.
 */
class SimulatedScheduler : public Scheduler {
 public:
  SimulatedScheduler(SimulatedTimeSystem* time_system, Scheduler* base_scheduler)
      : time_system_(time_system), base_scheduler_(base_scheduler) {}

  TimerUPtr CreateTimer(const TimerCB& cb, Dispatcher* dispatcher) override {
    return std::make_unique<SimulatedTimer>(time_system_, base_scheduler_, cb, dispatcher);
  }

 private:
  SimulatedTimeSystem* time_system_;
  Scheduler* base_scheduler_;
};

}  // namespace event
}  // namespace px
