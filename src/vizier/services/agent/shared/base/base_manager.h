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

#include "src/common/base/base.h"

namespace px {
namespace vizier {
namespace agent {

class BaseManager : public px::NotCopyable {
 public:
  BaseManager() = default;
  virtual ~BaseManager() = default;

  /**
   * Run the main event loop. This function blocks and uses the thread to run the event loop.
   * The agent manager will continue to execute until Stop is called.
   */
  virtual Status Run() = 0;

  /**
   * Stops the agent manager.
   * Safe to call from any thread.
   * \note Do not call this function from the destructor.
   */
  virtual Status Stop(std::chrono::milliseconds timeout) = 0;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
