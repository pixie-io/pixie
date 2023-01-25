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

#include "src/experimental/standalone_pem/standalone_pem_manager.h"
#include "src/shared/version/version.h"
#include "src/vizier/services/agent/shared/base/lifecycle.h"

using ::px::vizier::agent::DefaultDeathHandler;
using ::px::vizier::agent::StandalonePEMManager;
using ::px::vizier::agent::TerminationHandler;

DEFINE_int32(port, gflags::Int32FromEnv("PX_STANDALONE_PEM_PORT", 12345),
             "The port where the PEM receives GRPC requests.");

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

  auto manager = StandalonePEMManager::Create(agent_id, /* host_ip */ "0.0.0.0", FLAGS_port)
                     .ConsumeValueOrDie();
  TerminationHandler::set_manager(manager.get());

  PX_CHECK_OK(manager->Run());
  PX_CHECK_OK(manager->Stop(std::chrono::seconds{5}));

  // Clear the manager, because it has been stopped.
  TerminationHandler::set_manager(nullptr);

  return 0;
}
