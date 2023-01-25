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

#include "src/carnot/planner/objects/funcobject.h"

#include <memory>
#include <utility>
#include "src/carnot/planner/objects/type_object.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<FuncObject>> FuncObject::Create(
    std::string_view name, const std::vector<std::string>& arguments,
    const absl::flat_hash_map<std::string, DefaultType>& defaults, bool has_variable_len_args,
    bool has_variable_len_kwargs, FunctionType impl, ASTVisitor* ast_visitor) {
  return std::make_shared<FuncObject>(name, arguments, defaults, has_variable_len_args,
                                      has_variable_len_kwargs, impl, ast_visitor);
}

FuncObject::FuncObject(std::string_view name, const std::vector<std::string>& arguments,
                       const absl::flat_hash_map<std::string, DefaultType>& defaults,
                       bool has_variable_len_args, bool has_variable_len_kwargs, FunctionType impl,
                       ASTVisitor* ast_visitor)
    : QLObject(FuncType, ast_visitor),
      name_(name),
      arguments_(arguments),
      defaults_(defaults),
      impl_(impl),
      has_variable_len_kwargs_(has_variable_len_kwargs),
      has_variable_len_args_(has_variable_len_args) {
#if DCHECK_IS_ON()
  for (const auto& arg : defaults) {
    DCHECK(std::find(arguments.begin(), arguments.end(), arg.first) != arguments.end())
        << absl::Substitute("Default argument '$0' does not match an actual argument.", arg.first);
  }
#endif
}

StatusOr<QLObjectPtr> FuncObject::Call(const ArgMap& args, const pypa::AstPtr& ast) {
  PX_ASSIGN_OR_RETURN(ParsedArgs parsed_args, PrepareArgs(args, ast));
  return impl_(ast, parsed_args, ast_visitor());
}

StatusOr<ParsedArgs> FuncObject::PrepareArgs(const ArgMap& args, const pypa::AstPtr& ast) {
  // Iterate through the arguments and place them in.
  ParsedArgs parsed_args;

  // If the number of args is greater than all args, then we throw an error.
  int64_t input_nargs = static_cast<int64_t>(args.args.size());
  if (!has_variable_len_args_ && input_nargs > NumArgs()) {
    return CreateAstError(ast, "$0() takes $1 arguments but $2 were given.", name(), NumArgs(),
                          args.args.size());
  }

  for (const auto& [idx, node] : Enumerate(args.args)) {
    // Once we've exceeded the number of arguments, we add to variable arguments.
    // The error check above makes sure that this only happens if the Function can take variable
    // length arguments.
    if (idx >= arguments_.size()) {
      parsed_args.AddVariableArg(node);
      continue;
    }
    std::string arg_name = arguments_[idx];
    parsed_args.AddArg(arg_name, node);
  }

  absl::flat_hash_set<std::string> missing_args;

  if (input_nargs < NumArgs()) {
    missing_args =
        absl::flat_hash_set<std::string>(arguments_.begin() + input_nargs, arguments_.end());
  }
  // Parse through the keyword args.
  for (const auto& [arg, node] : args.kwargs) {
    if (parsed_args.HasArgOrKwarg(arg)) {
      std::string err_msg =
          absl::Substitute("$0() got multiple values for argument '$1'", name(), arg);
      return CreateAstError(ast, err_msg);
    }
    if (!missing_args.contains(arg)) {
      if (!has_variable_len_kwargs_) {
        std::string err_msg =
            absl::Substitute("$0() got an unexpected keyword argument '$1'", name(), arg);
        return CreateAstError(ast, err_msg);
      }
      parsed_args.AddKwarg(arg, node);
    } else {
      parsed_args.AddArg(arg, node);
      missing_args.erase(arg);
    }
  }

  absl::flat_hash_set<std::string> missing_pos_args;
  for (const std::string& arg : missing_args) {
    if (!HasDefault(arg)) {
      missing_pos_args.emplace(arg);
      continue;
    }
    PX_ASSIGN_OR_RETURN(auto default_node, GetDefault(arg));
    default_node->SetAst(ast);
    parsed_args.SubDefaultArg(arg, default_node);
  }

  // If missing positional arguments.
  if (missing_pos_args.size()) {
    std::string err_msg =
        absl::Substitute("$0() missing $1 required positional arguments $2", name(),
                         missing_pos_args.size(), FormatArguments(missing_pos_args));
    return CreateAstError(ast, err_msg);
  }
  return parsed_args;
}

bool FuncObject::HasDefault(std::string_view arg) const {
  return defaults_.find(arg) != defaults_.end();
}

StatusOr<QLObjectPtr> FuncObject::GetDefault(std::string_view arg) {
  //  Check if the argument exists among the defaults.
  if (!defaults_.contains(arg)) {
    return error::InvalidArgument("");
  }
  return ast_visitor()->ParseAndProcessSingleExpression(defaults_.find(arg)->second,
                                                        /*import_px*/ true);
}

std::string FuncObject::FormatArguments(const absl::flat_hash_set<std::string>& args) {
  // Joins the argument names by commas and surrounds each arg with single quotes.
  return absl::StrJoin(args, ",", [](std::string* out, const std::string& arg) {
    absl::StrAppend(out, absl::Substitute("'$0'", arg));
  });
}

Status FuncObject::AddVisSpec(std::unique_ptr<VisSpec> vis_spec) {
  vis_spec_ = std::move(vis_spec);
  return Status::OK();
}

StatusOr<std::shared_ptr<FuncObject>> GetCallMethod(const pypa::AstPtr& ast,
                                                    const QLObjectPtr& pyobject) {
  std::shared_ptr<FuncObject> func_object;
  if (pyobject->type_descriptor().type() == QLObjectType::kFunction) {
    pyobject->SetAst(ast);
    return std::static_pointer_cast<FuncObject>(pyobject);
  }
  PX_ASSIGN_OR_RETURN(func_object, pyobject->GetCallMethod());
  func_object->SetAst(ast);
  return func_object;
}

Status FuncObject::ResolveArgAnnotationsToTypes(
    const absl::flat_hash_map<std::string, QLObjectPtr>& arg_annotation_objs) {
  for (const auto& [name, obj] : arg_annotation_objs) {
    if (obj->type() != QLObjectType::kType) {
      PX_ASSIGN_OR_RETURN(auto type_obj,
                          TypeObject::Create(types::DataType::DATA_TYPE_UNKNOWN,
                                             types::SemanticType::ST_UNSPECIFIED, ast_visitor()));
      arg_types_.insert({name, type_obj});
    } else {
      auto type_obj = std::static_pointer_cast<TypeObject>(obj);
      arg_types_.insert({name, type_obj});
    }
  }
  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
