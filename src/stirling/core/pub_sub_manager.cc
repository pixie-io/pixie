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

#include <algorithm>

#include "src/common/base/base.h"
#include "src/stirling/core/pub_sub_manager.h"

namespace px {
namespace stirling {

using stirlingpb::InfoClass;
using stirlingpb::Publish;
using stirlingpb::Subscribe;

void PubSubManager::PopulatePublishProto(Publish* publish_pb,
                                         const InfoClassManagerVec& info_class_mgrs,
                                         std::optional<std::string_view> filter) {
  ECHECK(publish_pb != nullptr);
  // For each InfoClassManager get its proto and update publish_message.
  for (auto& schema : info_class_mgrs) {
    if (!filter.has_value() || schema->name() == filter.value()) {
      InfoClass* info_class_proto = publish_pb->add_published_info_classes();
      info_class_proto->MergeFrom(schema->ToProto());
    }
  }
}

Status PubSubManager::UpdateSchemaFromSubscribe(const Subscribe& subscribe_proto,
                                                const InfoClassManagerVec& info_class_mgrs) {
  int num_info_classes = subscribe_proto.subscribed_info_classes_size();
  for (int info_class_idx = 0; info_class_idx < num_info_classes; ++info_class_idx) {
    const auto& info_class_proto = subscribe_proto.subscribed_info_classes(info_class_idx);
    uint64_t id = info_class_proto.id();

    auto it = std::find_if(info_class_mgrs.begin(), info_class_mgrs.end(),
                           [&id](const std::unique_ptr<InfoClassManager>& info_class_ptr) {
                             return info_class_ptr->id() == id;
                           });

    // Check that the InfoClass exists in the map.
    if (it == info_class_mgrs.end()) {
      return error::NotFound("Info Class Schema not found in Config map");
    }

    // Check that the number or elements are the same between the proto
    // and the InfoClassManager object.

    size_t num_elements = info_class_proto.schema().elements_size();
    if (num_elements != (*it)->Schema().elements().size()) {
      return error::Internal("Number of elements in InfoClassManager does not match");
    }

    (*it)->SetSubscription(info_class_proto.subscribed());
    (*it)->SetPushPeriod(std::chrono::milliseconds{info_class_proto.push_period_millis()});
  }
  return Status::OK();
}

stirlingpb::Subscribe SubscribeToAllInfoClasses(const stirlingpb::Publish& publish_proto) {
  stirlingpb::Subscribe subscribe_proto;

  for (const auto& info_class : publish_proto.published_info_classes()) {
    auto sub_info_class = subscribe_proto.add_subscribed_info_classes();
    sub_info_class->MergeFrom(info_class);
    sub_info_class->set_subscribed(true);
  }
  return subscribe_proto;
}

stirlingpb::Subscribe SubscribeToInfoClass(const stirlingpb::Publish& publish_proto,
                                           std::string_view name) {
  stirlingpb::Subscribe subscribe_proto;

  for (const auto& info_class : publish_proto.published_info_classes()) {
    auto sub_info_class = subscribe_proto.add_subscribed_info_classes();
    sub_info_class->CopyFrom(info_class);
    if (sub_info_class->schema().name() == name) {
      sub_info_class->set_subscribed(true);
    }
  }
  return subscribe_proto;
}

}  // namespace stirling
}  // namespace px
