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

#include <uv.h>

#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <absl/base/attributes.h>
#include <absl/base/thread_annotations.h>
#include <absl/synchronization/mutex.h>
#include "src/common/event/dispatcher.h"
#include "src/common/event/event.h"

namespace px {
namespace event {

class API;

class LibuvTimer : public Timer {
 public:
  LibuvTimer(TimerCB cb, uv_loop_t* loop);
  // Ensure that DeferredDelete is called before the destructor is invoked.
  // Otherwise, the event loop might still invoke the callback function
  // of this timer.
  ~LibuvTimer() override;

  void DisableTimer() override;
  void EnableTimer(const std::chrono::milliseconds& ms) override;
  bool Enabled() override;

 private:
  TimerCB cb_;
  uv_timer_t timer_;
};

class LibuvScheduler : public Scheduler {
 public:
  explicit LibuvScheduler(std::string_view name);

  TimerUPtr CreateTimer(const TimerCB& cb, Dispatcher* dispatcher) override;
  void Stop();
  void Run(Dispatcher::RunType type);
  RunnableAsyncTaskUPtr CreateAsyncTask(std::unique_ptr<AsyncTask> task);
  uv_loop_t* uv_loop() { return &uv_loop_; }

  /**
   * Exits the libuv loop.
   */
  void LoopExit();
  std::string LogEntry(std::string_view entry);

 private:
  const std::string name_;
  uv_loop_t uv_loop_;
  uv_async_t stop_handler_;
};

class LibuvDispatcher : public Dispatcher {
 public:
  LibuvDispatcher(std::string_view name, const API& api, TimeSystem* time_system);
  TimerUPtr CreateTimer(TimerCB cb) override;
  const TimeSource& GetTimeSource() const override;
  void Stop() override;
  void Exit() override;
  void Post(PostCB callback) override;
  void DeferredDelete(DeferredDeletableUPtr&& to_delete) override;
  void Run(RunType type) override;
  RunnableAsyncTaskUPtr CreateAsyncTask(std::unique_ptr<AsyncTask> task) override;
  MonotonicTimePoint ApproximateMonotonicTime() const override;
  void UpdateMonotonicTime() override;
  std::string LogEntry(std::string_view entry);
  uv_loop_t* uv_loop() { return base_scheduler_.uv_loop(); }

 private:
  bool IsCorrectThread() {
    // Make sure that the thread that executed run() is the same as the thread calling thread unsafe
    // functions. This is a requirement for libuv.
    return run_tid_ == std::thread::id() || run_tid_ == std::this_thread::get_id();
  }

  void RunPostCallbacks();
  void DoDeferredDelete();

  const std::string name_;
  std::thread::id run_tid_;

  absl::Mutex post_lock_;
  std::list<PostCB> post_callbacks_ ABSL_GUARDED_BY(post_lock_);
  uv_async_t post_async_handler_;

  const API& api_;
  LibuvScheduler base_scheduler_;
  SchedulerUPtr scheduler_;
  MonotonicTimePoint approximate_monotonic_time_;

  // We maintain two deletion lists (double buffer), so that we support deleters calling other
  // deleters.
  std::vector<DeferredDeletableUPtr> to_delete_1_;
  std::vector<DeferredDeletableUPtr> to_delete_2_;
  std::vector<DeferredDeletableUPtr>* current_to_delete_ = nullptr;
  bool deferred_deleting_ = false;
  TimerUPtr deferred_delete_timer_;
};

}  // namespace event
}  // namespace px
