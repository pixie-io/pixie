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

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "src/common/event/api_impl.h"
#include "src/common/event/libuv.h"
#include "src/common/testing/event/simulated_time_system.h"

namespace px {
namespace event {
class SimulatedTimeSystemTest : public testing::Test {
 protected:
  void TearDown() override { dispatcher_->Exit(); }

  SimulatedTimeSystemTest() {
    start_monotonic_time_ = std::chrono::steady_clock::now();
    start_system_time_ = std::chrono::system_clock::now();
    time_system_ = std::make_unique<SimulatedTimeSystem>(start_monotonic_time_, start_system_time_);
    api_ = std::make_unique<APIImpl>(time_system_.get());
    dispatcher_ = std::make_unique<LibuvDispatcher>("test", *api_.get(), time_system_.get());
  }

  void sleepMSAndLoop(int64_t delay_ms) {
    time_system_->Sleep(std::chrono::milliseconds(delay_ms));
    dispatcher_->Run(Dispatcher::RunType::NonBlock);
  }

  void AdvanceSystemMsAndLoop(int64_t delay_ms) {
    time_system_->SetSystemTime(time_system_->SystemTime() + std::chrono::milliseconds(delay_ms));
    dispatcher_->Run(Dispatcher::RunType::NonBlock);
  }

  void AddTask(int64_t delay_ms, char marker) {
    std::chrono::milliseconds delay(delay_ms);
    TimerUPtr timer = dispatcher_->CreateTimer([this, marker, delay]() {
      PX_UNUSED(delay);
      output_.push_back(marker);
    });
    timer->EnableTimer(delay);
    timers_.push_back(std::move(timer));
  }

  std::unique_ptr<LibuvDispatcher> dispatcher_;
  std::vector<TimerUPtr> timers_;
  std::unique_ptr<SimulatedTimeSystem> time_system_;
  MonotonicTimePoint start_monotonic_time_;
  SystemTimePoint start_system_time_;
  std::vector<char> output_;
  std::unique_ptr<APIImpl> api_;
};

TEST_F(SimulatedTimeSystemTest, Monotonic) {
  EXPECT_EQ(start_monotonic_time_, time_system_->MonotonicTime());

  // Setting time forward works.
  time_system_->SetMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(5));
  EXPECT_EQ(start_monotonic_time_ + std::chrono::milliseconds(5), time_system_->MonotonicTime());

  // But going backward does not.
  time_system_->SetMonotonicTime(start_monotonic_time_ + std::chrono::milliseconds(3));
  EXPECT_EQ(start_monotonic_time_ + std::chrono::milliseconds(5), time_system_->MonotonicTime());
}

TEST_F(SimulatedTimeSystemTest, System) {
  EXPECT_EQ(start_system_time_, time_system_->SystemTime());

  // Setting time forward works.
  time_system_->SetSystemTime(start_system_time_ + std::chrono::milliseconds(5));
  EXPECT_EQ(start_system_time_ + std::chrono::milliseconds(5), time_system_->SystemTime());

  // And going backward works too.
  time_system_->SetSystemTime(start_system_time_ + std::chrono::milliseconds(3));
  EXPECT_EQ(start_system_time_ + std::chrono::milliseconds(3), time_system_->SystemTime());
}

TEST_F(SimulatedTimeSystemTest, Sleep) {
  sleepMSAndLoop(5);
  EXPECT_EQ(start_monotonic_time_ + std::chrono::milliseconds(5), time_system_->MonotonicTime());
  EXPECT_EQ(start_system_time_ + std::chrono::milliseconds(5), time_system_->SystemTime());
}

TEST_F(SimulatedTimeSystemTest, Ordering) {
  AddTask(5, '5');
  AddTask(3, '3');
  AddTask(6, '6');
  EXPECT_EQ(0, output_.size());
  sleepMSAndLoop(2);
  EXPECT_EQ(0, output_.size());
  sleepMSAndLoop(3);
  EXPECT_EQ(2, output_.size());
  EXPECT_EQ('3', output_.at(0));
  EXPECT_EQ('5', output_.at(1));
  sleepMSAndLoop(1);
  EXPECT_EQ(3, output_.size());
  EXPECT_EQ('6', output_.at(2));

  EXPECT_EQ(start_monotonic_time_ + std::chrono::milliseconds(6), time_system_->MonotonicTime());
  EXPECT_EQ(start_system_time_ + std::chrono::milliseconds(6), time_system_->SystemTime());
}

TEST_F(SimulatedTimeSystemTest, SystemTimeOrdering) {
  AddTask(5, '5');
  AddTask(3, '3');
  AddTask(6, '6');
  EXPECT_EQ(0, output_.size());
  AdvanceSystemMsAndLoop(2);
  EXPECT_EQ(0, output_.size());
  AdvanceSystemMsAndLoop(3);
  EXPECT_EQ(2, output_.size());
  EXPECT_EQ('3', output_.at(0));
  EXPECT_EQ('5', output_.at(1));
  AdvanceSystemMsAndLoop(1);
  EXPECT_EQ(3, output_.size());
  EXPECT_EQ('6', output_.at(2));
  time_system_->SetSystemTime(start_system_time_ + std::chrono::milliseconds(1));
  time_system_->SetSystemTime(start_system_time_ + std::chrono::milliseconds(100));
  EXPECT_EQ(3, output_.size());
  EXPECT_EQ('6', output_.at(2));  // callbacks don't get replayed.
}

TEST_F(SimulatedTimeSystemTest, DisableTimer) {
  AddTask(5, '5');
  AddTask(3, '3');
  AddTask(6, '6');
  timers_[0]->DisableTimer();
  EXPECT_EQ(0, output_.size());
  sleepMSAndLoop(5);
  EXPECT_EQ(1, output_.size());
  EXPECT_EQ('3', output_.at(0));
  sleepMSAndLoop(1);
  EXPECT_EQ(2, output_.size());
  EXPECT_EQ('3', output_.at(0));
  EXPECT_EQ('6', output_.at(1));
}

TEST_F(SimulatedTimeSystemTest, IgnoreRedundantDisable) {
  AddTask(5, '5');
  timers_[0]->DisableTimer();
  timers_[0]->DisableTimer();
  sleepMSAndLoop(5);
  EXPECT_EQ(0, output_.size());
}

TEST_F(SimulatedTimeSystemTest, OverrideEnable) {
  AddTask(5, '5');
  timers_[0]->EnableTimer(std::chrono::milliseconds(6));
  sleepMSAndLoop(5);
  EXPECT_EQ(0, output_.size());
  sleepMSAndLoop(1);
  EXPECT_EQ(1, output_.size());
  EXPECT_EQ('5', output_.at(0));
}

}  // namespace event
}  // namespace px
