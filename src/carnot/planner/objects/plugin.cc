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

#include "src/carnot/planner/objects/plugin.h"

#include "src/carnot/planner/ir/int_ir.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/none_object.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<PluginModule>> PluginModule::Create(PluginConfig* plugin_config,
                                                             ASTVisitor* ast_visitor, IR* graph) {
  auto plugin_module =
      std::shared_ptr<PluginModule>(new PluginModule(plugin_config, graph, ast_visitor));

  // Add a place holder to store the docstring.
  auto start_time_placeholder = std::make_shared<NoneObject>(ast_visitor);
  PX_RETURN_IF_ERROR(start_time_placeholder->SetDocString(kStartTimeDocstring));
  PX_RETURN_IF_ERROR(plugin_module->AssignAttribute(kStartTimeOpID, start_time_placeholder));
  auto end_time_placeholder = std::make_shared<NoneObject>(ast_visitor);
  PX_RETURN_IF_ERROR(end_time_placeholder->SetDocString(kEndTimeDocstring));
  PX_RETURN_IF_ERROR(plugin_module->AssignAttribute(kEndTimeOpID, end_time_placeholder));
  return plugin_module;
}

StatusOr<QLObjectPtr> PluginModule::GetAttributeImpl(const pypa::AstPtr& ast,
                                                     std::string_view name) const {
  if (!plugin_config_) {
    return CreateError("No plugin config found. Make sure the script is run in a plugin context.");
  }
  if (name == kStartTimeOpID) {
    PX_ASSIGN_OR_RETURN(IntIR * start_time_int,
                        graph_->CreateNode<IntIR>(ast, plugin_config_->start_time_ns));
    return ExprObject::Create(start_time_int, ast_visitor());
  } else if (name == kEndTimeOpID) {
    PX_ASSIGN_OR_RETURN(IntIR * end_time_int,
                        graph_->CreateNode<IntIR>(ast, plugin_config_->end_time_ns));
    return ExprObject::Create(end_time_int, ast_visitor());
  }
  return QLObject::GetAttributeImpl(ast, name);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
