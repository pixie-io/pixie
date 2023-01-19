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

#include <sole.hpp>

#include "src/shared/version/version.h"
#include "src/vizier/services/agent/shared/base/lifecycle.h"

using ::px::vizier::agent::DefaultDeathHandler;
using ::px::vizier::agent::TerminationHandler;

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  DefaultDeathHandler err_handler;

  // This covers signals such as SIGSEGV and other fatal errors.
  // We print the stack trace and die.
  auto signal_action = std::make_unique<px::SignalAction>();
  signal_action->RegisterFatalErrorHandler(err_handler);

  // Install signal handlers where graceful exit is possible.
  TerminationHandler::InstallSignalHandlers();

  sole::uuid agent_id = sole::uuid4();
  LOG(INFO) << absl::Substitute("Pixie PEM. Version: $0, id: $1", px::VersionInfo::VersionString(),
                                agent_id.str());
}
