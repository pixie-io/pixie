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
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/proto/stirling.pb.h"
#include "src/table_store/schema/relation.h"

namespace px {

/**
 * A relation and accompanying information such as names and ids.
 */
struct RelationInfo {
  RelationInfo() = default;
  RelationInfo(std::string name, uint64_t id, std::string desc,
               table_store::schema::Relation relation)
      : name(std::move(name)),
        id(id),
        desc(std::move(desc)),
        tabletized(false),
        relation(std::move(relation)) {}

  RelationInfo(std::string name, uint64_t id, std::string desc, uint64_t tabletization_key_idx,
               table_store::schema::Relation relation)
      : name(std::move(name)),
        id(id),
        desc(std::move(desc)),
        tabletized(true),
        tabletization_key_idx(tabletization_key_idx),
        relation(std::move(relation)) {}

  std::string name;
  uint64_t id;
  std::string desc;
  bool tabletized;
  uint64_t tabletization_key_idx;
  table_store::schema::Relation relation;
};

/**
 * Converts a subscription proto to relation info vector.
 * @param subscribe_pb The subscription proto.
 * @return Relation vector.
 */
std::vector<RelationInfo> ConvertPublishPBToRelationInfo(const stirling::stirlingpb::Publish& pb);

}  // namespace px
