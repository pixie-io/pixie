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

#include "src/carnot/planner/ir/ast_utils.h"

namespace px {
namespace carnot {
namespace planner {

std::string GetAstTypeName(pypa::AstType type) {
  std::vector<std::string> type_names = {
#undef PYPA_AST_TYPE
#define PYPA_AST_TYPE(X) #X,
// NOLINTNEXTLINE(build/include_order).
#include <pypa/ast/ast_type.inl>
#undef PYPA_AST_TYPE
  };
  DCHECK(type_names.size() > static_cast<size_t>(type));
  return absl::Substitute("$0", type_names[static_cast<int>(type)]);
}

std::string GetNameAsString(const pypa::AstPtr& node) { return PYPA_PTR_CAST(Name, node)->id; }

StatusOr<std::string> GetStrAstValue(const pypa::AstPtr& ast) {
  if (ast->type != pypa::AstType::Str) {
    return CreateAstError(ast, "Expected string type. Got $0", GetAstTypeName(ast->type));
  }
  return PYPA_PTR_CAST(Str, ast)->value;
}

Status WrapAstError(const pypa::AstPtr& ast, Status status) {
  if (status.ok() || status.has_context()) {
    return status;
  }

  return CreateAstError(ast, status.msg());
}

StatusOr<std::string> AstToString(const std::shared_ptr<pypa::Ast>& ast) {
  switch (ast->type) {
    case pypa::AstType::ExpressionStatement: {
      return AstToString(PYPA_PTR_CAST(ExpressionStatement, ast)->expr);
    }
    case pypa::AstType::Assign: {
      auto assign = PYPA_PTR_CAST(Assign, ast);
      std::string assign_str;
      for (const auto& target : assign->targets) {
        PX_ASSIGN_OR_RETURN(auto target_str, AstToString(target));
        assign_str += target_str + ", ";
      }
      assign_str.pop_back();
      assign_str.pop_back();
      PX_ASSIGN_OR_RETURN(auto value_str, AstToString(assign->value));
      return absl::Substitute("$0 = $1", assign_str, value_str);
    }
    case pypa::AstType::Return: {
      auto ret = PYPA_PTR_CAST(Return, ast);
      PX_ASSIGN_OR_RETURN(auto value_str, AstToString(ret->value));
      return absl::Substitute("return $0", value_str);
    }
    case pypa::AstType::Attribute: {
      auto attr = PYPA_PTR_CAST(Attribute, ast);
      PX_ASSIGN_OR_RETURN(auto value, AstToString(attr->value));
      PX_ASSIGN_OR_RETURN(auto attribute, AstToString(attr->attribute));
      return absl::Substitute("$0.$1", value, attribute);
    }
    case pypa::AstType::BinOp: {
      auto binop = PYPA_PTR_CAST(BinOp, ast);
      PX_ASSIGN_OR_RETURN(auto left, AstToString(binop->left));
      PX_ASSIGN_OR_RETURN(auto right, AstToString(binop->right));
      return absl::Substitute("$0 $1 $2", left, pypa::to_string(binop->op), right);
    }
    case pypa::AstType::BoolOp: {
      auto boolop = PYPA_PTR_CAST(BoolOp, ast);
      if (boolop->values.size() != 2) {
        return CreateAstError(ast, "Expected two arguments to '$0'.", pypa::to_string(boolop->op));
      }
      PX_ASSIGN_OR_RETURN(auto left, AstToString(boolop->values[0]));
      PX_ASSIGN_OR_RETURN(auto right, AstToString(boolop->values[1]));
      return absl::Substitute("$0 $1 $2", left, pypa::to_string(boolop->op), right);
    }
    case pypa::AstType::Call: {
      auto call = PYPA_PTR_CAST(Call, ast);
      PX_ASSIGN_OR_RETURN(auto function, AstToString(call->function));
      std::vector<std::string> args(call->arguments.size());
      for (const auto& [i, arg] : Enumerate(call->arguments)) {
        PX_ASSIGN_OR_RETURN(args[i], AstToString(arg));
      }
      for (const auto& kwarg : call->keywords) {
        PX_ASSIGN_OR_RETURN(auto value, AstToString(kwarg->value));
        PX_ASSIGN_OR_RETURN(auto name, AstToString(kwarg->name));
        args.push_back(absl::Substitute("$0=$1", name, value));
      }
      return absl::StrCat(function, "(", absl::StrJoin(args, ", "), ")");
    }
    case pypa::AstType::Dict: {
      auto dict = PYPA_PTR_CAST(Dict, ast);
      std::vector<std::string> args(dict->keys.size());
      for (const auto& [i, key] : Enumerate(dict->keys)) {
        PX_ASSIGN_OR_RETURN(auto k, AstToString(key));
        PX_ASSIGN_OR_RETURN(auto v, AstToString(dict->values[i]));
        args[i] = absl::Substitute("$0: $1", k, v);
      }
      return absl::StrCat("{", absl::StrJoin(args, ", "), "}");
    }
    case pypa::AstType::List: {
      auto list = PYPA_PTR_CAST(List, ast);
      std::vector<std::string> elms(list->elements.size());
      for (const auto& [i, elm] : Enumerate(list->elements)) {
        PX_ASSIGN_OR_RETURN(elms[i], AstToString(elm));
      }
      return absl::StrCat("[", absl::StrJoin(elms, ", "), "]");
    }
    case pypa::AstType::Name: {
      return PYPA_PTR_CAST(Name, ast)->id;
    }
    case pypa::AstType::Number: {
      auto num = PYPA_PTR_CAST(Number, ast);
      switch (num->num_type) {
        case pypa::AstNumber::Type::Float: {
          return absl::Substitute("$0", num->floating);
        }
        case pypa::AstNumber::Type::Integer:
        case pypa::AstNumber::Type::Long: {
          return absl::Substitute("$0", num->integer);
        }
      }
      break;
    }
    case pypa::AstType::Str: {
      auto str = PYPA_PTR_CAST(Str, ast);
      return absl::Substitute("'$0'", str->value);
    }
    case pypa::AstType::Subscript: {
      auto subscript = PYPA_PTR_CAST(Subscript, ast);
      PX_ASSIGN_OR_RETURN(auto value, AstToString(subscript->value));
      if (subscript->slice->type != pypa::AstType::Index) {
        return CreateAstError(ast, "Only index slices are supported");
      }
      PX_ASSIGN_OR_RETURN(auto index, AstToString(PYPA_PTR_CAST(Index, subscript->slice)->value));
      return absl::StrCat(value, "[", index, "]");
    }
    case pypa::AstType::Tuple: {
      auto list = PYPA_PTR_CAST(Tuple, ast);
      std::vector<std::string> elms(list->elements.size());
      for (const auto& [i, elm] : Enumerate(list->elements)) {
        PX_ASSIGN_OR_RETURN(elms[i], AstToString(elm));
      }
      return absl::StrCat("(", absl::StrJoin(elms, ", "), ")");
    }
    case pypa::AstType::UnaryOp: {
      auto unaryop = PYPA_PTR_CAST(UnaryOp, ast);
      PX_ASSIGN_OR_RETURN(auto operand, AstToString(unaryop->operand));
      return absl::Substitute("$0$1", pypa::to_string(unaryop->op), operand);
    }
    default:
      break;
  }
  return CreateAstError(ast, "Can't stringify AST, unsupported AST type: $0",
                        GetAstTypeName(ast->type));
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
