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

#include "src/carnot/planner/probes/config_module.h"
#include <sole.hpp>

#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/none_object.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<ConfigModule>> ConfigModule::Create(MutationsIR* mutations_ir,
                                                             ASTVisitor* ast_visitor) {
  auto config_module = std::shared_ptr<ConfigModule>(new ConfigModule(mutations_ir, ast_visitor));
  PX_RETURN_IF_ERROR(config_module->Init());
  return config_module;
}
StatusOr<QLObjectPtr> SetHandler(MutationsIR* mutations, const pypa::AstPtr& ast,
                                 const ParsedArgs& args, ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(auto agent_pod_name_ir, GetArgAs<StringIR>(ast, args, "agent_pod_name"));
  PX_ASSIGN_OR_RETURN(auto key_ir, GetArgAs<StringIR>(ast, args, "key"));
  PX_ASSIGN_OR_RETURN(auto val_ir, GetArgAs<StringIR>(ast, args, "value"));
  mutations->AddConfig(agent_pod_name_ir->str(), key_ir->str(), val_ir->str());
  return std::static_pointer_cast<QLObject>(std::make_shared<NoneObject>(ast, visitor));
}

Status ConfigModule::Init() {
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> set_fn,
      FuncObject::Create(kSetAgentConfigOpID, {"agent_pod_name", "key", "value"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(SetHandler, mutations_ir_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(set_fn->SetDocString(kSetAgentConfigOpDocstring));
  AddMethod(kSetAgentConfigOpID, set_fn);

  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
