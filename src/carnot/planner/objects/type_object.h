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

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_join.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief NoneObject represents None in python, the "null" object. This is used as a proxy for void
 * return type in Python interpretation.
 *
 */
class TypeObject : public QLObject {
 public:
  static constexpr TypeDescriptor TypeObjectType = {
      /* name */ "Type",
      /* type */ QLObjectType::kType,
  };

  static StatusOr<std::shared_ptr<TypeObject>> Create(IRNodeType node_type,
                                                      ASTVisitor* ast_visitor) {
    PX_ASSIGN_OR_RETURN(types::DataType data_type, IRNodeTypeToDataType(node_type));
    return Create(data_type, ast_visitor);
  }
  static StatusOr<std::shared_ptr<TypeObject>> Create(types::DataType data_type,
                                                      ASTVisitor* ast_visitor) {
    return Create(data_type, types::ST_NONE, ast_visitor);
  }

  static StatusOr<std::shared_ptr<TypeObject>> Create(QLObjectType ql_object_type,
                                                      ASTVisitor* ast_visitor) {
    auto type = std::shared_ptr<TypeObject>(
        new TypeObject(types::DATA_TYPE_UNKNOWN, types::ST_NONE, ql_object_type, ast_visitor));
    PX_RETURN_IF_ERROR(type->Init());
    return type;
  }

  static StatusOr<std::shared_ptr<TypeObject>> Create(types::DataType data_type,
                                                      types::SemanticType semantic_type,
                                                      ASTVisitor* ast_visitor) {
    auto type = std::shared_ptr<TypeObject>(
        new TypeObject(data_type, semantic_type, QLObjectType::kExpr, ast_visitor));
    PX_RETURN_IF_ERROR(type->Init());
    return type;
  }

  Status Init();

  Status NodeMatches(ExpressionIR* node) {
    // TODO(philkuz) make this nvi and expand it more.
    // TODO(philkuz) need to consider how semantic args should work in this case. Might need to add
    // specification to make args semantic args somehow.
    if (node->EvaluatedDataType() != data_type_) {
      return node->CreateIRNodeError(
          "Expected '$0', received '$1'", absl::AsciiStrToLower(types::ToString(data_type_)),
          absl::AsciiStrToLower(types::ToString(node->EvaluatedDataType())));
    }
    return Status::OK();
  }

  bool ObjectMatches(const QLObjectPtr& ql_object) const {
    if (ql_object->type() != ql_object_type_) {
      return false;
    }
    // Only evaluate DataType/SemanticType if the object type is an expression.
    if (ql_object_type_ != QLObjectType::kExpr) {
      return true;
    }
    auto expr_object = static_cast<ExprObject*>(ql_object.get());
    auto expr_ir = expr_object->expr();

    if (expr_ir->EvaluatedDataType() != data_type_) {
      return false;
    }

    if (!expr_ir->HasTypeCast()) {
      return semantic_type_ == types::ST_NONE;
    }
    return expr_ir->type_cast()->semantic_type() == semantic_type_;
  }

  types::DataType data_type() const { return data_type_; }
  types::SemanticType semantic_type() const { return semantic_type_; }
  QLObjectType ql_object_type() const { return ql_object_type_; }
  std::string TypeString() {
    if (ql_object_type_ != QLObjectType::kExpr) {
      return absl::AsciiStrToLower(magic_enum::enum_name(ql_object_type_));
    }
    if (semantic_type_ == types::ST_NONE) {
      return absl::AsciiStrToLower(types::ToString(data_type_));
    }
    return absl::AsciiStrToLower(types::ToString(semantic_type_));
  }

 protected:
  /**
   * @brief Construct a Type object that represents the null value in Python.
   *
   * @param ast the ast ptr for the
   */
  TypeObject(types::DataType data_type, types::SemanticType semantic_type,
             QLObjectType ql_object_type, ASTVisitor* ast_visitor)
      : QLObject(TypeObjectType, ast_visitor),
        data_type_(data_type),
        semantic_type_(semantic_type),
        ql_object_type_(ql_object_type) {}

 private:
  types::DataType data_type_;
  types::SemanticType semantic_type_;
  QLObjectType ql_object_type_;
};

class ParsedArgs;

/**
 * @brief Implements the cast logic
 */
class CastHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(types::DataType data_type, types::SemanticType semantic_type,
                                    const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor);
};
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
