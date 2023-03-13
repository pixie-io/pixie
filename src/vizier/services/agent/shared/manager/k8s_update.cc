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

#include "src/vizier/services/agent/shared/manager/k8s_update.h"

#include <memory>
#include <utility>
#include <vector>

#include "src/vizier/services/agent/shared/manager/manager.h"

namespace px {
namespace vizier {
namespace agent {

K8sUpdateHandler::K8sUpdateHandler(Dispatcher* d, px::md::AgentMetadataStateManager* mds_manager,
                                   Info* agent_info, Manager::VizierNATSConnector* nats_conn,
                                   const std::string& update_selector, size_t max_update_queue_size)
    : MessageHandler(d, agent_info, nats_conn),
      mds_manager_(mds_manager),
      update_selector_(update_selector),
      missing_metadata_request_timer_(
          dispatcher()->CreateTimer(std::bind(&K8sUpdateHandler::RequestMissingMetadata, this))),
      update_backlog_([](const ResourceUpdate& left, const ResourceUpdate& right) {
        return left.update_version() > right.update_version();
      }),
      max_update_queue_size_(max_update_queue_size) {
  // Request the initial backlog of missing metadata.
  missing_metadata_request_timer_->EnableTimer(std::chrono::milliseconds(0));
}

Status K8sUpdateHandler::AddK8sUpdate(const ResourceUpdate& update) {
  current_update_version_ = update.update_version();
  return mds_manager_->AddK8sUpdate(std::make_unique<ResourceUpdate>(update));
}

Status K8sUpdateHandler::HandleMissingK8sMetadataResponse(const MissingK8sMetadataResponse& resp) {
  if (resp.updates().size() > 0) {
    auto first_update = resp.updates()[0];
    // If the metadata service tells us that update N is the earliest update it has,
    // then we should consider that the new starting point, even if we are at an update
    // smaller than N-1.
    if (current_update_version_ < first_update.update_version() &&
        first_update.update_version() == resp.first_update_available()) {
      current_update_version_ = first_update.prev_update_version();
    }
  }

  if (resp.updates().size() == 0 && update_backlog_.size() > 0) {
    // If our backlog now predates the earliest updates known by the metadata service,
    // we need to write out the backlog, and stop trying to find missing updates that
    // no longer exist in the metadata service.
    if (resp.first_update_available() >= update_backlog_.top().update_version()) {
      current_update_version_ = update_backlog_.top().prev_update_version();
    }
  }

  for (const auto& update : resp.updates()) {
    PX_RETURN_IF_ERROR(HandleK8sUpdate(update));
  }

  // Check and flush backlog.
  // if backlog is done, disable timer
  while (update_backlog_.size()) {
    auto next = update_backlog_.top();
    // Break out of the loop if the backlog is still ahead of the
    // current resource version.
    if (current_update_version_ < next.prev_update_version()) {
      break;
    }

    // If this is the update that we need, then add it.
    if (current_update_version_ == next.prev_update_version()) {
      PX_RETURN_IF_ERROR(AddK8sUpdate(next));
    }
    update_backlog_.pop();
  }

  // Check to see if we have now received the initial batch of metadata, after startup.
  if (!initial_metadata_received_) {
    // Check to see if we have received the full batch of initial metadata.
    if (current_update_version_ >= resp.last_update_available()) {
      initial_metadata_received_ = true;
    }
  }

  // Two cases here:
  // 1. We were waiting on the initial set of metadata from the metadata service. If we
  // received it all, then we can disable the timer.
  // 2. We had received an out of order update. In that case, we needed to make a re-request
  // for the missing metadata in between. In that scenario, checking that the backlog is
  // empty will suffice.
  // If we are still waiting on data, then the timer will go off eventually and the missing
  // metadata will be re-requested.
  if (initial_metadata_received_ && !update_backlog_.size()) {
    missing_metadata_request_timer_->DisableTimer();
  }

  return Status::OK();
}

void K8sUpdateHandler::RequestMissingMetadata() {
  // If there is no backlog, then we don't have any missing metadata that we know of.
  if (!update_backlog_.size() && initial_metadata_received_) {
    return;
  }

  // Send the registration request.
  // Note that we will re-request an update we already have in the range of [from, to),
  // because we have to use our current resource version as the `from` value.
  messages::VizierMessage msg;
  auto req = msg.mutable_k8s_metadata_message()->mutable_missing_k8s_metadata_request();
  req->set_selector(update_selector_);

  if (current_update_version_) {
    // We don't know the resource numbers, but we know they will always be greater by at
    // least 1 than the most recent one we received. However, we keep it as a 0 if we haven't
    // received any updates yet.
    req->set_from_update_version(current_update_version_ + 1);
  }
  // Keep this as a 0 if we don't know the upper bound of the range we are asking for.
  // Note: requesting [from: 0, to: 0) is the same as requesting all
  if (update_backlog_.size()) {
    req->set_to_update_version(update_backlog_.top().update_version());
  }

  auto s = nats_conn()->Publish(msg);
  if (!s.ok()) {
    LOG(ERROR) << absl::Substitute("Failed to request missing K8s updates, retrying in $0 s",
                                   kMissingMetadataTimeout.count());
  }

  // Enable the timer once we make the request for the initial missing metadata.
  // It will get disabled if we see all of the initial data in time.
  missing_metadata_request_timer_->EnableTimer(kMissingMetadataTimeout);
}

Status K8sUpdateHandler::HandleK8sUpdate(const ResourceUpdate& update) {
  if (current_update_version_ == update.prev_update_version()) {
    return AddK8sUpdate(update);
  }

  // This was probably a duplicate, ignore.
  if (current_update_version_ > update.prev_update_version()) {
    return Status::OK();
  }

  // If this update is further along than we expect, add it to the backlog.
  update_backlog_.push(update);
  // Keep the most recent updates so that we know how far ahead the mds is.
  // Re-request updates that we dropped once we catch up.
  if (update_backlog_.size() > max_update_queue_size_) {
    update_backlog_.pop();
  }

  // If we have missing metadata (which we do if we have reached this part of the
  // logic) and we haven't made a recent outbound request for that missing metadata,
  // we should make that request now.
  if (!missing_metadata_request_timer_->Enabled()) {
    RequestMissingMetadata();
  }

  return Status::OK();
}

Status K8sUpdateHandler::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  LOG_IF(FATAL, !msg->has_k8s_metadata_message()) << "Expected K8sMetadataMessage";
  auto k8s_msg = msg->k8s_metadata_message();

  if (k8s_msg.has_k8s_metadata_update()) {
    return HandleK8sUpdate(k8s_msg.k8s_metadata_update());
  }
  if (k8s_msg.has_missing_k8s_metadata_response()) {
    return HandleMissingK8sMetadataResponse(k8s_msg.missing_k8s_metadata_response());
  }

  return error::Internal(
      "Expected either ResourceUpdate or MissingK8sMetadataResponse in K8sMetadataMessage");
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
