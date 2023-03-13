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

#include "src/carnot/planner/otel_generator/otel_generator.h"

#include <memory>
#include <utility>

#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/ast_utils.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/shared/scriptspb/scripts.pb.h"

namespace px {
namespace carnot {
namespace planner {

using table_store::schemapb::Schema;

StatusOr<std::string> OTelGenerator::GetUnusedVarName(CompilerState* compiler_state,
                                                      const std::string& script,
                                                      const std::string& base_name) {
  Parser parser;
  PX_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(script));

  bool func_based_exec = false;
  absl::flat_hash_set<std::string> reserved_names;
  compiler::ModuleHandler module_handler;
  compiler::MutationsIR mutations_ir;
  std::shared_ptr<IR> ir = std::make_shared<IR>();
  auto var_table = compiler::VarTable::Create();
  PX_ASSIGN_OR_RETURN(auto ast_walker,
                      compiler::ASTVisitorImpl::Create(
                          ir.get(), var_table, &mutations_ir, compiler_state, &module_handler,
                          func_based_exec, absl::flat_hash_set<std::string>{}));

  PX_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  auto cur_name = base_name;
  int64_t counter = 0;
  while (var_table->HasVariable(cur_name)) {
    if (counter > 1000) {
      return error::InvalidArgument("Gave up searching for an unused variable name with base: $0",
                                    base_name);
    }
    cur_name = absl::Substitute("$0_$1", base_name, counter);
    ++counter;
  }

  return cur_name;
}

StatusOr<std::vector<DisplayLine>> OTelGenerator::GetPxDisplayLines(const std::string& script) {
  // Parse the script into an ast.
  // Check for any calls to px.display().
  // Make sure the arguments are expected and valid.
  Parser parser;
  PX_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(script));
  std::vector<DisplayLine> display_lines;

  std::vector<std::string> all_script_lines = absl::StrSplit(script, '\n');

  // We are strictly looking for px.display calls here.
  for (const auto& [i, stmt] : Enumerate(ast->body->items)) {
    if (stmt->type != pypa::AstType::ExpressionStatement) {
      continue;
    }
    auto expr = PYPA_PTR_CAST(ExpressionStatement, stmt)->expr;
    if (expr->type != pypa::AstType::Call) {
      continue;
    }
    auto call = PYPA_PTR_CAST(Call, expr);
    // We check if the function is an attribute px.display.
    if (call->function->type != pypa::AstType::Attribute) {
      continue;
    }
    auto function = PYPA_PTR_CAST(Attribute, call->function);
    if (function->value->type != pypa::AstType::Name) {
      continue;
    }

    auto fn_value = PYPA_PTR_CAST(Name, function->value);
    if (fn_value->id != "px") {
      continue;
    }

    if (function->attribute->type != pypa::AstType::Name) {
      continue;
    }

    auto fn_attribute = PYPA_PTR_CAST(Name, function->attribute);
    if (fn_attribute->id != "display") {
      continue;
    }
    // Everything after this will set expectations for the arguments for px.display.
    // If anything is not as expected, we will return an error instead of skipping.

    // We expect two arguments, the first being a dataframe expression, and the second being a
    // string.

    if (call->arguments.size() != 2) {
      return CreateAstError(call, "expected two arguments to px.display, got $0",
                            call->arguments.size());
    }

    if (call->arguments[1]->type != pypa::AstType::Str) {
      return CreateAstError(call->arguments[1],
                            "expected second argument to px.display to be a string, received a $0",
                            GetAstTypeName(call->arguments[1]->type));
    }

    auto first_line = stmt->line - 1;
    int64_t last_line;
    if (i == ast->body->items.size() - 1) {
      last_line = static_cast<int64_t>(all_script_lines.size()) - 1;
    } else {
      // Here we assign the last line to be the line before the next statement.
      // We subtract 2 from the line number of the next statement for the following reasons:
      // Ast line numbers are 1-indexed (first line is 1), but GetPxDisplayLines is 0-indexed.
      // So we first have to subtract 1 to convert 1-index to 0-index, then we
      // subtract 1 again to get the line before the next statement.
      last_line = ast->body->items[i + 1]->line - 2;
    }

    auto table_name = PYPA_PTR_CAST(Str, call->arguments[1])->value;

    // Somehow parse this from the string.
    PX_ASSIGN_OR_RETURN(auto dataframe_argument, AstToString(call->arguments[0]));

    auto statement_str = absl::StrJoin(all_script_lines.begin() + first_line,
                                       all_script_lines.begin() + last_line + 1, "\n");

    display_lines.push_back(DisplayLine{
        statement_str,
        table_name,
        dataframe_argument,
        first_line,
        last_line,
    });
  }

  return display_lines;
}

StatusOr<absl::flat_hash_map<std::string, ::px::table_store::schemapb::Relation>>
OTelGenerator::CalculateOutputSchemas(compiler::Compiler* compiler, CompilerState* compiler_state,
                                      const std::string& pxl_script) {
  PX_ASSIGN_OR_RETURN(std::shared_ptr<IR> single_node_plan,
                      compiler->CompileToIR(pxl_script, compiler_state, {}));

  absl::flat_hash_map<std::string, ::px::table_store::schemapb::Relation> output_schemas;
  for (const auto& n : single_node_plan->FindNodesThatMatch(ExternalGRPCSink())) {
    auto gsink = static_cast<GRPCSinkIR*>(n);
    PX_ASSIGN_OR_RETURN(auto relation, gsink->resolved_table_type()->ToRelation());
    table_store::schemapb::Relation* relation_pb = &output_schemas[gsink->name()];
    PX_RETURN_IF_ERROR(relation.ToProto(relation_pb));
  }
  return output_schemas;
}

static std::regex kIndentRegex("^\\s*");
class DataFrameCallModifier {
 public:
  /**
   * @brief InAstSuite returns the DataFrameCalls that are inside this ast suite.
   *
   * We have this special interface because for reuse in FunctionDefs, where the body
   * of the functions are stored in a separate Suite object and the entire definition is
   * a single statement in the parent suite.
   *
   * @param body the current body of statements we iterate through.
   * @param all_script_lines a vector of the original script lines.
   * @param last_body_line the index of the last line for the body of the suite. If the suite is the
   * top level then the last_body_line index is the length of the script - 1. Otherwise, it's
   * something like the last line of a function body.
   * @return StatusOr<std::vector<DataFrameCall>>
   */
  static StatusOr<std::vector<DataFrameCall>> InAstSuite(
      const std::shared_ptr<pypa::AstSuite>& body, const std::vector<std::string>& all_script_lines,
      int64_t last_body_line) {
    std::vector<DataFrameCall> dataframe_calls;
    // We are strictly looking for px.display calls here.
    for (const auto& [i, statement] : Enumerate(body->items)) {
      int64_t first_statement_line = statement->line - 1;
      int64_t last_statement_line = last_body_line;
      if (i < body->items.size() - 1) {
        // Here we assign the last line to be the line before the next statement.
        // We subtract 2 from the line number of the next statement for the following reasons:
        // Ast line numbers are 1-indexed (first line is 1), but DataFrameCall is 0-indexed.
        // So we first have to subtract 1 to convert 1-index to 0-index, then we
        // subtract 1 again to get the line before the next statement.
        last_statement_line = body->items[i + 1]->line - 2;
      }
      std::string original_line =
          absl::StrJoin(all_script_lines.begin() + first_statement_line,
                        all_script_lines.begin() + last_statement_line + 1, "\n");

      // Extract the indent from the original line.
      std::smatch match;
      std::string indent;
      if (std::regex_search(original_line, match, kIndentRegex)) {
        indent = match[0].str();
      }

      switch (statement->type) {
        case pypa::AstType::ExpressionStatement: {
          auto expr_statement = PYPA_PTR_CAST(ExpressionStatement, statement);
          PX_ASSIGN_OR_RETURN(auto mod_expression, InExpression(expr_statement->expr));

          if (mod_expression.has_value()) {
            expr_statement->expr = mod_expression.value();
            PX_ASSIGN_OR_RETURN(auto modified_line, AstToString(expr_statement));
            dataframe_calls.push_back(DataFrameCall{
                original_line,
                absl::StrCat(indent, modified_line),
                first_statement_line,
                last_statement_line,
            });
          }
          break;
        }
        case pypa::AstType::Assign: {
          bool has_modified = false;
          auto assign = PYPA_PTR_CAST(Assign, statement);
          for (const auto& [i, target] : Enumerate(assign->targets)) {
            PX_ASSIGN_OR_RETURN(auto mod_target, InExpression(target));
            if (!mod_target.has_value()) {
              continue;
            }
            has_modified = true;
            assign->targets[i] = mod_target.value();
          }
          PX_ASSIGN_OR_RETURN(auto mod_value, InExpression(assign->value));
          if (mod_value.has_value()) {
            has_modified = true;
            assign->value = mod_value.value();
          }
          if (has_modified) {
            PX_ASSIGN_OR_RETURN(auto modified_line, AstToString(assign));
            dataframe_calls.push_back(DataFrameCall{
                original_line,
                absl::StrCat(indent, modified_line),
                first_statement_line,
                last_statement_line,
            });
          }
          break;
        }
        case pypa::AstType::FunctionDef: {
          auto function_def = PYPA_PTR_CAST(FunctionDef, statement);
          if (function_def->body->type != pypa::AstType::Suite) {
            return CreateAstError(function_def->body, "function body of type $0 not allowed",
                                  GetAstTypeName(function_def->body->type));
          }
          PX_ASSIGN_OR_RETURN(auto in_fn_dataframe_calls,
                              InAstSuite(PYPA_PTR_CAST(Suite, function_def->body), all_script_lines,
                                         last_statement_line));
          dataframe_calls.insert(dataframe_calls.end(), in_fn_dataframe_calls.begin(),
                                 in_fn_dataframe_calls.end());
          break;
        }
        case pypa::AstType::Return: {
          auto return_statement = PYPA_PTR_CAST(Return, statement);
          PX_ASSIGN_OR_RETURN(auto mod_value, InExpression(return_statement->value));
          if (mod_value.has_value()) {
            return_statement->value = mod_value.value();
            PX_ASSIGN_OR_RETURN(auto modified_line, AstToString(return_statement));
            dataframe_calls.push_back(DataFrameCall{
                original_line,
                absl::StrCat(indent, modified_line),
                first_statement_line,
                last_statement_line,
            });
          }
          break;
        }
        case pypa::AstType::Import: {
          break;
        }
        default:
          return CreateAstError(statement,
                                "Unexpected statement type for the OpenTelemetry generator: $0",
                                GetAstTypeName(statement->type));
      }
    }

    return dataframe_calls;
  }

 private:
  static bool IsPxDataFrame(const pypa::AstExpr& function) {
    if (function->type != pypa::AstType::Attribute) {
      return false;
    }
    auto attribute = PYPA_PTR_CAST(Attribute, function);
    if (attribute->value->type != pypa::AstType::Name) {
      return false;
    }

    auto fn_value = PYPA_PTR_CAST(Name, attribute->value);
    if (fn_value->id != "px") {
      return false;
    }

    if (attribute->attribute->type != pypa::AstType::Name) {
      return false;
    }

    auto fn_attribute = PYPA_PTR_CAST(Name, attribute->attribute);
    return fn_attribute->id == "DataFrame";
  }

  static std::shared_ptr<pypa::AstExpression> CreatePluginVarRef(
      const std::string& plugin_var_name) {
    auto px = std::make_shared<pypa::AstName>();
    px->id = "px";
    auto plugin = std::make_shared<pypa::AstName>();
    plugin->id = "plugin";

    auto parent_attribute = std::make_shared<pypa::AstAttribute>();
    parent_attribute->value = px;
    parent_attribute->attribute = plugin;

    auto attribute = std::make_shared<pypa::AstAttribute>();
    attribute->value = parent_attribute;
    auto base_name = std::make_shared<pypa::AstName>();
    base_name->id = plugin_var_name;
    attribute->attribute = base_name;
    return attribute;
  }

  using ModifiedExpression = std::optional<std::shared_ptr<pypa::AstExpression>>;
  static ModifiedExpression ModifyDataFrameCall(const std::shared_ptr<pypa::AstCall>& df_call) {
    std::vector<std::string> arg_names{"table", "select", "start_time", "end_time"};
    absl::flat_hash_map<std::string, std::shared_ptr<pypa::AstExpression>> arg_map;
    for (const auto& [i, arg] : Enumerate(df_call->arguments)) {
      arg_map[arg_names[i]] = arg;
    }
    for (auto& kw : df_call->keywords) {
      std::string key = GetNameAsString(kw->name);
      arg_map[key] = kw->value;
    }
    // Overwrite the start_time and end_time arguments.
    arg_map["start_time"] = CreatePluginVarRef("start_time");
    arg_map["end_time"] = CreatePluginVarRef("end_time");
    df_call->keywords.clear();
    df_call->arguments.clear();
    for (const auto& name : arg_names) {
      if (name == "table") {
        df_call->arguments.push_back(arg_map[name]);
        continue;
      }
      if (!arg_map.contains(name)) {
        continue;
      }
      auto kw = std::make_shared<pypa::AstKeyword>();
      auto kw_name = std::make_shared<pypa::AstName>();
      kw_name->id = name;
      kw->name = kw_name;
      kw->value = arg_map[name];
      df_call->keywords.push_back(kw);
    }
    return ModifiedExpression{df_call};
  }

  static StatusOr<ModifiedExpression> InExpression(
      const std::shared_ptr<pypa::AstExpression>& expr) {
    switch (expr->type) {
      case pypa::AstType::Call: {
        // If the function name is `px.DataFrame` then we need to modify the arguments.
        auto call = PYPA_PTR_CAST(Call, expr);
        bool has_modified = false;
        if (IsPxDataFrame(call->function)) {
          return ModifyDataFrameCall(call);
        }
        // Otherwise we modify the Call function, arguments, etc.
        PX_ASSIGN_OR_RETURN(auto mod_fn, InExpression(call->function));
        if (mod_fn.has_value()) {
          has_modified = true;
          call->function = mod_fn.value();
        }
        for (const auto& [i, arg] : Enumerate(call->arguments)) {
          PX_ASSIGN_OR_RETURN(auto mod_arg, InExpression(arg));
          if (mod_arg.has_value()) {
            has_modified = true;
            call->arguments[i] = mod_arg.value();
          }
        }
        if (has_modified) {
          return ModifiedExpression{call};
        } else {
          return ModifiedExpression{};
        }
      }
      case pypa::AstType::Attribute: {
        bool has_modified = false;
        auto attr = PYPA_PTR_CAST(Attribute, expr);
        PX_ASSIGN_OR_RETURN(auto mod_value, InExpression(attr->value));
        if (mod_value.has_value()) {
          attr->value = mod_value.value();
          has_modified = true;
        }
        PX_ASSIGN_OR_RETURN(auto mod_id, InExpression(attr->value));
        if (mod_id.has_value()) {
          attr->value = mod_id.value();
          has_modified = true;
        }
        if (has_modified) {
          return ModifiedExpression{attr};
        }
        return ModifiedExpression{};
      }
      case pypa::AstType::List: {
        bool has_modified = false;
        auto list = PYPA_PTR_CAST(List, expr);
        for (const auto& [i, elm] : Enumerate(list->elements)) {
          PX_ASSIGN_OR_RETURN(auto mod_elm, InExpression(elm));
          if (mod_elm.has_value()) {
            has_modified = true;
            list->elements[i] = mod_elm.value();
          }
        }
        if (has_modified) {
          return ModifiedExpression{list};
        }
        return ModifiedExpression{};
      }
      case pypa::AstType::Tuple: {
        bool has_modified = false;
        auto tuple = PYPA_PTR_CAST(Tuple, expr);
        for (const auto& [i, elm] : Enumerate(tuple->elements)) {
          PX_ASSIGN_OR_RETURN(auto mod_elm, InExpression(elm));
          if (mod_elm.has_value()) {
            has_modified = true;
            tuple->elements[i] = mod_elm.value();
          }
        }
        if (has_modified) {
          return ModifiedExpression{tuple};
        }
        return ModifiedExpression{};
      }
      case pypa::AstType::Subscript: {
        bool has_modified = false;
        auto subscript = PYPA_PTR_CAST(Subscript, expr);
        PX_ASSIGN_OR_RETURN(auto mod_value, InExpression(subscript->value));

        if (mod_value.has_value()) {
          has_modified = true;
          subscript->value = mod_value.value();
        }

        if (subscript->slice->type != pypa::AstType::Index) {
          return CreateAstError(subscript->slice, "'$0' object must be an index",
                                GetAstTypeName(subscript->slice->type));
        }
        auto index = PYPA_PTR_CAST(Index, subscript->slice);
        PX_ASSIGN_OR_RETURN(auto mod_slice, InExpression(index->value));
        if (mod_slice.has_value()) {
          has_modified = true;
          index->value = mod_slice.value();
        }
        if (has_modified) {
          return ModifiedExpression{subscript};
        }
        return ModifiedExpression{};
      }
      // The following are expressions that we don't need to modify.
      case pypa::AstType::Alias:
      case pypa::AstType::Arg:
      case pypa::AstType::Arguments:
      case pypa::AstType::Assert:
      case pypa::AstType::Assign:
      case pypa::AstType::AugAssign:
      case pypa::AstType::BinOp:
      case pypa::AstType::Bool:
      case pypa::AstType::BoolOp:
      case pypa::AstType::Break:
      case pypa::AstType::ClassDef:
      case pypa::AstType::Compare:
      case pypa::AstType::Complex:
      case pypa::AstType::Comprehension:
      case pypa::AstType::Continue:
      case pypa::AstType::Delete:
      case pypa::AstType::Dict:
      case pypa::AstType::DictComp:
      case pypa::AstType::DocString:
      case pypa::AstType::Ellipsis:
      case pypa::AstType::EllipsisObject:
      case pypa::AstType::Except:
      case pypa::AstType::Exec:
      case pypa::AstType::Expression:
      case pypa::AstType::ExpressionStatement:
      case pypa::AstType::ExtSlice:
      case pypa::AstType::For:
      case pypa::AstType::FunctionDef:
      case pypa::AstType::Generator:
      case pypa::AstType::Global:
      case pypa::AstType::If:
      case pypa::AstType::IfExpr:
      case pypa::AstType::Import:
      case pypa::AstType::ImportFrom:
      case pypa::AstType::Index:
      case pypa::AstType::Invalid:
      case pypa::AstType::Keyword:
      case pypa::AstType::Lambda:
      case pypa::AstType::ListComp:
      case pypa::AstType::Module:
      case pypa::AstType::Name:
      case pypa::AstType::None:
      case pypa::AstType::Number:
      case pypa::AstType::Pass:
      case pypa::AstType::Print:
      case pypa::AstType::Raise:
      case pypa::AstType::Repr:
      case pypa::AstType::Return:
      case pypa::AstType::Set:
      case pypa::AstType::SetComp:
      case pypa::AstType::Slice:
      case pypa::AstType::SliceType:
      case pypa::AstType::Statement:
      case pypa::AstType::Str:
      case pypa::AstType::Suite:
      case pypa::AstType::TryExcept:
      case pypa::AstType::TryFinally:
      case pypa::AstType::UnaryOp:
      case pypa::AstType::While:
      case pypa::AstType::With:
      case pypa::AstType::Yield:
      case pypa::AstType::YieldExpr:
        return ModifiedExpression{};
      default:
        return CreateAstError(expr,
                              "Unexpected expression type for the OpenTelemetry generator: $0",
                              GetAstTypeName(expr->type));
    }
    return ModifiedExpression{};
  }
};

StatusOr<std::vector<DataFrameCall>> OTelGenerator::ReplaceDataFrameTimes(
    const std::string& script) {
  Parser parser;
  PX_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(script));

  std::vector<std::string> all_script_lines = absl::StrSplit(script, '\n');

  return DataFrameCallModifier::InAstSuite(ast->body, all_script_lines,
                                           static_cast<int64_t>(all_script_lines.size() - 1));
}

std::string IndentBlock(const std::string& block, const std::string& indent) {
  return indent + absl::StrJoin(absl::StrSplit(block, '\n'), "\n" + indent);
}

const char kGaugeFormat[] = R"pxl(px.otel.metric.Gauge(
  name='$0',
  description='',
  value=$1,
))pxl";

StatusOr<std::string> OTelGenerator::RelationToOTelExport(
    const std::string& table_name, const std::string& unique_df_name,
    const px::table_store::schemapb::Relation& relation) {
  std::vector<std::string> resource_fields;
  std::vector<std::string> data_exports;
  std::string service_col = "";
  bool has_time_column = false;
  for (const auto& column : relation.columns()) {
    if (column.column_semantic_type() == ::px::types::ST_DURATION_NS_QUANTILES ||
        column.column_semantic_type() == ::px::types::ST_QUANTILES) {
      return error::InvalidArgument(
          "quantiles are not supported yet for generation of OTel export scripts");
    }
    if (column.column_name() == "time_") {
      if (column.column_type() != ::px::types::TIME64NS) {
        return error::InvalidArgument("time_ column must be of type TIME64NS, received $0",
                                      ToString(column.column_type()));
      }
      // The export script expects a time column to exist, but does not specify it.
      has_time_column = true;
      continue;
    }

    std::string name = table_name + "." + column.column_name();
    std::string df_col = unique_df_name + "." + column.column_name();

    if (column.column_semantic_type() == ::px::types::ST_SERVICE_NAME) {
      if (service_col == "" || column.column_name() == "service") {
        service_col = df_col;
      }
    }
    switch (column.column_type()) {
      case ::px::types::BOOLEAN:
      case ::px::types::STRING: {
        resource_fields.push_back(absl::Substitute("'$0': $1", name, df_col));
        break;
      }
      case ::px::types::INT64:
      case ::px::types::FLOAT64:
        data_exports.push_back(absl::Substitute(kGaugeFormat, name, df_col));
        break;
      case ::px::types::UINT128:
        return error::InvalidArgument(
            "column '$0' uses an unsupported type: UINT128. Please convert the column to a "
            "string",
            column.column_name());
      case ::px::types::TIME64NS:
        return error::InvalidArgument(
            "illegal column '$0' -> TIME64NS column not named 'time_' is ambiguous. Please file "
            "a "
            "feature request on GitHub if you have a clear use case for TIME64NS columns",
            column.column_name());
      case ::px::types::DATA_TYPE_UNKNOWN:
      case ::px::types::DataType_INT_MIN_SENTINEL_DO_NOT_USE_:
      case ::px::types::DataType_INT_MAX_SENTINEL_DO_NOT_USE_:
        return error::InvalidArgument("unsupported type $0", ToString(column.column_type()));
    }
  }
  if (!has_time_column) {
    return error::InvalidArgument("Table '$0' does not have a time_ column of TIME64NS type",
                                  table_name);
  }

  if (service_col == "") {
    return error::InvalidArgument(
        "Table '$0' does not have a service column. Make sure you create a service column ie "
        "`df.ctx['service']` and include it in any groupbys and joins",
        table_name);
  }

  if (data_exports.empty()) {
    return error::InvalidArgument(
        "Table '$0' does not have any INT64 or FLOAT64 that can be converted to OTel metrics",
        table_name);
  }

  resource_fields.push_back(absl::Substitute("'service.name': $0", service_col));

  std::string body = absl::Substitute("resource={\n$0\n},\ndata=[\n$1\n]",
                                      IndentBlock(absl::StrJoin(resource_fields, ",\n"), "  "),
                                      IndentBlock(absl::StrJoin(data_exports, ",\n"), "  "));
  return absl::Substitute("px.export($0, px.otel.Data(\n$1\n))", unique_df_name,
                          IndentBlock(body, "  "));
}

StatusOr<std::string> OTelGenerator::GenerateOTelScript(compiler::Compiler* compiler,
                                                        CompilerState* compiler_state,
                                                        const std::string& pxl_script) {
  PX_ASSIGN_OR_RETURN(auto schema,
                      OTelGenerator::CalculateOutputSchemas(compiler, compiler_state, pxl_script));
  if (schema.size() == 0) {
    return error::InvalidArgument("script does not have any output tables");
  }

  PX_ASSIGN_OR_RETURN(auto display_lines, OTelGenerator::GetPxDisplayLines(pxl_script));
  if (display_lines.size() != schema.size()) {
    return error::InvalidArgument("script has $0 output tables, but $1 px.display calls",
                                  schema.size(), display_lines.size());
  }

  // Verify that table names are fully unique.
  absl::flat_hash_set<std::string> table_names;
  for (const auto& display_call : display_lines) {
    if (table_names.contains(display_call.table_name)) {
      return error::InvalidArgument("duplicate table name. '$0' already in use",
                                    display_call.table_name);
    }
    table_names.insert(display_call.table_name);
  }

  PX_ASSIGN_OR_RETURN(auto unique_df_name,
                      OTelGenerator::GetUnusedVarName(compiler_state, pxl_script, "otel_df"));

  std::vector<std::string> all_script_lines = absl::StrSplit(pxl_script, '\n');
  int64_t prev_idx = 0;
  std::vector<std::string> blocks;
  for (const auto& [i, display_call] : Enumerate(display_lines)) {
    std::vector<std::string> out_lines;
    for (int64_t j = prev_idx; j <= display_call.line_number_end; ++j) {
      out_lines.push_back(all_script_lines[j]);
    }
    prev_idx = display_call.line_number_end + 1;
    out_lines.push_back(absl::Substitute("\n$0 = $1", unique_df_name, display_call.table_argument));

    if (!schema.contains(display_call.table_name)) {
      return error::InvalidArgument(
          "no relation generated for $0, likely that the table name in the px.display call is "
          "duplicated",
          display_call.table_name);
    }

    auto relation = schema.at(display_call.table_name);
    PX_ASSIGN_OR_RETURN(
        std::string export_statement,
        OTelGenerator::RelationToOTelExport(display_call.table_name, unique_df_name, relation));
    out_lines.push_back(export_statement);

    blocks.push_back(absl::StrJoin(out_lines, "\n"));
  }

  // Grab the content after the last display call and add it as the last block.
  if (prev_idx < static_cast<int64_t>(all_script_lines.size())) {
    blocks.push_back(
        absl::StrJoin(all_script_lines.begin() + prev_idx, all_script_lines.end(), "\n"));
  }

  std::string otel_script = absl::StrJoin(blocks, "\n\n");

  // Parse and replace the DataFrame calls with DataFrame calls that use plugin start/end variables.
  PX_ASSIGN_OR_RETURN(auto dataframe_calls, ReplaceDataFrameTimes(otel_script));

  std::vector<std::string> split_again = absl::StrSplit(otel_script, '\n');
  std::vector<std::string> out_lines;
  int64_t prev = 0;
  for (const auto& call : dataframe_calls) {
    out_lines.insert(out_lines.end(), split_again.begin() + prev,
                     split_again.begin() + call.line_number_start);
    prev = call.line_number_end + 1;
    out_lines.push_back(call.updated_line);
  }
  out_lines.insert(out_lines.end(), split_again.begin() + prev, split_again.end());

  return absl::StrJoin(out_lines, "\n");
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
