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

#include <grpcpp/grpcpp.h>
#include <algorithm>

#include <sole.hpp>

#include "src/vizier/services/agent/kelvin/kelvin_manager.h"
#include "src/vizier/services/agent/shared/base/lifecycle.h"

#include "src/common/base/base.h"
#include "src/common/event/nats.h"
#include "src/common/signal/signal.h"
#include "src/shared/version/version.h"

DEFINE_string(nats_url, gflags::StringFromEnv("PL_NATS_URL", "pl-nats"),
              "The host address of the nats cluster");

DEFINE_string(mds_addr, gflags::StringFromEnv("PL_MDS_SVC_NAME", "vizier-metadata-svc"),
              "The service name of MDS");

DEFINE_string(mds_port, gflags::StringFromEnv("PL_MDS_PORT", "50400"),
              "The port of the metadata service");

DEFINE_string(namespace, gflags::StringFromEnv("PL_POD_NAMESPACE", "pl"),
              "The namespace of the Vizier instance");

DEFINE_string(pod_ip, gflags::StringFromEnv("PL_POD_IP", ""),
              "The IP address of the pod this controller is running on");

DEFINE_int32(rpc_port, gflags::Int32FromEnv("PL_RPC_PORT", 59300), "The port of the RPC server");

DEFINE_string(pod_name, gflags::StringFromEnv("PL_POD_NAME", ""),
              "The name of the POD the PEM is running on");

DEFINE_string(host_ip, gflags::StringFromEnv("PL_HOST_IP", ""),
              "The IP of the host this service is running on");

using ::px::vizier::agent::DefaultDeathHandler;
using ::px::vizier::agent::KelvinManager;
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
  LOG(INFO) << absl::Substitute("Pixie Kelvin. Version: $0, id: $1",
                                px::VersionInfo::VersionString(), agent_id.str());
  if (FLAGS_pod_ip.length() == 0) {
    LOG(FATAL) << "The POD_IP must be specified";
  }
  if (FLAGS_host_ip.length() == 0) {
    LOG(FATAL) << "The HOST_IP must be specified";
  }

  std::string addr = absl::Substitute("$0:$1", FLAGS_pod_ip, FLAGS_rpc_port);
  if (FLAGS_pod_ip[0] != '[' && FLAGS_pod_ip.rfind(':') != FLAGS_pod_ip.npos) {
    // IPv6 literals must be enclosed in brackets.
    addr = absl::Substitute("[$0]:$1", FLAGS_pod_ip, FLAGS_rpc_port);
  }

  std::string mds_addr =
      absl::Substitute("$0.$1.svc:$2", FLAGS_mds_addr, FLAGS_namespace, FLAGS_mds_port);

  auto manager = KelvinManager::Create(agent_id, FLAGS_pod_name, FLAGS_host_ip, addr,
                                       FLAGS_rpc_port, FLAGS_nats_url, mds_addr)
                     .ConsumeValueOrDie();

  TerminationHandler::set_manager(manager.get());

  PX_CHECK_OK(manager->Run());
  PX_CHECK_OK(manager->Stop(std::chrono::seconds{1}));

  // Clear the manager, because it has been stopped.
  TerminationHandler::set_manager(nullptr);

  return 0;
}
