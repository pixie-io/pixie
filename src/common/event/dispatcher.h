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

#include "src/common/event/deferred_delete.h"
#include "src/common/event/task.h"
#include "src/common/event/time_system.h"
#include "src/common/event/timer.h"

namespace px {
namespace event {

/**
 * Callback invoked when a dispatcher post() runs.
 */
using PostCB = std::function<void()>;
using PoolExecFunc = std::function<void()>;
using PoolExecCompletionCB = std::function<void()>;

/**
 * Dispatcher is the high level class for manging event dispatching (time sources, fs reads, etc.).
 */
class Dispatcher {
 public:
  virtual ~Dispatcher() = default;

  /**
   * CreateTimer
   * @param cb
   * @return
   */
  virtual TimerUPtr CreateTimer(TimerCB cb) = 0;

  /**
   * Returns a time-source to use with this dispatcher.
   */
  virtual const TimeSource& GetTimeSource() const = 0;

  /**
   * Stops the event loop. The event loop can be resumed or terminated by calling Exit().
   * This function is safe to call from any thread.
   */
  virtual void Stop() = 0;

  /**
   * Exits the event loop.
   */
  virtual void Exit() = 0;

  /**
   * Posts a functor to the dispatcher. This is safe cross thread. The functor runs in the context
   * of the dispatcher event loop which may be on a different thread than the caller.
   */
  virtual void Post(PostCB callback) = 0;

  /**
   * Will delete the passed in pointer in a future event loop.
   * This allows us to delete unique_ptr, with a clear ownership model.
   */
  virtual void DeferredDelete(DeferredDeletableUPtr&& to_delete) = 0;

  /**
   * RunType specifies the run loop.
   */
  enum class RunType {
    Block,        // Executes any events that have been activated, then exit.
    NonBlock,     // Waits for any pending events to activate, executes them,
                  // then exits. Exits immediately if there are no pending or
                  // active events.
    RunUntilExit  // Runs the event-loop until loopExit() is called, blocking
                  // until there are pending or active events.
  };

  /**
   * Run the event loop.
   */
  virtual void Run(RunType type) = 0;

  /**
   * Create and async task that can be scheduled on the thread pool.
   * @param task the task to run. It will be owned by the returned RunnableAsyncTask.
   * @return runnable task. The lifetime of this needs to exceed the Run/Done functions.
   * It is best to mark it as deferred delete in the Done function.
   */
  virtual RunnableAsyncTaskUPtr CreateAsyncTask(std::unique_ptr<AsyncTask> task) = 0;

  /**
   * Returns a recently cached MonotonicTime value. Updates on every iteration of the event loop.
   */
  virtual MonotonicTimePoint ApproximateMonotonicTime() const = 0;

  /**
   * Force update of the monotonic time.
   */
  virtual void UpdateMonotonicTime() = 0;
};

using DispatcherUPtr = std::unique_ptr<Dispatcher>;

/**
 * Scheduler is responsible for scheduling events.
 */
class Scheduler {
 public:
  virtual ~Scheduler() = default;

  /**
   * Creates a timer.
   */
  virtual TimerUPtr CreateTimer(const TimerCB& cb, Dispatcher* dispatcher) = 0;
};

using SchedulerUPtr = std::unique_ptr<Scheduler>;

}  // namespace event
}  // namespace px
