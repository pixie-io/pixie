#pragma once

#include <deque>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "src/common/base/base.h"

namespace pl {
namespace sync {

class ThreadPool {
 private:
  struct Task {
    std::string name;
    std::function<void()> func;
  };

 public:
  /**
   * Create a thread pool with the specified number of threads.
   *
   * The thread pool will maintain this number of threads and will not dynamically
   * resize.
   * @param num_threads The number of threads.
   */
  explicit ThreadPool(int num_threads) {
    for (int i = 0; i < num_threads; ++i) {
      workers_.emplace_back(std::thread(&ThreadPool::WorkerLoop, this));
    }
  }

  ~ThreadPool() {
    keep_running_ = false;
    int work_count = 0;
    {
      absl::MutexLock l(&mu_);
      work_count = q_.size();
      // Clear the queue.
      std::deque<Task>().swap(q_);
    }
    LOG_IF(ERROR, work_count > 0) << "Thread pool terminating with pending work";

    // TODO(zasgar): We probably want to hard terminate after some timeout. Otherwise this might
    // hang.
    for (auto& th : workers_) {
      th.join();
    }
  }

  /**
   * Enqueue new work to exectue in the pool.
   * @return A future with the return value of the specified function.
   */
  template <typename F, typename... Args>
  auto Enqueue(std::string_view task_name, F&& f, Args&&... args)
      -> StatusOr<std::future<typename std::result_of_t<F(Args...)>>> {
    using return_type = typename std::result_of_t<F(Args...)>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    if (!keep_running_) {
      return error::ResourceUnavailable("thread pool has already started termination");
    }

    std::future<return_type> res = task->get_future();
    {
      absl::MutexLock l(&mu_);
      q_.emplace_back(Task{std::string(task_name), [task]() { (*task)(); }});
    }

    return res;
  }

  /**
   *  Get the number of jobs left in the queue.
   */
  int JobsRemaining() const {
    absl::MutexLock l(&mu_);
    return q_.size();
  }

  /**
   * Wait for the work to drain.
   */
  void WaitDrain() const {
    absl::MutexLock l(&mu_);
    mu_.Await(absl::Condition(this, &ThreadPool::WorkFinished));
  }

  std::string DebugString() const {
    absl::MutexLock l(&mu_);
    if (q_.empty()) {
      return "No tasks are running";
    }

    std::string s = "Following tasks in exec queue:";
    for (auto& t : q_) {
      s += absl::StrFormat("%s\n", t.name);
    }
    return s;
  }

 private:
  bool HasWorkAvailable() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) { return !q_.empty(); }

  bool WorkFinished() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    return !HasWorkAvailable() && tasks_running == 0;
  }

  bool WorkerShouldUnblock() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    return HasWorkAvailable() || keep_running_ == false;
  }

  void WorkerLoop() {
    while (true) {
      Task task;
      {
        absl::MutexLock l(&mu_);
        mu_.Await(absl::Condition(this, &ThreadPool::WorkerShouldUnblock));
        if (!keep_running_) {
          // Termination signal.
          return;
        }
        task = std::move(q_.front());
        q_.pop_front();
      }
      tasks_running++;
      VLOG(1) << "Running task: " << task.name;
      task.func();
      tasks_running--;
    }
  }

  mutable absl::Mutex mu_;
  std::atomic<bool> keep_running_ = true;
  std::atomic<int> tasks_running = 0;
  std::deque<Task> q_ ABSL_GUARDED_BY(mu_);
  std::vector<std::thread> workers_;
};

}  // namespace sync
}  // namespace pl
