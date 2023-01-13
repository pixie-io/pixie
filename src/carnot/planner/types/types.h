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

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/substitute.h>

#include "src/common/base/statusor.h"
#include "src/shared/types/type_utils.h"
#include "src/shared/types/types.h"
#include "src/table_store/schema/relation.h"

namespace px {
namespace carnot {
namespace planner {

using px::table_store::schema::Relation;
using px::types::DataType;
using px::types::SemanticType;

class BaseType;
using TypePtr = std::shared_ptr<BaseType>;

class BaseType {
 public:
  virtual ~BaseType() {}
  virtual TypePtr Copy() const = 0;
  virtual std::string DebugString() const = 0;
  virtual bool IsValueType() const { return false; }
  virtual bool IsTableType() const { return false; }
  virtual bool Equals(TypePtr) const = 0;
};

class ValueType;
using ValueTypePtr = std::shared_ptr<ValueType>;

class ValueType : public BaseType {
  /**
   * @brief ValueType is the most basic type. It stores the primitive data type and the semantic
   * type.
   */
 public:
  DataType data_type() const { return data_type_; }
  SemanticType semantic_type() const { return semantic_type_; }

  TypePtr Copy() const override { return ValueType::Create(data_type_, semantic_type_); }

  bool operator==(const ValueType& other) const {
    return (data_type_ == other.data_type()) && (semantic_type_ == other.semantic_type());
  }
  bool operator!=(const ValueType& other) const { return !(*this == other); }

  std::string DebugString() const override {
    return absl::Substitute("ValueType($0, $1)", types::ToString(data_type_),
                            types::ToString(semantic_type_));
  }

  static ValueTypePtr Create(DataType data_type, SemanticType semantic_type) {
    return ValueTypePtr(new ValueType(data_type, semantic_type));
  }

  friend std::ostream& operator<<(std::ostream& os, const ValueType& val) {
    os << val.DebugString();
    return os;
  }

  bool IsValueType() const override { return true; }
  bool Equals(TypePtr other_type) const override {
    if (!other_type->IsValueType()) {
      return false;
    }
    auto other = std::static_pointer_cast<ValueType>(other_type);
    return *this == *other;
  }

 protected:
  explicit ValueType(DataType data_type, SemanticType semantic_type)
      : data_type_(data_type), semantic_type_(semantic_type) {}

 private:
  DataType data_type_;
  SemanticType semantic_type_;
};

class TableType : public BaseType {
  /**
   * @brief TableType stores column data types, mapping column names to there type.
   *
   * Currently, all Operators have a TableType and all expressions have a ValueType, but with the
   * data model changes we might want to extend the type system to make tags data there own type
   * structure.
   */
 public:
  static std::shared_ptr<TableType> Create() { return std::shared_ptr<TableType>(new TableType); }

  static std::shared_ptr<TableType> Create(Relation rel) {
    return std::shared_ptr<TableType>(new TableType(rel));
  }

  void AddColumn(std::string col_name, TypePtr type_) {
    DCHECK_EQ(0U, map_.count(col_name)) << absl::Substitute(
        "Cannot AddColumn '$0'. Column already exists in type: $1", col_name, DebugString());
    map_.insert({col_name, type_});
    ordered_col_names_.push_back(col_name);
  }
  bool HasColumn(std::string col_name) const { return map_.find(col_name) != map_.end(); }
  bool RemoveColumn(std::string col_name) {
    auto col_to_remove = map_.find(col_name);
    if (col_to_remove == map_.end()) {
      return false;
    }
    map_.erase(col_to_remove);
    auto it = std::find(ordered_col_names_.begin(), ordered_col_names_.end(), col_name);
    ordered_col_names_.erase(it);
    return true;
  }

  bool RenameColumn(std::string old_col_name, std::string new_col_name) {
    DCHECK_NE(old_col_name, new_col_name);
    auto it = map_.find(old_col_name);
    if (it == map_.end()) {
      return false;
    }
    map_.insert({new_col_name, it->second});
    map_.erase(old_col_name);
    auto col_name_it =
        std::find(ordered_col_names_.begin(), ordered_col_names_.end(), old_col_name);
    *col_name_it = new_col_name;
    return true;
  }

  StatusOr<TypePtr> GetColumnType(std::string col_name) const {
    auto it = map_.find(col_name);
    if (it == map_.end()) {
      return Status(statuspb::INVALID_ARGUMENT,
                    absl::Substitute("cannot find column $0 in table type container", col_name));
    }
    return it->second;
  }

  TypePtr Copy() const override {
    auto copy = TableType::Create();
    for (const auto& [name, type] : *this) {
      copy->AddColumn(name, type);
    }
    return copy;
  }

  StatusOr<Relation> ToRelation() const {
    Relation r;
    for (const auto& [name, type] : *this) {
      if (!type->IsValueType()) {
        return error::Internal(
            "Can not convert TableType with non-ValueType columns into a relation");
      }
      auto val = std::static_pointer_cast<ValueType>(type);
      r.AddColumn(val->data_type(), name, val->semantic_type());
    }
    return r;
  }

  std::string DebugString() const override {
    std::vector<std::string> col_debug_strings;
    for (const auto& [name, type] : *this) {
      col_debug_strings.push_back(absl::Substitute("$0: $1", name, type->DebugString()));
    }
    return "TableType(" + absl::StrJoin(col_debug_strings, " | ") + ")";
  }

  const std::vector<std::string>& ColumnNames() const { return ordered_col_names_; }
  int64_t GetColumnIndex(std::string name) const {
    auto it = std::find(ordered_col_names_.begin(), ordered_col_names_.end(), name);
    if (it == ordered_col_names_.end()) {
      return -1;
    }
    return it - ordered_col_names_.begin();
  }

  bool IsTableType() const override { return true; }
  bool Equals(TypePtr other_type) const override {
    if (!other_type->IsTableType()) {
      return false;
    }
    auto other = std::static_pointer_cast<TableType>(other_type);
    if (ordered_col_names_ != other->ordered_col_names_) {
      return false;
    }
    if (map_.size() != other->map_.size()) {
      return false;
    }
    for (const auto& [name, type] : map_) {
      if (other->map_.count(name) == 0) {
        return false;
      }
      if (!type->Equals(other->map_[name])) {
        return false;
      }
    }
    return true;
  }

  class TableTypeIterator {
   public:
    using value_type = std::pair<const std::string, std::shared_ptr<BaseType>>;
    using difference_type = std::ptrdiff_t;
    using pointer = const std::pair<const std::string, std::shared_ptr<BaseType>>*;
    using reference = const std::pair<const std::string, std::shared_ptr<BaseType>>&;
    using iterator_category = std::input_iterator_tag;
    using vector_iterator = std::vector<std::string>::const_iterator;

    TableTypeIterator(const std::map<std::string, std::shared_ptr<BaseType>>& table_map,
                      vector_iterator curr)
        : table_map_(table_map), curr_(curr) {}

    TableTypeIterator& operator++() {
      ++curr_;
      return *this;
    }
    bool operator==(const TableTypeIterator& it) { return it.curr_ == curr_; }
    bool operator!=(const TableTypeIterator& it) { return it.curr_ != curr_; }

    reference operator*() const {
      auto it = table_map_.find(*curr_);
      return *it;
    }

    pointer operator->() const {
      auto it = table_map_.find(*curr_);
      return &(*it);
    }

   private:
    const std::map<std::string, std::shared_ptr<BaseType>>& table_map_;
    vector_iterator curr_;
  };
  using const_iterator = TableTypeIterator;
  const_iterator begin() const { return TableTypeIterator(map_, ordered_col_names_.begin()); }
  const_iterator end() const { return TableTypeIterator(map_, ordered_col_names_.end()); }

 protected:
  TableType() {}
  explicit TableType(const Relation& rel) {
    for (size_t i = 0; i < rel.NumColumns(); i++) {
      AddColumn(rel.col_names()[i],
                ValueType::Create(rel.col_types()[i], rel.col_semantic_types()[i]));
    }
  }

 private:
  std::map<std::string, std::shared_ptr<BaseType>> map_;
  std::vector<std::string> ordered_col_names_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
