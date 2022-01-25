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

#include <iostream>

#include "src/shared/schema/utils.h"
#include "src/stirling/stirling.h"
#include "src/table_store/schema/schema.h"

int main() {
  auto source_registry = px::stirling::CreateProdSourceRegistry();
  auto sources = source_registry->sources();

  absl::flat_hash_map<std::string, px::table_store::schema::Relation> rel_map;
  for (const auto& reg_element : sources) {
    for (auto schema : reg_element.schema) {
      px::table_store::schema::Relation relation;
      for (const auto& element : schema.elements()) {
        relation.AddColumn(element.type(), std::string(element.name()), element.stype(),
                           element.desc());
      }
      rel_map[schema.name()] = relation;
    }
  }
  px::table_store::schemapb::Schema schema_pb;
  PL_CHECK_OK(px::table_store::schema::Schema::ToProto(&schema_pb, rel_map));
  schema_pb.SerializeToOstream(&std::cout);
  return 0;
}
