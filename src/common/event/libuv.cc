#include "src/common/event/libuv.h"

#include <uv.h>

#include "src/common/base/base.h"
#include "src/common/event/api.h"

namespace pl {
namespace event {

namespace {
/**
 * LibuvRunnableAsyncTask is a wrapper that contains Libuv specific run loop.
 */
class LibuvRunnableAsyncTask : public RunnableAsyncTask {
 public:
  LibuvRunnableAsyncTask(uv_loop_t* loop, AsyncTask* task) : RunnableAsyncTask(task), loop_(loop) {
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
    PL_UNUSED(status);
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

RunnableAsyncTaskUPtr LibuvScheduler::CreateAsyncTask(AsyncTask* task) {
  return std::make_unique<LibuvRunnableAsyncTask>(&uv_loop_, task);
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
    post_timer_->EnableTimer(std::chrono::milliseconds(0));
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
      post_timer_(CreateTimer([this]() { RunPostCallbacks(); })) {
  UpdateMonotonicTime();
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

void LibuvDispatcher::Stop() { base_scheduler_.Stop(); }

RunnableAsyncTaskUPtr LibuvDispatcher::CreateAsyncTask(AsyncTask* task) {
  return base_scheduler_.CreateAsyncTask(task);
}

std::string LibuvDispatcher::LogEntry(std::string_view entry) {
  return base_scheduler_.LogEntry(entry);
}

}  // namespace event
}  // namespace pl
