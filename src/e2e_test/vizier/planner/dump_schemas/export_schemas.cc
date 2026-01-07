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

#include <fstream>
#include <iostream>
#include <string>

#include "src/stirling/stirling.h"
#include "src/table_store/schema/schema.h"

DEFINE_string(out_file_path, "schema.pb", "The file to save the serialized Schema");

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

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
  PX_CHECK_OK(px::table_store::schema::Schema::ToProto(&schema_pb, rel_map));

  std::ofstream out_schema;
  out_schema.open(FLAGS_out_file_path);
  schema_pb.SerializeToOstream(&out_schema);
  out_schema.close();

  return 0;
}
