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

#include <chrono>
#include <functional>
#include <memory>

#include "src/common/base/base.h"
#include "src/common/testing/event/simulated_time_system.h"

namespace px {
namespace event {

// SimulatedTimer
SimulatedTimer::~SimulatedTimer() {
  if (armed_) {
    DisableTimer();
  }
}

void SimulatedTimer::ActivateTimerWithLock() {
  armed_ = false;

  std::chrono::milliseconds duration = std::chrono::milliseconds::zero();
  base_timer_->EnableTimer(duration);
}

void SimulatedTimer::DisableTimer() {
  absl::MutexLock lock(time_system_->GetTimeLock());
  DisableTimerWithLock();
}

void SimulatedTimer::DisableTimerWithLock() {
  if (armed_) {
    time_system_->RemoveTimerWithLock(this);
    armed_ = false;
  }
}

void SimulatedTimer::EnableTimer(const std::chrono::milliseconds& duration) {
  absl::MutexLock lock(time_system_->GetTimeLock());
  EnableTimerWithLock(duration);
}

void SimulatedTimer::EnableTimerWithLock(const std::chrono::milliseconds& duration) {
  DisableTimerWithLock();
  armed_ = true;
  if (duration.count() == 0) {
    ActivateTimerWithLock();
  } else {
    time_system_->AddTimerWithLock(this, duration);
  }
}

bool SimulatedTimer::Enabled() {
  absl::MutexLock lock(time_system_->GetTimeLock());
  return armed_ || base_timer_->Enabled();
}

// SimulatedTimeSystem
SimulatedTimeSystem::~SimulatedTimeSystem() {
  absl::MutexLock lock(&timers_lock_);
  for (auto t : timers_) {
    t->DisableTimerWithLock();
  }
}

SystemTimePoint SimulatedTimeSystem::SystemTime() const { return system_time_; }

MonotonicTimePoint SimulatedTimeSystem::MonotonicTime() const { return monotonic_time_; }

void SimulatedTimeSystem::Sleep(const Duration& duration) {
  SystemTimePoint system_time =
      system_time_ + std::chrono::duration_cast<SystemTimePoint::duration>(duration);
  SetSystemTime(system_time);
}

void SimulatedTimeSystem::SetMonotonicTimeAndActivateTimers(MonotonicTimePoint monotonic_time) {
  CHECK(IsCorrectThread());

  absl::MutexLock lock(&timers_lock_);

  if (monotonic_time >= monotonic_time_) {
    // Timers is a std::set ordered by wakeup time, so pulling off begin() each
    // iteration gives you wakeup order. Also note that timers may be added
    // or removed during the call to activate() so it would not be correct to
    // range-iterate over the set.
    while (!timers_.empty()) {
      TimerSet::iterator pos = timers_.begin();
      SimulatedTimer* timer = *pos;
      MonotonicTimePoint timer_time = timer->time();
      if (timer_time > monotonic_time) {
        break;
      }
      system_time_ +=
          std::chrono::duration_cast<SystemTimePoint::duration>(timer_time - monotonic_time_);
      monotonic_time_ = timer_time;
      timers_.erase(pos);
      timer->ActivateTimerWithLock();
    }
    system_time_ +=
        std::chrono::duration_cast<SystemTimePoint::duration>(monotonic_time - monotonic_time_);
    monotonic_time_ = monotonic_time;
  }
}

void SimulatedTimeSystem::SetSystemTime(const SystemTimePoint& system_time) {
  CHECK(IsCorrectThread());

  if (system_time > system_time_) {
    MonotonicTimePoint monotonic_time =
        monotonic_time_ +
        std::chrono::duration_cast<MonotonicTimePoint::duration>(system_time - system_time_);
    SetMonotonicTimeAndActivateTimers(monotonic_time);
  } else {
    system_time_ = system_time;
  }
}

SchedulerUPtr SimulatedTimeSystem::CreateScheduler(Scheduler* base_scheduler) {
  return std::make_unique<SimulatedScheduler>(this, base_scheduler);
}

int64_t SimulatedTimeSystem::NextIndex() {
  absl::MutexLock lock(&timers_lock_);
  return index_++;
}

void SimulatedTimeSystem::AddTimerWithLock(SimulatedTimer* timer,
                                           const std::chrono::milliseconds& duration) {
  timer->SetTime(monotonic_time_ + duration);
  timers_.insert(timer);
}

void SimulatedTimeSystem::RemoveTimerWithLock(SimulatedTimer* timer) { timers_.erase(timer); }

// Compare two alarms, based on wakeup time and insertion order. Returns true if
// a comes before b.
bool SimulatedTimeSystem::CompareSimulatedTimer::operator()(const SimulatedTimer* a,
                                                            const SimulatedTimer* b) const {
  if (a != b) {
    if (a->time() < b->time()) {
      return true;
    } else if (a->time() == b->time() && a->index() < b->index()) {
      return true;
    }
  }
  return false;
}

}  // namespace event
}  // namespace px
