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
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/carnot/planner/ir/metadata_ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"

namespace px {
namespace carnot {
namespace planner {

using ::px::shared::metadatapb::MetadataType;

class NameMetadataProperty : public MetadataProperty {
 public:
  explicit NameMetadataProperty(MetadataType metadata_type, std::vector<MetadataType> key_columns)
      : MetadataProperty(metadata_type, types::DataType::STRING, key_columns) {}
  // Expect format to be "<namespace>/<value>"
  bool ExprFitsFormat(ExpressionIR* ir_node) const override {
    DCHECK(ir_node->type() == IRNodeType::kString);
    std::string value = static_cast<StringIR*>(ir_node)->str();
    std::vector<std::string> split_str = absl::StrSplit(value, "/");
    return split_str.size() == 2;
  }
  std::string ExplainFormat() const override { return "String with format <namespace>/<name>."; }
};

class IdMetadataProperty : public MetadataProperty {
 public:
  explicit IdMetadataProperty(MetadataType metadata_type, std::vector<MetadataType> key_columns)
      : MetadataProperty(metadata_type, types::DataType::STRING, key_columns) {}
  // TODO(philkuz) udate this fits format when we have a better idea what the format should be.
  // ExprFitsFormat always evaluates to true because id format is not yet defined.
  bool ExprFitsFormat(ExpressionIR*) const override { return true; }
  std::string ExplainFormat() const override { return ""; }
};

class Int64MetadataProperty : public MetadataProperty {
 public:
  explicit Int64MetadataProperty(MetadataType metadata_type, std::vector<MetadataType> key_columns)
      : MetadataProperty(metadata_type, types::DataType::INT64, key_columns) {}
  bool ExprFitsFormat(ExpressionIR* expr) const override { return Match(expr, Int()); }
  std::string ExplainFormat() const override { return ""; }
};

class MetadataHandler {
 public:
  StatusOr<MetadataProperty*> GetProperty(const std::string& md_name) const;
  bool HasProperty(const std::string& md_name) const;
  static std::unique_ptr<MetadataHandler> Create();

 private:
  MetadataHandler() {}
  MetadataProperty* AddProperty(std::unique_ptr<MetadataProperty> md_property);
  void AddMapping(const std::string& name, MetadataProperty* property);
  template <typename Property>
  void AddObject(MetadataType md_type, const std::vector<std::string>& aliases,
                 const std::vector<MetadataType>& key_columns);

  std::vector<std::unique_ptr<MetadataProperty>> property_pool;
  std::unordered_map<std::string, MetadataProperty*> metadata_map;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
