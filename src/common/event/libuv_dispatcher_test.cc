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
#include <thread>
#include "src/common/base/base.h"
#include "src/common/event/api.h"
#include "src/common/event/event.h"
#include "src/common/event/nats.h"

using px::event::APIImpl;
using px::event::RealTimeSystem;

namespace px {
namespace event {

int g_compute_pi_destructor_count = 0;

namespace {

class ComputePi : public px::event::AsyncTask {
 public:
  ~ComputePi() { ++g_compute_pi_destructor_count; }

  void Work() override {
    ++work_call_count_;
    VLOG(1) << "PI_ID : " << std::this_thread::get_id();
    int n = 10000;
    double sum = 0.0;
    int sign = 1;
    for (int i = 0; i < n; ++i) {
      sum += sign / (2.0 * i + 1.0);
      sign *= -1;
    }
    pi_ = 4.0 * sum;
  }

  void Done() override {
    ++done_call_count_;
    VLOG(1) << "PI_COMPUTE_DONE : " << std::this_thread::get_id();
  }

  int done_call_count() const { return done_call_count_; }
  int work_call_count() const { return work_call_count_; }

 private:
  int done_call_count_ = 0;
  int work_call_count_ = 0;

  double pi_;
};

}  // namespace

class LibuvDispatcherTest : public ::testing::Test {
 public:
  LibuvDispatcherTest()
      : api_(std::make_unique<APIImpl>(&time_system_)),
        dispatcher_(api_->AllocateDispatcher("test_loop")) {}

 protected:
  RealTimeSystem time_system_;
  std::unique_ptr<API> api_;
  std::unique_ptr<Dispatcher> dispatcher_;
};

TEST_F(LibuvDispatcherTest, test_timed_events) {
  // This test is meant to exercise the event code so we can find any TSAN/ASAN failures.
  // It is not meant to be a precise test for the timing logic since that will likely make the
  // test flaky.
  // TODO(zasgar): Fix a fake TSAN error caused by incorrectly identified race on
  // dispatcher_->Stop(). Related: https://github.com/nodejs/node/issues/26100
  // https://github.com/joyent/libuv/issues/1198

  int timer1_call_count = 0;
  std::atomic<int> timer_done_count = 0;
  std::thread watcher_th([&timer_done_count, this] {
    while (timer_done_count != 2) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      // Spin.
    }
    this->dispatcher_->Stop();
  });

  auto timer1 = dispatcher_->CreateTimer([&timer1_call_count, &timer_done_count] {
    ++timer1_call_count;
    timer_done_count += 1;
  });

  EXPECT_FALSE(timer1->Enabled());
  timer1->EnableTimer(std::chrono::milliseconds(200));
  EXPECT_TRUE(timer1->Enabled());

  // This checks that timers can be re-enabled in the loop.
  int timer2_call_count = 0;
  std::unique_ptr<px::event::Timer> timer2;
  timer2 = dispatcher_->CreateTimer([&timer2, &timer2_call_count, &timer_done_count] {
    static int count = 0;
    ++timer2_call_count;
    if (++count > 2) {
      timer2->DisableTimer();
      timer_done_count += 1;
      return;
    }
    timer2->EnableTimer(std::chrono::milliseconds(100));
  });
  timer2->EnableTimer(std::chrono::milliseconds(100));

  dispatcher_->Run(Dispatcher::RunType::Block);
  dispatcher_->Exit();
  watcher_th.join();

  EXPECT_EQ(1, timer1_call_count);
  EXPECT_EQ(3, timer2_call_count);
}

TEST_F(LibuvDispatcherTest, threadpool) {
  auto task = std::make_unique<ComputePi>();
  // Store the pointer so that we can access the results later.
  auto task_ptr = task.get();
  auto runnable = dispatcher_->CreateAsyncTask(std::move(task));
  runnable->Run();

  dispatcher_->Run(Dispatcher::RunType::RunUntilExit);

  EXPECT_EQ(1, task_ptr->work_call_count());
  EXPECT_EQ(1, task_ptr->done_call_count());

  EXPECT_EQ(0, g_compute_pi_destructor_count);

  dispatcher_->DeferredDelete(std::move(runnable));
  dispatcher_->Run(Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, g_compute_pi_destructor_count);

  dispatcher_->Exit();
}

}  // namespace event
}  // namespace px
