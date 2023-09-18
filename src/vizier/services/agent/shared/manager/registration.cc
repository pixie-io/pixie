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

#include <memory>
#include <utility>
#include <vector>

#include "src/vizier/services/agent/shared/base/info.h"
#include "src/vizier/services/agent/shared/manager/manager.h"
#include "src/vizier/services/agent/shared/manager/registration.h"

namespace px {
namespace vizier {
namespace agent {

using ::px::event::Dispatcher;
using ::px::shared::k8s::metadatapb::ResourceUpdate;

RegistrationHandler::RegistrationHandler(px::event::Dispatcher* dispatcher, Info* agent_info,
                                         Manager::VizierNATSConnector* nats_conn,
                                         RegistrationHook post_registration_hook,
                                         RegistrationHook post_reregistration_hook)
    : MessageHandler(dispatcher, agent_info, nats_conn),
      post_registration_hook_(post_registration_hook),
      post_reregistration_hook_(post_reregistration_hook) {
  registration_timeout_ = dispatcher->CreateTimer([this] {
    absl::base_internal::SpinLockHolder lock(&registration_lock_);
    registration_timeout_.release();
    registration_wait_.release();

    if (!registration_in_progress_) {
      return;
    }
    LOG(FATAL) << "Timeout waiting for registration ack";
  });

  registration_wait_ = dispatcher->CreateTimer([this] {
    registration_wait_->DisableTimer();
    auto s = DispatchRegistration();
    LOG_IF(FATAL, !s.ok()) << absl::Substitute("Failed to register agent: $0", s.msg());
    registration_timeout_->EnableTimer(kRegistrationPeriod);
  });
}

// convert px::system::KernelVersion to px::vizier::services::shared::agent:KernelVersion
::px::vizier::services::shared::agent::KernelVersion KernelToProto(
    const system::KernelVersion& kv) {
  ::px::vizier::services::shared::agent::KernelVersion kv_proto;
  kv_proto.set_version(kv.version);
  kv_proto.set_major_rev(kv.major_rev);
  kv_proto.set_minor_rev(kv.minor_rev);
  return kv_proto;
}

Status RegistrationHandler::DispatchRegistration() {
  // Send the registration request.
  messages::VizierMessage req;

  {
    absl::base_internal::SpinLockHolder lock(&registration_lock_);
    // Reuse the same ASID if the agent has registered in the past to preserve UPIDs.
    if (ever_registered_) {
      LOG_IF(FATAL, agent_info()->asid == 0) << "Reregistering agent which is not yet registered";
      req.mutable_register_agent_request()->set_asid(agent_info()->asid);
    } else {
      LOG_IF(FATAL, agent_info()->asid != 0) << "Registering agent has already been registered";
    }
  }

  auto req_info = req.mutable_register_agent_request()->mutable_info();

  ToProto(agent_info()->agent_id, req_info->mutable_agent_id());
  req_info->set_ip_address(agent_info()->address);
  auto host_info = req_info->mutable_host_info();
  host_info->set_hostname(agent_info()->hostname);
  host_info->set_pod_name(agent_info()->pod_name);
  host_info->set_host_ip(agent_info()->host_ip);
  auto kernel_version_proto = KernelToProto(agent_info()->kernel_version);
  host_info->mutable_kernel()->CopyFrom(kernel_version_proto);
  *req_info->mutable_capabilities() = agent_info()->capabilities;
  *req_info->mutable_parameters() = agent_info()->parameters;

  PX_RETURN_IF_ERROR(nats_conn()->Publish(req));
  return Status::OK();
}

void RegistrationHandler::RegisterAgent(bool reregister) {
  {
    absl::base_internal::SpinLockHolder lock(&registration_lock_);
    if (registration_in_progress_) {
      // Registration already in progress.
      return;
    }
    registration_in_progress_ = true;
    LOG_IF(FATAL, reregister != ever_registered_)
        << (reregister ? "Re-registering agent that does not exist"
                       : "Registering already registered agent");
  }

  // Send the agent info.

  // Wait a random amount of time before registering. This is so the agents don't swarm the
  // metadata service all at the same time when Vizier first starts up.
  std::random_device rnd_device;
  std::mt19937_64 eng{rnd_device()};
  std::uniform_int_distribution<> dist{
      kMinWaitTimeMillis,
      kMaxWaitTimeMillis};  // Wait a random amount of time between 10ms to 1min.

  registration_wait_->EnableTimer(std::chrono::milliseconds{dist(eng)});
}

Status RegistrationHandler::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  LOG_IF(FATAL, !msg->has_register_agent_response())
      << "Did not get register agent response. Got: " << msg->DebugString();

  absl::base_internal::SpinLockHolder lock(&registration_lock_);

  if (!registration_in_progress_) {
    // We may have gotten a duplicate registration response from NATS.
    return Status::OK();
  }
  registration_timeout_->DisableTimer();
  registration_in_progress_ = false;

  if (!ever_registered_) {
    ever_registered_ = true;
    return post_registration_hook_(msg->register_agent_response().asid());
  }
  return post_reregistration_hook_(msg->register_agent_response().asid());
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
