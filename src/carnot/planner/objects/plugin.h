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
#include <absl/container/flat_hash_set.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planpb/plan.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
class PluginModule : public QLObject {
 public:
  inline static constexpr char kPluginModule[] = "plugin";
  static constexpr TypeDescriptor PluginModuleType = {
      /* name */ kPluginModule,
      /* type */ QLObjectType::kModule,
  };
  static StatusOr<std::shared_ptr<PluginModule>> Create(PluginConfig* plugin_config,
                                                        ASTVisitor* ast_visitor, IR* graph);

  inline static constexpr char kStartTimeOpID[] = "start_time";
  inline static constexpr char kStartTimeDocstring[] = R"doc(
  Holds the start_time value according to the plugin_config. Will throw an error if no config is specified.
  )doc";
  inline static constexpr char kEndTimeOpID[] = "end_time";
  inline static constexpr char kEndTimeDocstring[] = R"doc(
  Holds the end_time value according to the plugin_config. Will throw an error if no config is specified.
  )doc";

 protected:
  PluginModule(PluginConfig* plugin_config, IR* graph, ASTVisitor* ast_visitor)
      : QLObject(PluginModuleType, ast_visitor), plugin_config_(plugin_config), graph_(graph) {}
  StatusOr<QLObjectPtr> GetAttributeImpl(const pypa::AstPtr& ast,
                                         std::string_view name) const override;

  bool HasNonMethodAttribute(std::string_view) const override { return true; }

 private:
  PluginConfig* plugin_config_ = nullptr;
  IR* graph_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
