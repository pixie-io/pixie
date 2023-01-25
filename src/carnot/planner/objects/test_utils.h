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

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/planner/ast/ast_visitor.h"
#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/none_object.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

constexpr char kRegistryInfoProto[] = R"proto(
udtfs {
  name: "GetUDAList"
  executor: UDTF_ONE_KELVIN
  relation {
    columns {
      column_name: "name"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "return_type"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "args"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
udtfs {
  name: "GetUDFList"
  executor: UDTF_ONE_KELVIN
  relation {
    columns {
      column_name: "name"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "return_type"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "args"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
udtfs {
  name: "GetUDTFList"
  executor: UDTF_ONE_KELVIN
  relation {
    columns {
      column_name: "name"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "executor"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "init_args"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "output_relation"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
)proto";

class QLObjectTest : public OperatorTests {
 protected:
  void SetUp() override {
    OperatorTests::SetUp();

    info = std::make_shared<RegistryInfo>();
    udfspb::UDFInfo info_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(kRegistryInfoProto, &info_pb));
    PX_CHECK_OK(info->Init(info_pb));
    compiler_state = std::make_unique<CompilerState>(
        std::make_unique<RelationMap>(), /* sensitive_columns */ SensitiveColumnMap{}, info.get(),
        /* time_now */ 0,
        /* max_output_rows_per_table */ 0, "result_addr", "result_ssl_targetname",
        /* redaction_options */ RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});

    var_table = VarTable::Create();
    ast_visitor = ASTVisitorImpl::Create(graph.get(), var_table, &mutations_ir_,
                                         compiler_state.get(), &module_handler)
                      .ConsumeValueOrDie();
  }
  /**
   * @brief ParseScript takes a script and an initial variable state then parses
   * and walks through the AST given this initial variable state, updating the state
   * with whatever was walked through.
   *
   * If you're testing Objects, create a var_table, fill it with the object(s) you want
   * to test, then write a script interacting with those objects and store the result
   * into a variable.
   *
   * Then you can check if the ParseScript actually succeeds and what data is stored
   * in the var table.
   */
  Status ParseScript(const std::shared_ptr<VarTable>& var_table, const std::string& script) {
    Parser parser;
    PX_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(script));

    bool func_based_exec = false;
    absl::flat_hash_set<std::string> reserved_names;
    MutationsIR mutations_ir;
    ModuleHandler module_handler;
    PX_ASSIGN_OR_RETURN(
        auto ast_walker,
        ASTVisitorImpl::Create(graph.get(), var_table, &mutations_ir, compiler_state.get(),
                               &module_handler, func_based_exec, reserved_names));

    return ast_walker->ProcessModuleNode(ast);
  }

  StatusOr<QLObjectPtr> ParseExpression(const std::string& expr) {
    std::string var = "sp";
    std::string script =
        absl::Substitute("$0 = $1", var, std::string(absl::StripLeadingAsciiWhitespace(expr)));
    PX_RETURN_IF_ERROR(ParseScript(var_table, script));
    return var_table->Lookup(var);
  }

  ArgMap MakeArgMap(const std::vector<std::pair<std::string, IRNode*>>& kwargs,
                    const std::vector<IRNode*>& args) {
    std::vector<NameToNode> converted_kwargs;
    std::vector<QLObjectPtr> converted_args;
    for (const auto& p : kwargs) {
      converted_kwargs.push_back(
          {p.first, QLObject::FromIRNode(compiler_state.get(), p.second, ast_visitor.get())
                        .ConsumeValueOrDie()});
    }
    for (IRNode* node : args) {
      converted_args.push_back(
          QLObject::FromIRNode(compiler_state.get(), node, ast_visitor.get()).ConsumeValueOrDie());
    }
    return ArgMap{converted_kwargs, converted_args};
  }

  QLObjectPtr FromIRNode(IRNode* node) {
    if (Match(node, Operator())) {
      return Dataframe::Create(compiler_state.get(), static_cast<OperatorIR*>(node),
                               ast_visitor.get())
          .ConsumeValueOrDie();
    } else if (Match(node, Expression())) {
      return ExprObject::Create(static_cast<ExpressionIR*>(node), ast_visitor.get())
          .ConsumeValueOrDie();
    }
    CHECK(false) << node->CreateIRNodeError("Could not create QL object from IRNode of type $0",
                                            node->type_string())
                        .ToString();
  }

  QLObjectPtr ToQLObject(IRNode* node) {
    return QLObject::FromIRNode(compiler_state.get(), node, ast_visitor.get()).ConsumeValueOrDie();
  }

  std::unique_ptr<CompilerState> compiler_state = nullptr;
  std::shared_ptr<RegistryInfo> info = nullptr;
  std::shared_ptr<ASTVisitor> ast_visitor = nullptr;
  ModuleHandler module_handler;
  MutationsIR mutations_ir_;
  std::shared_ptr<VarTable> var_table;
};

StatusOr<QLObjectPtr> NoneObjectFunc(const pypa::AstPtr&, const ParsedArgs&, ASTVisitor* visitor) {
  return StatusOr<QLObjectPtr>(std::make_shared<NoneObject>(visitor));
}

std::string PrintObj(QLObjectPtr obj) {
  switch (obj->type()) {
    case QLObjectType::kList: {
      auto list = std::static_pointer_cast<CollectionObject>(obj);
      std::vector<std::string> items;
      for (auto item : list->items()) {
        items.push_back(PrintObj(item));
      }
      return absl::Substitute("[$0]", absl::StrJoin(items, ","));
    }
    case QLObjectType::kDict: {
      auto dict = std::static_pointer_cast<DictObject>(obj);
      std::vector<std::string> items;
      for (const auto& [i, key] : Enumerate(dict->keys())) {
        auto value = dict->values()[i];
        items.push_back(absl::Substitute("'$0', $1", PrintObj(key), PrintObj(value)));
      }
      return absl::Substitute("[$0]", absl::StrJoin(items, ","));
    }
    default:
      return std::string(obj->type_descriptor().name());
  }
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
