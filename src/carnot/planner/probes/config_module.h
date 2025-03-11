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

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/probes/probes.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
class ConfigModule : public QLObject {
 public:
  // Constant for the modules.
  inline static constexpr char kConfigModuleObjName[] = "pxconfig";
  static constexpr TypeDescriptor PxConfigModuleType = {
      /* name */ kConfigModuleObjName,
      /* type */ QLObjectType::kModule,
  };
  static StatusOr<std::shared_ptr<ConfigModule>> Create(MutationsIR* mutations_ir,
                                                        ASTVisitor* ast_visitor);

  // Constants for functions of pxtrace.
  inline static constexpr char kSetAgentConfigOpID[] = "set_agent_config";
  inline static constexpr char kSetAgentConfigOpDocstring[] = R"doc(
  Sets a particular value for a the key in the config on a particular agent.

  Args:
    agent_pod_name (int): The pod name of the Pixie agent to target for the config update.
    key (str): The key of the config to set. Can specify nested attributes as so
      `toplevel.midlevel.desired_key`.
    value (str): The value to set for the corresponding key.

  )doc";

 protected:
  explicit ConfigModule(MutationsIR* mutations_ir, ASTVisitor* ast_visitor)
      : QLObject(PxConfigModuleType, ast_visitor), mutations_ir_(mutations_ir) {}
  Status Init();

 private:
  MutationsIR* mutations_ir_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
