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

#include "src/carnot/planner/compiler/ast_visitor.h"

#include <utility>

#include "src/carnot/planner/compiler_error_context/compiler_error_context.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/module.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/objects/type_object.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/probes/config_module.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
using pypa::AstType;

StatusOr<FuncIR::Op> ASTVisitorImpl::GetOp(const std::string& python_op, const pypa::AstPtr node) {
  auto op_find = FuncIR::op_map.find(python_op);
  if (op_find == FuncIR::op_map.end()) {
    return CreateAstError(node, "Operator '$0' not handled", python_op);
  }
  return op_find->second;
}

StatusOr<FuncIR::Op> ASTVisitorImpl::GetUnaryOp(const std::string& python_op,
                                                const pypa::AstPtr node) {
  auto op_find = FuncIR::unary_op_map.find(python_op);
  if (op_find == FuncIR::op_map.end()) {
    return CreateAstError(node, "Unary Operator '$0' not handled", python_op);
  }
  return op_find->second;
}

StatusOr<std::shared_ptr<ASTVisitorImpl>> ASTVisitorImpl::Create(
    IR* graph, std::shared_ptr<VarTable> var_table, MutationsIR* mutations,
    CompilerState* compiler_state, ModuleHandler* module_handler, bool func_based_exec,
    const absl::flat_hash_set<std::string>& reserved_names,
    const absl::flat_hash_map<std::string, std::string>& module_map) {
  std::shared_ptr<ASTVisitorImpl> ast_visitor = std::shared_ptr<ASTVisitorImpl>(new ASTVisitorImpl(
      graph, mutations, compiler_state, VarTable::Create(), std::move(var_table), func_based_exec,
      reserved_names, module_handler, std::make_shared<udf::Registry>("udcf")));

  PX_RETURN_IF_ERROR(ast_visitor->InitGlobals());
  PX_RETURN_IF_ERROR(ast_visitor->SetupModules(module_map));
  builtins::RegisterMathOpsOrDie(ast_visitor->udf_registry_.get());
  return ast_visitor;
}

StatusOr<std::shared_ptr<ASTVisitorImpl>> ASTVisitorImpl::Create(
    IR* graph, MutationsIR* mutations, CompilerState* compiler_state, ModuleHandler* module_handler,
    bool func_based_exec, const absl::flat_hash_set<std::string>& reserved_names,
    const absl::flat_hash_map<std::string, std::string>& module_map) {
  return Create(graph, VarTable::Create(), mutations, compiler_state, module_handler,
                func_based_exec, reserved_names, module_map);
}

std::shared_ptr<ASTVisitorImpl> ASTVisitorImpl::CreateChild() {
  return CreateChildImpl(var_table_->CreateChild());
}

std::shared_ptr<ASTVisitor> ASTVisitorImpl::CreateModuleVisitor(
    std::shared_ptr<VarTable> var_table) {
  return std::static_pointer_cast<ASTVisitor>(CreateChildImpl(var_table));
}

std::shared_ptr<ASTVisitorImpl> ASTVisitorImpl::CreateChildImpl(
    std::shared_ptr<VarTable> var_table) {
  // The flag values should come from the parent var table, not be copied here.
  auto visitor = std::shared_ptr<ASTVisitorImpl>(
      new ASTVisitorImpl(ir_graph_, mutations_, compiler_state_, global_var_table_, var_table,
                         func_based_exec_, {}, module_handler_, udf_registry_));
  return visitor;
}

Status ASTVisitorImpl::SetupModules(
    const absl::flat_hash_map<std::string, std::string>& module_name_to_pxl_map) {
  DCHECK(module_handler_);
  PX_ASSIGN_OR_RETURN(
      (*module_handler_)[PixieModule::kPixieModuleObjName],
      PixieModule::Create(ir_graph_, compiler_state_, this, func_based_exec_, reserved_names_));
  PX_ASSIGN_OR_RETURN((*module_handler_)[TraceModule::kTraceModuleObjName],
                      TraceModule::Create(mutations_, this));
  PX_ASSIGN_OR_RETURN((*module_handler_)[ConfigModule::kConfigModuleObjName],
                      ConfigModule::Create(mutations_, this));
  for (const auto& [module_name, module_text] : module_name_to_pxl_map) {
    PX_ASSIGN_OR_RETURN((*module_handler_)[module_name], Module::Create(module_text, this));
  }
  return Status::OK();
}

Status ASTVisitorImpl::InitGlobals() {
  // Populate the type objects
  PX_ASSIGN_OR_RETURN(auto string_type_object, TypeObject::Create(IRNodeType::kString, this));
  global_var_table_->Add(ASTVisitorImpl::kStringTypeName, string_type_object);
  PX_ASSIGN_OR_RETURN(auto int_type_object, TypeObject::Create(IRNodeType::kInt, this));
  global_var_table_->Add(ASTVisitorImpl::kIntTypeName, int_type_object);
  PX_ASSIGN_OR_RETURN(auto float_type_object, TypeObject::Create(IRNodeType::kFloat, this));
  global_var_table_->Add(ASTVisitorImpl::kFloatTypeName, float_type_object);
  PX_ASSIGN_OR_RETURN(auto bool_type_object, TypeObject::Create(IRNodeType::kBool, this));
  global_var_table_->Add(ASTVisitorImpl::kBoolTypeName, bool_type_object);
  PX_ASSIGN_OR_RETURN(auto list_type_object, TypeObject::Create(QLObjectType::kList, this));
  global_var_table_->Add(ASTVisitorImpl::kListTypeName, list_type_object);
  // Populate other reserved words
  global_var_table_->Add(ASTVisitorImpl::kNoneName, std::make_shared<NoneObject>(this));

  return CreateBoolLiterals();
}

Status ASTVisitorImpl::CreateBoolLiterals() {
  auto bool_ast = std::make_shared<pypa::Ast>(pypa::AstType::Bool);
  bool_ast->line = 0;
  bool_ast->column = 0;
  PX_ASSIGN_OR_RETURN(auto true_ir, ir_graph_->CreateNode<BoolIR>(bool_ast, true));
  PX_ASSIGN_OR_RETURN(auto true_object, ExprObject::Create(true_ir, this));
  global_var_table_->Add(ASTVisitorImpl::kTrueName, true_object);
  PX_ASSIGN_OR_RETURN(auto false_ir, ir_graph_->CreateNode<BoolIR>(bool_ast, false));
  PX_ASSIGN_OR_RETURN(auto false_object, ExprObject::Create(false_ir, this));
  global_var_table_->Add(ASTVisitorImpl::kFalseName, false_object);
  return Status::OK();
}

Status ASTVisitorImpl::ProcessExprStmtNode(const pypa::AstExpressionStatementPtr& e) {
  OperatorContext op_context({}, "", {});
  return Process(e->expr, op_context).status();
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessSingleExpressionModule(
    const pypa::AstModulePtr& module) {
  OperatorContext op_context({}, "");
  const std::vector<pypa::AstStmt>& items_list = module->body->items;
  if (items_list.size() != 1) {
    return CreateAstError(module,
                          "ProcessModuleExpression only works for single lined statements.");
  }
  const pypa::AstStmt& stmt = items_list[0];
  switch (stmt->type) {
    case pypa::AstType::ExpressionStatement: {
      return Process(PYPA_PTR_CAST(ExpressionStatement, stmt)->expr, op_context);
    }
    default: {
      return CreateAstError(module, "Want expression, got $0", GetAstTypeName(stmt->type));
    }
  }
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ParseAndProcessSingleExpression(
    std::string_view single_expr_str, bool import_px) {
  Parser parser;
  PX_ASSIGN_OR_RETURN(pypa::AstModulePtr ast,
                      parser.Parse(single_expr_str.data(), /* parse_doc_strings */ false));
  if (import_px) {
    auto child_visitor = CreateChild();
    // Use a child of this ASTVisitor so that we can add px to its child var_table without
    // affecting top-level visitor state.
    child_visitor->var_table_->Add(
        PixieModule::kPixieModuleObjName,
        (*child_visitor->module_handler_)[PixieModule::kPixieModuleObjName]);
    return child_visitor->ProcessSingleExpressionModule(ast);
  }

  return ProcessSingleExpressionModule(ast);
}

Status ASTVisitorImpl::ProcessModuleNode(const pypa::AstModulePtr& m) {
  PX_RETURN_IF_ERROR(ProcessASTSuite(m->body, /*is_function_definition_body*/ false));
  return Status::OK();
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ParseStringAsType(const pypa::AstPtr& ast,
                                                        const std::string& value,
                                                        const std::shared_ptr<TypeObject>& type) {
  if (type->ql_object_type() != QLObjectType::kExpr) {
    auto parsed_or_s = ParseAndProcessSingleExpression(value, /*import_px*/ true);
    if (!parsed_or_s.ok()) {
      return AddOuterContextToError(parsed_or_s.status(), ast, "Failed to parse arg '$0'as '$1'",
                                    value, QLObjectTypeString(type->ql_object_type()));
    }
    auto parsed = parsed_or_s.ConsumeValueOrDie();
    if (!type->ObjectMatches(parsed)) {
      return CreateAstError(ast, "Expected '$0' got '$1' for expr '$2'", type->TypeString(),
                            QLObjectTypeString(parsed->type()));
    }
    return parsed;
  }

  ExpressionIR* node;
  switch (type->data_type()) {
    case types::DataType::BOOLEAN: {
      bool val;
      if (!absl::SimpleAtob(value, &val)) {
        return CreateAstError(ast, "Failed to parse arg with value '$0' as bool.", value);
      }
      PX_ASSIGN_OR_RETURN(node, ir_graph()->CreateNode<BoolIR>(ast, val));
      break;
    }
    case types::DataType::STRING: {
      PX_ASSIGN_OR_RETURN(node, ir_graph()->CreateNode<StringIR>(ast, value));
      break;
    }
    case types::DataType::INT64: {
      int64_t val;
      if (!absl::SimpleAtoi(value, &val)) {
        return CreateAstError(ast, "Failed to parse arg with value '$0' as int64.", value);
      }
      PX_ASSIGN_OR_RETURN(node, ir_graph()->CreateNode<IntIR>(ast, val));
      break;
    }
    case types::DataType::FLOAT64: {
      double val;
      if (!absl::SimpleAtod(value, &val)) {
        return CreateAstError(ast, "Failed to parse arg with value '$0' as float64.", value);
      }
      PX_ASSIGN_OR_RETURN(node, ir_graph()->CreateNode<FloatIR>(ast, val));
      break;
    }
    case types::DataType::TIME64NS: {
      int64_t val;
      if (!absl::SimpleAtoi(value, &val)) {
        return CreateAstError(ast, "Failed to parse arg with value '$0' as time.", value);
      }
      PX_ASSIGN_OR_RETURN(node, ir_graph()->CreateNode<TimeIR>(ast, val));
      break;
    }
    case types::DataType::UINT128: {
      return CreateAstError(ast, "Passing arg of type UINT128 is currently unsupported.");
      break;
    }
    default:
      return CreateAstError(
          ast, "All arguments to executed functions must have an underlying concrete type.");
  }
  return ExprObject::Create(node, this);
}

StatusOr<ArgMap> ASTVisitorImpl::ProcessExecFuncArgs(const pypa::AstPtr& ast,
                                                     const std::shared_ptr<FuncObject>& func,
                                                     const ArgValues& arg_values) {
  ArgMap args;
  for (const auto& arg : arg_values) {
    if (std::find(func->arguments().begin(), func->arguments().end(), arg.name()) ==
        func->arguments().end()) {
      return CreateAstError(ast, "Function '$0' does not have an argument called '$1'",
                            func->name(), arg.name());
    }
    auto it = func->arg_types().find(arg.name());
    if (it == func->arg_types().end()) {
      return CreateAstError(ast, "Arg type annotation required. Function: '$0', arg: '$1'",
                            func->name(), arg.name());
    }
    PX_ASSIGN_OR_RETURN(auto node, ParseStringAsType(ast, arg.value(), it->second));
    // FuncObject::Call has logic to handle accepting normal args as kwargs,
    // so its easiest to just pass everything as kwargs. In the future, if we want to support
    // variadic args in exec funcs we will have to change this.
    args.kwargs.emplace_back(arg.name(), node);
  }
  return args;
}

Status ASTVisitorImpl::ProcessExecFuncs(const ExecFuncs& exec_funcs) {
  // TODO(James): handle errors here better. For now I have a fake ast ptr with -1 for line and
  // col.
  auto ast = std::make_shared<pypa::AstExpressionStatement>();
  ast->line = 0;
  ast->column = 0;
  for (const auto& func : exec_funcs) {
    if (func.func_name() == "") {
      return CreateAstError(ast, "Must specify func_name for each FuncToExecute.");
    }
    if (func.output_table_prefix() == "") {
      return CreateAstError(ast, "Output_table_prefix must be specified for function $0.",
                            func.func_name());
    }

    // Get Function Object.
    auto objptr = var_table_->Lookup(func.func_name());
    if (objptr == nullptr) {
      return CreateAstError(ast, "Function to execute, '$0', not found.", func.func_name());
    }
    if (objptr->type() != QLObjectType::kFunction) {
      return CreateAstError(ast, "'$0' is a '$1' not a function.", func.func_name(),
                            objptr->name());
    }
    auto func_obj = std::static_pointer_cast<FuncObject>(objptr);

    // Process arguments.
    ArgValues arg_values(func.arg_values().begin(), func.arg_values().end());
    PX_ASSIGN_OR_RETURN(auto arg_map, ProcessExecFuncArgs(ast, func_obj, arg_values));

    // Call function.
    PX_ASSIGN_OR_RETURN(auto return_obj, func_obj->Call(arg_map, ast));

    // Process returns.
    if (!CollectionObject::IsCollection(return_obj)) {
      if (return_obj->type() != QLObjectType::kDataframe) {
        return CreateAstError(ast, "Function '$0' returns '$1' but should return a DataFrame.",
                              func.func_name(), return_obj->name());
      }
      auto df = std::static_pointer_cast<Dataframe>(return_obj);
      PX_RETURN_IF_ERROR(AddResultSink(ir_graph(), ast, func.output_table_prefix(), df->op(),
                                       compiler_state_->result_address(),
                                       compiler_state_->result_ssl_targetname()));
      continue;
    }

    auto return_collection = std::static_pointer_cast<CollectionObject>(return_obj);
    for (const auto& [i, obj] : Enumerate(return_collection->items())) {
      if (obj->type() != QLObjectType::kDataframe) {
        return CreateAstError(
            ast, "Function '$0' returns '$1' at index $2. All returned objects must be dataframes.",
            func.func_name(), obj->name(), i);
      }
      auto df = std::static_pointer_cast<Dataframe>(obj);
      auto out_name = absl::Substitute("$0[$1]", func.output_table_prefix(), i);
      PX_RETURN_IF_ERROR(AddResultSink(ir_graph(), ast, out_name, df->op(),
                                       compiler_state_->result_address(),
                                       compiler_state_->result_ssl_targetname()));
    }
  }
  return Status::OK();
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessASTSuite(const pypa::AstSuitePtr& body,
                                                      bool is_function_definition_body) {
  pypa::AstStmtList items_list = body->items;
  if (items_list.size() == 0) {
    return CreateAstError(body, "No runnable code found");
  }
  if (items_list[0]->type == pypa::AstType::DocString) {
    if (!is_function_definition_body) {
      PX_ASSIGN_OR_RETURN(auto doc_string,
                          ProcessDocString(PYPA_PTR_CAST(DocString, items_list[0])));
      var_table_->Add("__doc__", doc_string);
    }
    // NOTE(james): If this is a function definition body we can still erase, since we handle
    // function docstrings at function definition time.
    items_list.erase(items_list.begin());
  } else {
    if (!is_function_definition_body) {
      PX_ASSIGN_OR_RETURN(auto ir_node, ir_graph_->CreateNode<StringIR>(body, ""));
      PX_ASSIGN_OR_RETURN(auto doc_string, ExprObject::Create(ir_node, this));
      var_table_->Add("__doc__", doc_string);
    }
  }
  // iterate through all the items on this list.
  for (pypa::AstStmt stmt : items_list) {
    switch (stmt->type) {
      case pypa::AstType::Import: {
        PX_RETURN_IF_ERROR(ProcessImport(PYPA_PTR_CAST(Import, stmt)));
        break;
      }
      case pypa::AstType::ImportFrom: {
        PX_RETURN_IF_ERROR(ProcessImportFrom(PYPA_PTR_CAST(ImportFrom, stmt)));
        break;
      }
      case pypa::AstType::ExpressionStatement: {
        PX_RETURN_IF_ERROR(ProcessExprStmtNode(PYPA_PTR_CAST(ExpressionStatement, stmt)));
        break;
      }
      case pypa::AstType::Assign: {
        PX_RETURN_IF_ERROR(ProcessAssignNode(PYPA_PTR_CAST(Assign, stmt)));
        break;
      }
      case pypa::AstType::FunctionDef: {
        PX_RETURN_IF_ERROR(ProcessFunctionDefNode(PYPA_PTR_CAST(FunctionDef, stmt)));
        break;
      }
      case pypa::AstType::DocString: {
        return CreateAstError(stmt,
                              "Doc strings are only allowed at the start of a module or function.");
      }
      case pypa::AstType::Return: {
        // If we are not parsing a function definition's body then we must error.
        if (!is_function_definition_body) {
          return CreateAstError(stmt, "'return' outside function");
        }
        // We exit early if the function definition return is used.
        return ProcessFuncDefReturn(PYPA_PTR_CAST(Return, stmt));
      }
      default: {
        return CreateAstError(stmt, "Can't parse expression of type $0",
                              GetAstTypeName(stmt->type));
      }
    }
  }
  // If we reach the end of the stmt list before hitting a return, return a NoneObject.
  return std::static_pointer_cast<QLObject>(std::make_shared<NoneObject>(body, this));
}

Status ASTVisitorImpl::AddPixieModule(std::string_view as_name) {
  PX_ASSIGN_OR_RETURN(auto px, PixieModule::Create(ir_graph_, compiler_state_, this,
                                                   func_based_exec_, reserved_names_));
  var_table_->Add(as_name, px);
  return Status::OK();
}

std::string AsNameFromAlias(const pypa::AstAliasPtr& alias) {
  std::string name = PYPA_PTR_CAST(Name, alias->name)->id;

  if (alias->as_name == nullptr) {
    return name;
  }
  return PYPA_PTR_CAST(Name, alias->as_name)->id;
}

Status ASTVisitorImpl::ProcessImport(const pypa::AstImportPtr& import) {
  auto alias = PYPA_PTR_CAST(Alias, import->names);
  std::string name = PYPA_PTR_CAST(Name, alias->name)->id;
  std::string as_name = AsNameFromAlias(alias);

  if (!module_handler_->contains(name)) {
    return CreateAstError(import, "ModuleNotFoundError: No module named '$0'", name);
  }
  var_table_->Add(as_name, (*module_handler_)[name]);
  return Status::OK();
}

Status ASTVisitorImpl::ProcessImportFrom(const pypa::AstImportFromPtr& from) {
  if (from->level != 0) {
    return error::Unimplemented("Unexpected import level $0, expected 0", from->level);
  }
  std::string module = PYPA_PTR_CAST(Name, from->module)->id;

  if (!module_handler_->contains(module)) {
    return CreateAstError(from, "ModuleNotFoundError: No module named '$0'", module);
  }
  auto obj = (*module_handler_)[module];

  pypa::AstExprList aliases;
  if (from->names->type == AstType::Tuple) {
    auto tup = PYPA_PTR_CAST(Tuple, from->names);
    aliases = tup->elements;
  } else if (from->names->type == AstType::Alias) {
    aliases = {from->names};
  } else {
    return CreateAstError(from->names, "Unexpected type in import statement '$0'",
                          magic_enum::enum_name(from->names->type));
  }
  for (const auto& el : aliases) {
    auto alias = PYPA_PTR_CAST(Alias, el);
    std::string name = PYPA_PTR_CAST(Name, alias->name)->id;
    std::string as_name = AsNameFromAlias(alias);

    if (!obj->HasAttribute(name)) {
      return CreateAstError(from, "cannot import name '$1' from '$0'", module, name);
    }
    PX_ASSIGN_OR_RETURN(auto attr, obj->GetAttribute(from, name));
    var_table_->Add(as_name, attr);
  }
  return Status::OK();
}

// Assignment by subscript is more restrictive than assignment by attribute.
// Subscript assignment is currently only valid for creating map expressions such as the
// following: df['foo'] = 1+2
Status ASTVisitorImpl::ProcessSubscriptAssignment(const pypa::AstSubscriptPtr& subscript,
                                                  const pypa::AstExpr& expr_node) {
  PX_ASSIGN_OR_RETURN(auto processed_node, Process(subscript, {{}, "", {}}));
  PX_ASSIGN_OR_RETURN(auto processed_target_table, Process(subscript->value, {{}, "", {}}));

  if (processed_target_table->type() != QLObjectType::kDataframe) {
    return CreateAstError(subscript, "Subscript assignment not allowed for $0 objects",
                          processed_target_table->name());
  }

  return ProcessMapAssignment(subscript->value,
                              std::static_pointer_cast<Dataframe>(processed_target_table),
                              processed_node, expr_node);
}

// Assignment by attribute supports all of the cases that subscript assignment does,
// in addition to assigning to
Status ASTVisitorImpl::ProcessAttributeAssignment(const pypa::AstAttributePtr& attr,
                                                  const pypa::AstExpr& expr_node) {
  PX_ASSIGN_OR_RETURN(auto processed_target, Process(attr->value, {{}, "", {}}));

  if (processed_target->type() != QLObjectType::kDataframe) {
    PX_ASSIGN_OR_RETURN(std::string attr_name, GetAttributeStr(attr));
    PX_ASSIGN_OR_RETURN(auto processed_value,
                        Process(PYPA_PTR_CAST(Call, expr_node), {{}, "", {}}));
    return processed_target->AssignAttribute(attr_name, processed_value);
  }

  // If the target is a Dataframe, we are doing a Subscript map assignment like "df.foo = 2".
  // We need to do special handling here as opposed to the above logic in order to produce a new
  // dataframe.
  PX_ASSIGN_OR_RETURN(auto processed_node, Process(attr, {{}, "", {}}));
  return ProcessMapAssignment(attr->value, std::static_pointer_cast<Dataframe>(processed_target),
                              processed_node, expr_node);
}

Status ASTVisitorImpl::ProcessMapAssignment(const pypa::AstPtr& assign_target,
                                            std::shared_ptr<Dataframe> parent_df,
                                            QLObjectPtr target_node,
                                            const pypa::AstExpr& expr_node) {
  if (assign_target->type != AstType::Name) {
    return CreateAstError(assign_target,
                          "Can only assign to Dataframe by subscript from Name, received $0",
                          GetAstTypeName(assign_target->type));
  }
  auto assign_name_string = GetNameAsString(assign_target);

  PX_ASSIGN_OR_RETURN(auto target_column,
                      GetArgAs<ColumnIR>(assign_target, target_node, "assignment target"));

  if (!parent_df->op()) {
    return CreateAstError(assign_target,
                          "Cannot assign column to dataframe that does not contain an operator");
  }

  // Maps can only assign to the same table as the input table when of the form:
  // df['foo'] = df['bar'] + 2
  OperatorContext op_context{{parent_df->op()}, Dataframe::kMapOpID, {assign_name_string}};
  PX_ASSIGN_OR_RETURN(auto expr_obj, Process(expr_node, op_context));
  PX_ASSIGN_OR_RETURN(ExpressionIR * expr_val,
                      GetArgAs<ExpressionIR>(expr_node, expr_obj, "assignment value"));

  PX_ASSIGN_OR_RETURN(auto dataframe, parent_df->FromColumnAssignment(compiler_state_, expr_node,
                                                                      target_column, expr_val));
  var_table_->Add(assign_name_string, dataframe);

  return ir_graph_->DeleteNode(target_column->id());
}

StatusOr<QLObjectPtr> ASTVisitorImpl::Process(const pypa::AstExpr& node,
                                              const OperatorContext& op_context) {
  switch (node->type) {
    case AstType::Attribute: {
      return ProcessAttribute(PYPA_PTR_CAST(Attribute, node), op_context);
    }
    case AstType::BinOp: {
      return ProcessDataBinOp(PYPA_PTR_CAST(BinOp, node), op_context);
    }
    case AstType::BoolOp: {
      return ProcessDataBoolOp(PYPA_PTR_CAST(BoolOp, node), op_context);
    }
    case AstType::Call:
      return ProcessCallNode(PYPA_PTR_CAST(Call, node), op_context);
    case AstType::Compare: {
      return ProcessDataCompare(PYPA_PTR_CAST(Compare, node), op_context);
    }
    case AstType::Dict: {
      return ProcessDict(PYPA_PTR_CAST(Dict, node), op_context);
    }
    case AstType::List: {
      return ProcessList(PYPA_PTR_CAST(List, node), op_context);
    }
    case AstType::Name: {
      return LookupVariable(PYPA_PTR_CAST(Name, node));
    }
    case AstType::Number: {
      return ProcessNumber(PYPA_PTR_CAST(Number, node));
    }
    case AstType::Str: {
      return ProcessStr(PYPA_PTR_CAST(Str, node));
    }
    case AstType::Subscript: {
      return ProcessSubscriptCall(PYPA_PTR_CAST(Subscript, node), op_context);
    }
    case AstType::Tuple: {
      return ProcessTuple(PYPA_PTR_CAST(Tuple, node), op_context);
    }
    case AstType::UnaryOp: {
      return ProcessDataUnaryOp(PYPA_PTR_CAST(UnaryOp, node), op_context);
    }
    default:
      return CreateAstError(node, "Expression node '$0' not defined", GetAstTypeName(node->type));
  }
}

Status ASTVisitorImpl::ProcessAssignNode(const pypa::AstAssignPtr& node) {
  // Check # nodes to assign.
  if (node->targets.size() != 1) {
    return CreateAstError(node, "We only support single target assignment.");
  }
  // Get the name that we are targeting.
  auto target_node = node->targets[0];

  // Special handler for this type of statement: df['foo'] = df['bar']
  if (target_node->type == AstType::Subscript) {
    return ProcessSubscriptAssignment(PYPA_PTR_CAST(Subscript, node->targets[0]), node->value);
  }
  if (target_node->type == AstType::Attribute) {
    return ProcessAttributeAssignment(PYPA_PTR_CAST(Attribute, node->targets[0]), node->value);
  }

  if (target_node->type != AstType::Name) {
    return CreateAstError(target_node, "Assignment target must be a Name or Subscript");
  }

  std::string assign_name = GetNameAsString(target_node);
  OperatorContext op_context({}, "", {});
  PX_ASSIGN_OR_RETURN(auto processed_node, Process(node->value, op_context));
  var_table_->Add(assign_name, processed_node);
  return Status::OK();
}

Status ASTVisitorImpl::DoesArgMatchAnnotation(QLObjectPtr ql_arg, QLObjectPtr annotation_obj) {
  DCHECK(annotation_obj);
  if (annotation_obj->type() == QLObjectType::kType) {
    auto type_object = std::static_pointer_cast<TypeObject>(annotation_obj);
    if (type_object->ObjectMatches(ql_arg)) {
      return Status::OK();
    }

    if (!ExprObject::IsExprObject(ql_arg)) {
      return ql_arg->CreateError("Expected '$0', received '$1'", type_object->TypeString(),
                                 QLObjectTypeString(ql_arg->type()));
    }
    return type_object->NodeMatches(static_cast<ExprObject*>(ql_arg.get())->expr());
  } else if (annotation_obj->type() != ql_arg->type()) {
    return ql_arg->CreateError("Expected '$0', received '$1'", annotation_obj->name(),
                               ql_arg->name());
  }
  return Status::OK();
}

StatusOr<QLObjectPtr> ASTVisitorImpl::FuncDefHandler(
    const std::vector<std::string>& arg_names,
    const absl::flat_hash_map<std::string, QLObjectPtr>& arg_annotation_objs,
    const pypa::AstSuitePtr& body, const pypa::AstPtr& ast, const ParsedArgs& args) {
  // TODO(philkuz) (PL-1365) figure out how to wrap the internal errors with the ast that's passed
  // in.
  PX_UNUSED(ast);

  auto func_visitor = CreateChild();
  for (const std::string& arg_name : arg_names) {
    QLObjectPtr arg_object = args.GetArg(arg_name);

    if (arg_annotation_objs.contains(arg_name)) {
      PX_RETURN_IF_ERROR(
          DoesArgMatchAnnotation(arg_object, arg_annotation_objs.find(arg_name)->second));
    }
    func_visitor->var_table()->Add(arg_name, arg_object);
  }

  return func_visitor->ProcessASTSuite(body, /*is_function_definition_body*/ true);
}

Status ASTVisitorImpl::ProcessFunctionDefNode(const pypa::AstFunctionDefPtr& node) {
  // Create the func object.
  // Use the new function defintion body as the function object.
  // Every time the function is evaluated we should evaluate the body with the values for the
  // args passed into the scope.
  // Parse the args to create the necessary
  auto function_name_node = node->name;
  std::vector<std::string> parsed_arg_names;
  absl::flat_hash_map<std::string, QLObjectPtr> arg_annotations_objs;
  for (const auto& arg : node->args.arguments) {
    if (arg->type != AstType::Arg) {
      return CreateAstError(arg, "function parameter must be an argument, not a $0",
                            GetAstTypeName(arg->type));
    }
    auto arg_ptr = PYPA_PTR_CAST(Arg, arg);
    parsed_arg_names.push_back(arg_ptr->arg);
    if (arg_ptr->annotation) {
      PX_ASSIGN_OR_RETURN(auto annotation_obj, Process(arg_ptr->annotation, {{}, "", {}}));
      arg_annotations_objs[arg_ptr->arg] = annotation_obj;
    }
  }
  // TODO(philkuz) delete keywords from the function definition.
  // For some reason this is kept around, not clear why, making sure that it's 0 for now.
  DCHECK_EQ(node->args.keywords.size(), 0UL);

  // The default values for args. Should be the same length as args. For now we should consider
  // not processing these.
  DCHECK_EQ(node->args.defaults.size(), node->args.arguments.size());
  for (const auto& default_value : node->args.defaults) {
    // TODO(philkuz) support default.
    if (default_value != nullptr) {
      return CreateAstError(default_value, "default values not supported in function definitions");
    }
  }

  // TODO(philkuz) support *args.
  if (node->args.args) {
    return CreateAstError(node->args.args,
                          "variable length args are not supported in function definitions");
  }

  // TODO(philkuz) support **kwargs
  if (node->args.kwargs) {
    return CreateAstError(node->args.kwargs,
                          "variable length kwargs are not supported in function definitions");
  }

  if (function_name_node->type != AstType::Name) {
    return CreateAstError(function_name_node, "function definition must be a name, not a $0",
                          GetAstTypeName(function_name_node->type));
  }

  if (node->body->type != AstType::Suite) {
    return CreateAstError(node->body, "function body of type $0 not allowed",
                          GetAstTypeName(node->body->type));
  }
  pypa::AstSuitePtr body = PYPA_PTR_CAST(Suite, node->body);
  std::string function_name = GetNameAsString(function_name_node);

  PX_ASSIGN_OR_RETURN(auto defined_func,
                      FuncObject::Create(function_name, parsed_arg_names, {}, false, false,
                                         std::bind(&ASTVisitorImpl::FuncDefHandler, this,
                                                   parsed_arg_names, arg_annotations_objs, body,
                                                   std::placeholders::_1, std::placeholders::_2),
                                         this));
  DCHECK_LE(node->decorators.size(), 1U);
  for (const auto& d : node->decorators) {
    // Each decorator should be a function that takes in the defined_func as an argument.
    PX_ASSIGN_OR_RETURN(auto dec_fn, Process(d, OperatorContext{{}, ""}));
    PX_ASSIGN_OR_RETURN(auto fn_object, GetCallMethod(d, dec_fn));
    ArgMap map{{}, {defined_func}};
    PX_ASSIGN_OR_RETURN(auto object_fn, fn_object->Call(map, d));
    PX_ASSIGN_OR_RETURN(defined_func, GetCallMethod(d, object_fn));
  }

  PX_ASSIGN_OR_RETURN(auto doc_string_obj, ProcessFuncDefDocString(body));
  PX_ASSIGN_OR_RETURN(auto doc_string, GetAsString(doc_string_obj));
  PX_RETURN_IF_ERROR(defined_func->SetDocString(doc_string));

  PX_RETURN_IF_ERROR(defined_func->ResolveArgAnnotationsToTypes(arg_annotations_objs));

  var_table_->Add(function_name, defined_func);
  return Status::OK();
}

Status ASTVisitorImpl::ValidateSubscriptValue(const pypa::AstExpr& node,
                                              const OperatorContext& op_context) {
  if (op_context.operator_name != Dataframe::kMapOpID) {
    return Status::OK();
  }
  switch (node->type) {
    case AstType::Attribute: {
      // We want to make sure that the parent of an attribute is completely valid, even if it's
      // nested. ie. `df.ctx['service']`
      return ValidateSubscriptValue(PYPA_PTR_CAST(Attribute, node)->value, op_context);
    }
    case AstType::Name: {
      std::string name = GetNameAsString(node);
      if (std::find(op_context.referenceable_dataframes.begin(),
                    op_context.referenceable_dataframes.end(),
                    name) == op_context.referenceable_dataframes.end()) {
        return CreateAstError(node, "name '$0' is not available in this context", name);
      }
      ABSL_FALLTHROUGH_INTENDED;
    }
    default:
      return Status::OK();
  }
}
StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessSubscriptCall(const pypa::AstSubscriptPtr& node,
                                                           const OperatorContext& op_context) {
  // Validate to make sure that we can actually take the subscript in this context.
  PX_RETURN_IF_ERROR(ValidateSubscriptValue(node->value, op_context));
  PX_ASSIGN_OR_RETURN(QLObjectPtr pyobject, Process(node->value, op_context));
  if (!pyobject->HasSubscriptMethod()) {
    return pyobject->CreateError("$0 is not subscriptable", pyobject->name());
  }
  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> func_object, pyobject->GetSubscriptMethod());

  auto slice = node->slice;
  if (slice->type != AstType::Index) {
    return CreateAstError(slice, "'$0' object must be an index", GetAstTypeName(slice->type));
  }

  std::vector<std::string> dfs = op_context.referenceable_dataframes;
  if (node->value->type == AstType::Name) {
    dfs.push_back(GetNameAsString(node->value));
  }

  OperatorContext new_op_context(op_context.parent_ops, op_context.operator_name, dfs);
  PX_ASSIGN_OR_RETURN(QLObjectPtr arg, Process(PYPA_PTR_CAST(Index, slice)->value, new_op_context));
  ArgMap args;
  args.args.push_back(arg);
  return func_object->Call(args, node);
}

StatusOr<std::string> ASTVisitorImpl::GetFuncName(const pypa::AstCallPtr& node) {
  std::string func_name;
  switch (node->function->type) {
    case AstType::Name: {
      func_name = GetNameAsString(node->function);
      break;
    }
    case AstType::Attribute: {
      auto attr = PYPA_PTR_CAST(Attribute, node->function);
      if (attr->attribute->type != AstType::Name) {
        return CreateAstError(node->function, "Couldn't get string name out of node of type $0.",
                              GetAstTypeName(attr->attribute->type));
      }
      func_name = GetNameAsString(attr->attribute);
      break;
    }
    default: {
      return CreateAstError(node->function, "Couldn't get string name out of node of type $0.",
                            GetAstTypeName(node->function->type));
    }
  }
  return func_name;
}

StatusOr<ArgMap> ASTVisitorImpl::ProcessArgs(const pypa::AstCallPtr& call_ast,
                                             const OperatorContext& op_context) {
  ArgMap arg_map;
  for (const auto& arg : call_ast->arguments) {
    PX_ASSIGN_OR_RETURN(auto value, Process(arg, op_context));
    arg_map.args.push_back(value);
  }

  // Iterate through the keywords
  for (auto& kw_ptr : call_ast->keywords) {
    std::string key = GetNameAsString(kw_ptr->name);
    PX_ASSIGN_OR_RETURN(auto value, Process(kw_ptr->value, op_context));
    arg_map.kwargs.emplace_back(key, value);
  }

  return arg_map;
}

StatusOr<QLObjectPtr> ASTVisitorImpl::LookupVariable(const pypa::AstPtr& ast,
                                                     const std::string& name) {
  auto var = var_table_->Lookup(name);
  if (var != nullptr) {
    var->SetAst(ast);
    return var;
  }
  auto global_var = global_var_table_->Lookup(name);
  if (global_var == nullptr) {
    return CreateAstError(ast, "name '$0' is not defined", name);
  }
  global_var->SetAst(ast);
  return global_var;
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessAttribute(const pypa::AstAttributePtr& node,
                                                       const OperatorContext& op_context) {
  PX_ASSIGN_OR_RETURN(std::string attr_name, GetAttributeStr(node));
  PX_ASSIGN_OR_RETURN(QLObjectPtr value_obj, Process(node->value, op_context));
  return value_obj->GetAttribute(node, attr_name);
}

StatusOr<std::string> ASTVisitorImpl::GetAttributeStr(const pypa::AstAttributePtr& attr) {
  if (attr->attribute->type != AstType::Name) {
    return CreateAstError(attr, "$0 not a valid attribute", GetAstTypeName(attr->attribute->type));
  }
  return GetNameAsString(attr->attribute);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessCallNode(const pypa::AstCallPtr& node,
                                                      const OperatorContext& op_context) {
  std::shared_ptr<FuncObject> func_object;
  // pyobject declared up here because we need this object to be allocated when
  // func_object->Call() is made.
  PX_ASSIGN_OR_RETURN(QLObjectPtr pyobject, Process(node->function, op_context));
  if (ExprObject::IsExprObject(pyobject)) {
    auto expr = static_cast<ExprObject*>(pyobject.get());
    // TODO(philkuz) Look into pushing column resolution earlier to avoid this messiness.
    // if we can know all the columns in the DataFrame at init, this would
    // not exist. Otherwise, we treat any attribute of the DataFrame that doesn't exist
    // as a ColumnNode.
    if (Match(expr->expr(), ColumnNode())) {
      return CreateAstError(node, "dataframe has no method '$0'",
                            static_cast<ColumnIR*>(expr->expr())->col_name());
    }
    return CreateAstError(node, "expression object is not callable");
  }

  PX_ASSIGN_OR_RETURN(func_object, GetCallMethod(node, pyobject));
  PX_ASSIGN_OR_RETURN(ArgMap args, ProcessArgs(node, op_context));
  return func_object->Call(args, node);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessStr(const pypa::AstStrPtr& ast) {
  PX_ASSIGN_OR_RETURN(auto str_value, GetStrAstValue(ast));
  PX_ASSIGN_OR_RETURN(StringIR * node, ir_graph_->CreateNode<StringIR>(ast, str_value));
  return ExprObject::Create(node, this);
}

StatusOr<std::vector<QLObjectPtr>> ASTVisitorImpl::ProcessCollectionChildren(
    const pypa::AstExprList& elements, const OperatorContext& op_context) {
  std::vector<QLObjectPtr> children;
  for (auto& child : elements) {
    PX_ASSIGN_OR_RETURN(QLObjectPtr child_node, Process(child, op_context));
    children.push_back(child_node);
  }
  return children;
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessList(const pypa::AstListPtr& ast,
                                                  const OperatorContext& op_context) {
  PX_ASSIGN_OR_RETURN(std::vector<QLObjectPtr> expr_vec,
                      ProcessCollectionChildren(ast->elements, op_context));
  return ListObject::Create(ast, expr_vec, this);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessTuple(const pypa::AstTuplePtr& ast,
                                                   const OperatorContext& op_context) {
  PX_ASSIGN_OR_RETURN(std::vector<QLObjectPtr> expr_vec,
                      ProcessCollectionChildren(ast->elements, op_context));
  return TupleObject::Create(ast, expr_vec, this);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessNumber(const pypa::AstNumberPtr& node) {
  switch (node->num_type) {
    case pypa::AstNumber::Type::Float: {
      PX_ASSIGN_OR_RETURN(FloatIR * ir_node, ir_graph_->CreateNode<FloatIR>(node, node->floating));
      return ExprObject::Create(ir_node, this);
    }
    case pypa::AstNumber::Type::Integer:
    case pypa::AstNumber::Type::Long: {
      PX_ASSIGN_OR_RETURN(IntIR * ir_node, ir_graph_->CreateNode<IntIR>(node, node->integer));
      return ExprObject::Create(ir_node, this);
    }
  }
  return error::Internal("unreachable");
}

StatusOr<udf::ScalarUDFDefinition*> GetUDFDefinition(const std::shared_ptr<udf::Registry>& registry,
                                                     const std::string& name,
                                                     const std::vector<ExpressionIR*>& args) {
  std::vector<types::DataType> arg_types;
  for (const ExpressionIR* arg : args) {
    if (!arg->IsData()) {
      return nullptr;
    }
    DCHECK(arg->IsDataTypeEvaluated());
    arg_types.push_back(arg->EvaluatedDataType());
  }
  auto udf_or_s = registry->GetScalarUDFDefinition(name, arg_types);
  if (!udf_or_s.ok() && udf_or_s.code() == statuspb::NOT_FOUND) {
    return nullptr;
  }
  return udf_or_s;
}

StatusOr<ExpressionIR*> ExecUDF(IR* graph, const pypa::AstPtr& ast, udf::ScalarUDFDefinition* def,
                                const std::vector<ExpressionIR*>& args) {
  std::vector<std::shared_ptr<types::ColumnWrapper>> column_pool;
  std::vector<const types::ColumnWrapper*> columns;
  // Extract the argument values out into column wrappers.
  for (ExpressionIR* arg : args) {
    CHECK(arg->IsData()) << "Unexpected type for UDCF ";
    DCHECK(arg->IsDataTypeEvaluated());
    types::DataType arg_type = arg->EvaluatedDataType();
    auto col = types::ColumnWrapper::Make(arg_type, 0);
    column_pool.push_back(col);
    switch (arg->type()) {
      case IRNodeType::kInt:
        col->Append<types::Int64Value>(static_cast<IntIR*>(arg)->val());
        break;
      case IRNodeType::kFloat:
        col->Append<types::Float64Value>(static_cast<FloatIR*>(arg)->val());
        break;
      case IRNodeType::kString:
        col->Append<types::StringValue>(static_cast<StringIR*>(arg)->str());
        break;
      case IRNodeType::kUInt128:
        col->Append<types::UInt128Value>(static_cast<UInt128IR*>(arg)->val());
        break;
      case IRNodeType::kBool:
        col->Append<types::BoolValue>(static_cast<BoolIR*>(arg)->val());
        break;
      case IRNodeType::kTime:
        col->Append<types::Time64NSValue>(static_cast<TimeIR*>(arg)->val());
        break;
      default:
        CHECK("Can't find Arg type");
    }
    columns.push_back(col.get());
  }

  // Execute the UDF.
  auto output = types::ColumnWrapper::Make(def->exec_return_type(), 1);
  auto function_ctx = std::make_unique<px::carnot::udf::FunctionContext>(nullptr, nullptr);
  auto udf = def->Make();
  PX_RETURN_IF_ERROR(def->ExecBatch(udf.get(), function_ctx.get(), columns, output.get(), 1));

  // Convert the output type into a DataIR.
  switch (def->exec_return_type()) {
    case types::INT64:
      return graph->CreateNode<IntIR>(ast, output->Get<types::Int64Value>(0).val);
    case types::FLOAT64:
      return graph->CreateNode<FloatIR>(ast, output->Get<types::Float64Value>(0).val);
    case types::STRING:
      return graph->CreateNode<StringIR>(ast, output->Get<types::StringValue>(0));
    case types::UINT128:
      return graph->CreateNode<UInt128IR>(ast, output->Get<types::UInt128Value>(0).val);
    case types::BOOLEAN:
      return graph->CreateNode<BoolIR>(ast, output->Get<types::BoolValue>(0).val);
    case types::TIME64NS:
      return graph->CreateNode<TimeIR>(ast, output->Get<types::Time64NSValue>(0).val);
    default:
      return CreateAstError(ast, "Unable to find a matching return type for the UDF");
  }
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessDataBinOp(const pypa::AstBinOpPtr& node,
                                                       const OperatorContext& op_context) {
  std::string op_str = pypa::to_string(node->op);

  PX_ASSIGN_OR_RETURN(auto left_obj, Process(node->left, op_context));
  PX_ASSIGN_OR_RETURN(auto right_obj, Process(node->right, op_context));
  PX_ASSIGN_OR_RETURN(ExpressionIR * left,
                      GetArgAs<ExpressionIR>(left_obj, "left side of operation"));
  PX_ASSIGN_OR_RETURN(ExpressionIR * right,
                      GetArgAs<ExpressionIR>(right_obj, "right side of operation"));

  PX_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));
  std::vector<ExpressionIR*> args = {left, right};
  PX_ASSIGN_OR_RETURN(auto udf, GetUDFDefinition(udf_registry_, op.carnot_op_name, args));
  if (udf != nullptr) {
    PX_ASSIGN_OR_RETURN(ExpressionIR * expr, ExecUDF(ir_graph_, node, udf, args));
    return ExprObject::Create(expr, this);
  }
  PX_ASSIGN_OR_RETURN(FuncIR * ir_node, ir_graph_->CreateNode<FuncIR>(node, op, args));
  return ExprObject::Create(ir_node, this);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessDataBoolOp(const pypa::AstBoolOpPtr& node,
                                                        const OperatorContext& op_context) {
  std::string op_str = pypa::to_string(node->op);
  if (node->values.size() != 2) {
    return CreateAstError(node, "Expected two arguments to '$0'.", op_str);
  }

  PX_ASSIGN_OR_RETURN(auto left_obj, Process(node->values[0], op_context));
  PX_ASSIGN_OR_RETURN(auto right_obj, Process(node->values[1], op_context));
  PX_ASSIGN_OR_RETURN(ExpressionIR * left,
                      GetArgAs<ExpressionIR>(left_obj, "left side of operation"));
  PX_ASSIGN_OR_RETURN(ExpressionIR * right,
                      GetArgAs<ExpressionIR>(right_obj, "right side of operation"));

  PX_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));
  std::vector<ExpressionIR*> args{left, right};
  PX_ASSIGN_OR_RETURN(auto udf, GetUDFDefinition(udf_registry_, op.carnot_op_name, args));
  if (udf != nullptr) {
    PX_ASSIGN_OR_RETURN(ExpressionIR * expr, ExecUDF(ir_graph_, node, udf, args));
    return ExprObject::Create(expr, this);
  }
  PX_ASSIGN_OR_RETURN(FuncIR * ir_node, ir_graph_->CreateNode<FuncIR>(node, op, args));
  return ExprObject::Create(ir_node, this);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessDataCompare(const pypa::AstComparePtr& node,
                                                         const OperatorContext& op_context) {
  DCHECK_EQ(node->operators.size(), 1ULL);
  std::string op_str = pypa::to_string(node->operators[0]);
  if (node->comparators.size() != 1) {
    return CreateAstError(node, "Only expected one argument to the right of '$0'.", op_str);
  }

  PX_ASSIGN_OR_RETURN(auto left_obj, Process(node->left, op_context));
  PX_ASSIGN_OR_RETURN(ExpressionIR * left,
                      GetArgAs<ExpressionIR>(left_obj, "left side of operation"));
  std::vector<ExpressionIR*> args{left};

  for (const auto& comp : node->comparators) {
    PX_ASSIGN_OR_RETURN(auto obj, Process(comp, op_context));
    PX_ASSIGN_OR_RETURN(ExpressionIR * expr, GetArgAs<ExpressionIR>(obj, "argument to operation"));
    args.push_back(expr);
  }

  PX_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));
  PX_ASSIGN_OR_RETURN(auto udf, GetUDFDefinition(udf_registry_, op.carnot_op_name, args));
  if (udf != nullptr) {
    PX_ASSIGN_OR_RETURN(ExpressionIR * expr, ExecUDF(ir_graph_, node, udf, args));
    return ExprObject::Create(expr, this);
  }

  PX_ASSIGN_OR_RETURN(FuncIR * ir_node, ir_graph_->CreateNode<FuncIR>(node, op, args));
  return ExprObject::Create(ir_node, this);
}
StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessDict(const pypa::AstDictPtr& node,
                                                  const OperatorContext& op_context) {
  DCHECK_EQ(node->keys.size(), node->values.size());
  std::vector<QLObjectPtr> keys;
  std::vector<QLObjectPtr> values;
  for (const auto& [i, key] : Enumerate(node->keys)) {
    const auto& value = node->values[i];
    PX_ASSIGN_OR_RETURN(auto key_obj, Process(key, op_context));
    keys.push_back(key_obj);
    PX_ASSIGN_OR_RETURN(auto value_obj, Process(value, op_context));
    values.push_back(value_obj);
  }
  return DictObject::Create(node, keys, values, this);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessDataUnaryOp(const pypa::AstUnaryOpPtr& node,
                                                         const OperatorContext& op_context) {
  PX_ASSIGN_OR_RETURN(auto operand_obj, Process(node->operand, op_context));
  PX_ASSIGN_OR_RETURN(ExpressionIR * operand,
                      GetArgAs<ExpressionIR>(operand_obj, "operand of unary op"));

  std::string op_str = pypa::to_string(node->op);
  PX_ASSIGN_OR_RETURN(FuncIR::Op op, GetUnaryOp(op_str, node));
  if (op.op_code == FuncIR::Opcode::non_op) {
    return ExprObject::Create(operand, this);
  }

  std::vector<ExpressionIR*> args{operand};
  PX_ASSIGN_OR_RETURN(auto udf, GetUDFDefinition(udf_registry_, op.carnot_op_name, args));
  if (udf != nullptr) {
    PX_ASSIGN_OR_RETURN(ExpressionIR * expr, ExecUDF(ir_graph_, node, udf, args));
    return ExprObject::Create(expr, this);
  }
  PX_ASSIGN_OR_RETURN(FuncIR * ir_node, ir_graph_->CreateNode<FuncIR>(node, op, args));
  return ExprObject::Create(ir_node, this);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessFuncDefReturn(const pypa::AstReturnPtr& ret) {
  if (ret->value == nullptr) {
    return std::static_pointer_cast<QLObject>(std::make_shared<NoneObject>(ret, this));
  }

  return Process(ret->value, {{}, "", {}});
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessDocString(const pypa::AstDocStringPtr& doc_string) {
  PX_ASSIGN_OR_RETURN(StringIR * ir_node,
                      ir_graph_->CreateNode<StringIR>(doc_string, doc_string->doc));
  return ExprObject::Create(ir_node, this);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessFuncDefDocString(const pypa::AstSuitePtr& body) {
  pypa::AstStmtList items_list = body->items;
  if (items_list.size() == 0 || items_list[0]->type != pypa::AstType::DocString) {
    PX_ASSIGN_OR_RETURN(StringIR * ir_node, ir_graph_->CreateNode<StringIR>(body, ""));
    return ExprObject::Create(ir_node, this);
  }
  return ProcessDocString(PYPA_PTR_CAST(DocString, items_list[0]));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
