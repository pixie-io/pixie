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
#include <string>
#include <utility>

#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_join.h>

#include "src/common/base/base.h"
#include "src/shared/types/type_utils.h"
#include "src/table_store/schema/relation.h"

namespace px {
namespace table_store {
namespace schema {

using std::string;

Relation::Relation() = default;

Relation::Relation(ColTypeArray col_types, ColNameArray col_names)
    : Relation(col_types, col_names, ColDescArray(col_types.size(), "")) {}

Relation::Relation(ColTypeArray col_types, ColNameArray col_names, ColDescArray col_desc)
    : Relation(col_types, col_names, col_desc,
               ColSemanticTypeArray(col_types.size(), types::ST_NONE)) {}

Relation::Relation(ColTypeArray col_types, ColNameArray col_names,
                   ColSemanticTypeArray col_semantic_types)
    : Relation(col_types, col_names, ColDescArray(col_types.size(), ""), col_semantic_types) {}

Relation::Relation(ColTypeArray col_types, ColNameArray col_names, ColDescArray col_desc,
                   ColSemanticTypeArray col_semantic_types)
    : Relation(col_types, col_names, col_desc, col_semantic_types,
               ColPatternTypeArray(col_types.size(), types::UNSPECIFIED)) {}

Relation::Relation(ColTypeArray col_types, ColNameArray col_names, ColDescArray col_desc,
                   ColSemanticTypeArray col_semantic_types, ColPatternTypeArray col_pattern_types)
    : col_types_(std::move(col_types)),
      col_names_(std::move(col_names)),
      col_desc_(std::move(col_desc)),
      col_semantic_types_(std::move(col_semantic_types)),
      col_pattern_types_(std::move(col_pattern_types)) {
  CHECK(col_types_.size() == col_names_.size()) << "Initialized with mismatched col names/sizes";
  CHECK(col_names_.size() == col_desc_.size()) << "Initialized with mismatched col names/desc";
  CHECK(col_desc_.size() == col_semantic_types_.size())
      << "Initialized with mismatches col semantic types sizes";
  CHECK(col_desc_.size() == col_pattern_types_.size())
      << "Initialized with mismatches col pattern types sizes";
  absl::flat_hash_set<std::string> unique_names;
  for (const auto& col_name : col_names_) {
    DCHECK(!unique_names.contains(col_name))
        << absl::Substitute("Duplicate column name '$0' in relation", col_name);
    unique_names.insert(col_name);
  }
}

size_t Relation::NumColumns() const { return col_types_.size(); }

void Relation::AddColumn(const types::DataType& col_type, const std::string& col_name,
                         std::string_view col_desc) {
  AddColumn(col_type, col_name, types::SemanticType::ST_NONE, col_desc);
}

void Relation::AddColumn(const types::DataType& col_type, const std::string& col_name,
                         const types::SemanticType& col_semantic_type, std::string_view col_desc) {
  AddColumn(col_type, col_name, col_semantic_type, types::PatternType::UNSPECIFIED, col_desc);
}

void Relation::AddColumn(const types::DataType& col_type, const std::string& col_name,
                         const types::SemanticType& col_semantic_type,
                         const types::PatternType& col_pattern_type, std::string_view col_desc) {
  DCHECK(std::find(col_names_.begin(), col_names_.end(), col_name) == col_names_.end())
      << absl::Substitute("Column '$0' already exists", col_name);
  col_types_.push_back(col_type);
  col_names_.push_back(col_name);
  col_desc_.push_back(std::string(col_desc));
  col_semantic_types_.push_back(col_semantic_type);
  col_pattern_types_.push_back(col_pattern_type);
}

bool Relation::HasColumn(size_t idx) const { return idx < col_types_.size(); }
int64_t Relation::GetColumnIndex(const std::string& col_name) const {
  auto it = std::find(col_names_.begin(), col_names_.end(), col_name);
  if (it == col_names_.end()) {
    return -1;
  }
  auto col_idx = std::distance(col_names_.begin(), it);
  return col_idx;
}

bool Relation::HasColumn(const std::string& col_name) const {
  return HasColumn(GetColumnIndex(col_name));
}

types::DataType Relation::GetColumnType(size_t idx) const {
  CHECK(HasColumn(idx)) << "Column does not exist";
  return col_types_[idx];
}

types::DataType Relation::GetColumnType(const std::string& col_name) const {
  return GetColumnType(GetColumnIndex(col_name));
}

const std::string& Relation::GetColumnName(size_t idx) const {
  CHECK(HasColumn(idx)) << absl::Substitute("Column $0 does not exist. Only $1 columns available.",
                                            idx, NumColumns());
  return col_names_[idx];
}

const std::string& Relation::GetColumnDesc(size_t idx) const {
  CHECK(HasColumn(idx)) << absl::Substitute("Column $0 does not exist. Only $1 columns available.",
                                            idx, NumColumns());
  return col_desc_[idx];
}

const std::string& Relation::GetColumnDesc(const std::string& col_name) const {
  return GetColumnDesc(GetColumnIndex(col_name));
}

types::SemanticType Relation::GetColumnSemanticType(size_t idx) const {
  CHECK(HasColumn(idx)) << absl::Substitute("Column $0 does not exist. Only $1 columns available.",
                                            idx, NumColumns());
  return col_semantic_types_[idx];
}

types::SemanticType Relation::GetColumnSemanticType(const std::string& col_name) const {
  return GetColumnSemanticType(GetColumnIndex(col_name));
}

types::PatternType Relation::GetColumnPatternType(size_t idx) const {
  CHECK(HasColumn(idx)) << absl::Substitute("Column $0 does not exist. Only $1 columns available.",
                                            idx, NumColumns());
  return col_pattern_types_[idx];
}

types::PatternType Relation::GetColumnPatternType(const std::string& col_name) const {
  return GetColumnPatternType(GetColumnIndex(col_name));
}

std::string Relation::DebugString() const {
  CHECK(col_types_.size() == col_names_.size()) << "Mismatched col names/sizes";
  std::vector<string> col_info_as_str;
  for (size_t i = 0; i < col_types_.size(); ++i) {
    col_info_as_str.push_back(absl::StrCat(col_names_[i], ":", types::ToString(col_types_[i])));
  }
  return "[" + absl::StrJoin(col_info_as_str, ", ") + "]";
}

Status Relation::ToProto(table_store::schemapb::Relation* relation_proto) const {
  CHECK(relation_proto != nullptr);
  size_t num_columns = NumColumns();
  for (size_t col_idx = 0; col_idx < num_columns; ++col_idx) {
    auto col_pb = relation_proto->add_columns();
    col_pb->set_column_type(GetColumnType(col_idx));
    col_pb->set_column_name(GetColumnName(col_idx));
    col_pb->set_column_semantic_type(GetColumnSemanticType(col_idx));
  }
  return Status::OK();
}
Status Relation::FromProto(const table_store::schemapb::Relation* relation_pb) {
  if (NumColumns() != 0) {
    return error::AlreadyExists("Relation already has $0 columns. Can't init from proto.",
                                NumColumns());
  }
  for (int idx = 0; idx < relation_pb->columns_size(); ++idx) {
    auto column = relation_pb->columns(idx);
    AddColumn(column.column_type(), column.column_name(), column.column_semantic_type());
  }
  return Status::OK();
}

}  // namespace schema
}  // namespace table_store
}  // namespace px
