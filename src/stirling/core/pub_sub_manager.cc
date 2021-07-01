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

void PopulatePublishProto(Publish* publish_pb, const InfoClassManagerVec& info_class_mgrs,
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

}  // namespace stirling
}  // namespace px
