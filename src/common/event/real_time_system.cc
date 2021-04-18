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

#include "src/common/event/real_time_system.h"

#include <chrono>
#include <functional>
#include <memory>

#include "src/common/event/dispatcher.h"

namespace px {
namespace event {

namespace {

class ProxyScheduler : public Scheduler {
 public:
  explicit ProxyScheduler(Scheduler* base_scheduler) : base_scheduler_(base_scheduler) {}

  TimerUPtr CreateTimer(const TimerCB& cb, Dispatcher* dispatcher) override {
    return base_scheduler_->CreateTimer(cb, dispatcher);
  }

 private:
  Scheduler* base_scheduler_;
};

}  // namespace

/**
 * CreateScheduler returns a wrapper that delegates timer creation to the scheduler instance
 * that is passed in as the base_scheduler.
 * @param base_scheduler The scheduler instance to use.
 * @return Unique pointer to the scheduler.
 */
SchedulerUPtr RealTimeSystem::CreateScheduler(Scheduler* base_scheduler) {
  return std::make_unique<ProxyScheduler>(base_scheduler);
}

}  // namespace event
}  // namespace px
