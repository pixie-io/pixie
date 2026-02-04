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

#include <vector>

#include "src/shared/schema/utils.h"

namespace px {
namespace {

table_store::schema::Relation InfoClassProtoToRelation(
    const stirling::stirlingpb::InfoClass& info_class_pb) {
  table_store::schema::Relation relation;
  for (const auto& element : info_class_pb.schema().elements()) {
    relation.AddColumn(element.type(), element.name(), element.stype(), element.ptype(),
                       element.desc());
  }
  return relation;
}

RelationInfo ConvertInfoClassPBToRelationInfo(
    const stirling::stirlingpb::InfoClass& info_class_pb) {
  if (info_class_pb.schema().tabletized()) {
    return RelationInfo(info_class_pb.schema().name(), info_class_pb.id(),
                        info_class_pb.schema().desc(), info_class_pb.schema().tabletization_key(),
                        InfoClassProtoToRelation(info_class_pb));
  }
  return RelationInfo(info_class_pb.schema().name(), info_class_pb.id(),
                      info_class_pb.schema().desc(), InfoClassProtoToRelation(info_class_pb));
}

}  // namespace

std::vector<RelationInfo> ConvertPublishPBToRelationInfo(const stirling::stirlingpb::Publish& pb) {
  std::vector<RelationInfo> relation_info_vec;
  relation_info_vec.reserve(pb.published_info_classes_size());
  for (const auto& info_class : pb.published_info_classes()) {
    relation_info_vec.emplace_back(ConvertInfoClassPBToRelationInfo(info_class));
  }
  return relation_info_vec;
}

}  // namespace px
