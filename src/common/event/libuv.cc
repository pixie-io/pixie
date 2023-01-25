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

#include "src/common/event/libuv.h"

#include <uv.h>

#include <memory>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/event/api.h"

namespace px {
namespace event {

namespace {
/**
 * LibuvRunnableAsyncTask is a wrapper that contains Libuv specific run loop.
 */
class LibuvRunnableAsyncTask : public RunnableAsyncTask {
 public:
  LibuvRunnableAsyncTask(uv_loop_t* loop, std::unique_ptr<AsyncTask> task)
      : RunnableAsyncTask(std::move(task)), loop_(loop) {
    work_.data = this;
  }

  virtual ~LibuvRunnableAsyncTask() = default;

  /**
   * Run queues work on the event queue to run on the threadpool.
   */
  void Run() override { ECHECK(uv_queue_work(loop_, &work_, WorkCB, AfterWorkCB) == 0); }

 private:
  static void inline WorkCB(uv_work_t* req) {
    auto self = static_cast<LibuvRunnableAsyncTask*>(req->data);
    self->task_->Work();
  }

  static void inline AfterWorkCB(uv_work_t* req, int status) {
    PX_UNUSED(status);
    auto self = static_cast<LibuvRunnableAsyncTask*>(req->data);
    self->task_->Done();
  }

  uv_loop_t* loop_;
  uv_work_t work_;
};

void OnUVWalkClose(uv_handle_t* handle, void* /*arg*/) { uv_close(handle, nullptr); }

}  // namespace

//----- Timer
LibuvTimer::LibuvTimer(TimerCB cb, uv_loop_t* loop) : cb_(cb) {
  int rc = uv_timer_init(loop, &timer_);
  CHECK(rc == 0) << "Failed to initialize uv_timer";
}
void LibuvTimer::DisableTimer() {
  int rc = uv_timer_stop(&timer_);
  CHECK(rc == 0) << "Failed to disabled uv_timer";
}

void LibuvTimer::EnableTimer(const std::chrono::milliseconds& ms) {
  timer_.data = this;
  uv_timer_cb timer_cb = [](uv_timer_t* t) {
    auto self = reinterpret_cast<LibuvTimer*>(t->data);
    self->cb_();
  };

  int rc = uv_timer_start(&timer_, timer_cb, ms.count(), 0);
  CHECK(rc == 0) << "Failed to start timer";
}

bool LibuvTimer::Enabled() {
  return uv_is_active(reinterpret_cast<const uv_handle_t*>(&timer_)) != 0;
}

LibuvTimer::~LibuvTimer() {
  if (Enabled()) {
    DisableTimer();
  }
  auto h = reinterpret_cast<uv_handle_t*>(&timer_);
  if (!uv_is_closing(h)) {
    uv_close(h, nullptr);
  }
}

//----- Scheduler

TimerUPtr LibuvScheduler::CreateTimer(const TimerCB& cb, Dispatcher*) {
  return std::make_unique<LibuvTimer>(cb, &uv_loop_);
}

void LibuvScheduler::Run(Dispatcher::RunType type) {
  uv_run_mode mode;
  switch (type) {
    case Dispatcher::RunType::Block:
      mode = UV_RUN_DEFAULT;
      break;
    case Dispatcher::RunType::NonBlock:
      mode = UV_RUN_NOWAIT;
      break;
    case Dispatcher::RunType::RunUntilExit:
      mode = UV_RUN_ONCE;
      break;
    default:
      CHECK(0) << "Unsupported run mode";
  }
  uv_run(&uv_loop_, mode);
}

LibuvScheduler::LibuvScheduler(std::string_view name) : name_(std::string(name)) {
  int rc = uv_loop_init(&uv_loop_);
  CHECK(rc == 0) << "Failed to init Libuv loop";
  stop_handler_.data = this;
  uv_async_init(&uv_loop_, &stop_handler_, [](uv_async_t* h) {
    LibuvScheduler* s = reinterpret_cast<LibuvScheduler*>(h->data);
    LOG(INFO) << s->LogEntry("Loop stopped");
    uv_close(reinterpret_cast<uv_handle_t*>(h), [](uv_handle_t* h) {
      LibuvScheduler* s = reinterpret_cast<LibuvScheduler*>(h->data);
      LOG(INFO) << s->LogEntry("Stop finished");
    });
  });
}

void LibuvScheduler::LoopExit() {
  // Should signal run loop to exit, and also wait for exit to complete.
  if (int rc = uv_loop_close(&uv_loop_); rc != 0) {
    if (rc == UV_EBUSY) {
      uv_walk(&uv_loop_, OnUVWalkClose, NULL);
      // Give it a chance to cleanup.
      Run(Dispatcher::RunType::Block);
    } else {
      CHECK(0) << "Got unexpected error closing event loop: " << uv_strerror(rc);
    }
  }

  if (int rc = uv_loop_close(&uv_loop_); rc != 0) {
    LOG(ERROR) << "Failed to close event loop " << uv_strerror(rc);
  }
}

void LibuvScheduler::Stop() {
  LOG(INFO) << LogEntry("Stop called");
  int rc = uv_async_send(&stop_handler_);
  CHECK(rc == 0) << "Failed to stop scheduler";
}

RunnableAsyncTaskUPtr LibuvScheduler::CreateAsyncTask(std::unique_ptr<AsyncTask> task) {
  return std::make_unique<LibuvRunnableAsyncTask>(&uv_loop_, std::move(task));
}

std::string LibuvScheduler::LogEntry(std::string_view entry) {
  std::stringstream current_tid_ss;
  current_tid_ss << std::this_thread::get_id();

  return absl::Substitute("n:$0|c:$1:: $2", name_, current_tid_ss.str(), entry);
}

//----- Dispatcher

void LibuvDispatcher::Exit() { base_scheduler_.LoopExit(); }

void LibuvDispatcher::Post(PostCB cb) {
  bool activate = false;
  {
    absl::MutexLock lock(&post_lock_);
    activate = post_callbacks_.empty();
    post_callbacks_.emplace_back(cb);
  }

  if (activate) {
    int rc = uv_async_send(&post_async_handler_);
    CHECK(rc == 0) << "Failed to schedule post processing";
  }
}

void LibuvDispatcher::DeferredDelete(DeferredDeletableUPtr&& to_delete) {
  CHECK(IsCorrectThread());
  current_to_delete_->emplace_back(std::move(to_delete));
  if (current_to_delete_->size() == 1) {
    deferred_delete_timer_->EnableTimer(std::chrono::milliseconds{0});
  }
}

void LibuvDispatcher::Run(Dispatcher::RunType type) {
  run_tid_ = std::this_thread::get_id();

  RunPostCallbacks();
  base_scheduler_.Run(type);
}

MonotonicTimePoint LibuvDispatcher::ApproximateMonotonicTime() const {
  return approximate_monotonic_time_;
}

const TimeSource& LibuvDispatcher::GetTimeSource() const { return api_.TimeSourceRef(); }

void LibuvDispatcher::UpdateMonotonicTime() {
  approximate_monotonic_time_ = api_.TimeSourceRef().MonotonicTime();
}

LibuvDispatcher::LibuvDispatcher(std::string_view name, const API& api, TimeSystem* time_system)
    : name_(std::string(name)),
      api_(api),
      base_scheduler_(name),
      scheduler_(time_system->CreateScheduler(&base_scheduler_)),
      current_to_delete_(&to_delete_1_),
      deferred_delete_timer_(CreateTimer([this]() { DoDeferredDelete(); })) {
  UpdateMonotonicTime();

  post_async_handler_.data = this;
  uv_async_init(base_scheduler_.uv_loop(), &post_async_handler_, [](uv_async_t* h) {
    LibuvDispatcher* d = reinterpret_cast<LibuvDispatcher*>(h->data);
    d->RunPostCallbacks();
  });
  // Don't block the event loop if this is the only reference.
  uv_unref(reinterpret_cast<uv_handle_t*>(&post_async_handler_));
}

TimerUPtr LibuvDispatcher::CreateTimer(TimerCB cb) {
  CHECK(IsCorrectThread());
  return scheduler_->CreateTimer(cb, this);
}

void LibuvDispatcher::RunPostCallbacks() {
  while (true) {
    PostCB cb;
    {
      absl::MutexLock lock(&post_lock_);
      if (post_callbacks_.empty()) {
        return;
      }
      cb = post_callbacks_.front();
      post_callbacks_.pop_front();
    }
    cb();
  }
}

void LibuvDispatcher::Stop() {
  uv_close(reinterpret_cast<uv_handle_t*>(&post_async_handler_), nullptr);
  base_scheduler_.Stop();
}

RunnableAsyncTaskUPtr LibuvDispatcher::CreateAsyncTask(std::unique_ptr<AsyncTask> task) {
  return base_scheduler_.CreateAsyncTask(std::move(task));
}

std::string LibuvDispatcher::LogEntry(std::string_view entry) {
  return base_scheduler_.LogEntry(entry);
}

void LibuvDispatcher::DoDeferredDelete() {
  std::vector<DeferredDeletableUPtr>& to_delete = *current_to_delete_;

  // Nothing to delete, or delete in progress.
  size_t num_to_delete = to_delete.size();
  if (deferred_deleting_ || !num_to_delete) {
    return;
  }

  // Double buffer swap to allow deletes to call other deleters.
  if (current_to_delete_ == &to_delete_1_) {
    current_to_delete_ = &to_delete_2_;
  } else {
    current_to_delete_ = &to_delete_1_;
  }

  // Loop through to delete to make sure deletion is done in the right order.
  deferred_deleting_ = true;
  for (size_t i = 0; i < to_delete.size(); ++i) {
    to_delete[i].reset();
  }
  to_delete.clear();
  deferred_deleting_ = false;
}

}  // namespace event
}  // namespace px
