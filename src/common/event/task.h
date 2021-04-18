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
#include <functional>
#include <memory>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/event/deferred_delete.h"

namespace px {
namespace event {

/**
 * AsyncTask is an interface for tasks that we can run on the threadpool.
 */
class AsyncTask {
 public:
  virtual ~AsyncTask() = default;
  /**
   * Work is run on a threadpool.
   */
  virtual void Work() = 0;
  /**
   * Done is called on the event thread after exection of Work() is complete.
   * This function should not preform any blocking or compute intensive
   * operations.
   */
  virtual void Done() = 0;
};
using AsyncTaskUPtr = std::unique_ptr<AsyncTask>;

/**
 * RunnableAsyncTask is a wrapper around an AsyncTask.
 * The lifetime of the contained task must execeed the lifetime of this class.
 * TODO(zasgar): Explore using packaged tasks.
 */
class RunnableAsyncTask : public DeferredDeletable {
 public:
  explicit RunnableAsyncTask(std::unique_ptr<AsyncTask> task) : task_(std::move(task)) {}
  virtual ~RunnableAsyncTask() = default;

  virtual void Run() = 0;

 protected:
  std::unique_ptr<AsyncTask> task_;
};
using RunnableAsyncTaskUPtr = std::unique_ptr<RunnableAsyncTask>;

}  // namespace event
}  // namespace px
