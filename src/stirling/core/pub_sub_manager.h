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

#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/stirling/core/info_class_manager.h"
#include "src/stirling/proto/stirling.pb.h"

namespace px {
namespace stirling {

/**
 * Create a proto message from InfoClassManagers (where each have a schema).
 *
 * @param publish_pb pointer to a Publish proto message.
 * @param info_class_mgrs Reference to a vector of info class manager unique pointers.
 * @param filter Generate a publish proto for a single info class, specified by name.
 */
void PopulatePublishProto(stirlingpb::Publish* publish_pb,
                          const InfoClassManagerVec& info_class_mgrs,
                          std::optional<std::string_view> filter = {});

/**
 * Utility function to index a publish message by ID, for quick access.
 */
inline void IndexPublication(const stirlingpb::Publish& pub,
                             absl::flat_hash_map<uint64_t, stirlingpb::InfoClass>* map) {
  for (const auto& info_class : pub.published_info_classes()) {
    (*map)[info_class.id()] = info_class;
  }
}

}  // namespace stirling
}  // namespace px
