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
#include "src/common/base/logging.h"
#include "src/common/signal/signal.h"
#include "src/vizier/services/agent/shared/base/base_manager.h"

namespace px {
namespace vizier {
namespace agent {

class DefaultDeathHandler : public px::FatalErrorHandlerInterface {
 public:
  DefaultDeathHandler() = default;
  void OnFatalError() const override {
    // Stack trace will print automatically; any additional state dumps can be done here.
    // Note that actions here must be async-signal-safe and must not allocate memory.
  }
};

// Signal handlers for graceful termination.
class TerminationHandler {
 public:
  // This list covers signals that are handled gracefully.
  static constexpr auto kSignals = ::px::MakeArray(SIGINT, SIGQUIT, SIGTERM, SIGHUP);

  static void InstallSignalHandlers() {
    for (size_t i = 0; i < kSignals.size(); ++i) {
      signal(kSignals[i], TerminationHandler::OnTerminate);
    }
  }

  static void set_manager(BaseManager* manager) { manager_ = manager; }

  static void OnTerminate(int signum) {
    if (manager_ != nullptr) {
      LOG(INFO) << "Trying to gracefully stop agent manager";
      auto s = manager_->Stop(std::chrono::seconds{5});
      if (!s.ok()) {
        LOG(ERROR) << "Failed to gracefully stop agent manager, it will terminate shortly.";
      }
      exit(signum);
    }
  }

 private:
  inline static BaseManager* manager_ = nullptr;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
