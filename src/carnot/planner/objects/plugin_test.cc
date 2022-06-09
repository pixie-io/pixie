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

#include <absl/strings/ascii.h>
#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/plugin.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class PluginTest : public QLObjectTest {};

TEST_F(PluginTest, get_start_time) {
  PluginConfig plugin_config{1234, 5678};

  ASSERT_OK_AND_ASSIGN(auto plugin,
                       PluginModule::Create(&plugin_config, ast_visitor.get(), graph.get()));
  var_table->Add("plugin", plugin);

  ASSERT_OK_AND_ASSIGN(auto start_time_ns, ParseExpression("plugin.start_time"));
  auto start_time_expr = static_cast<ExprObject*>(start_time_ns.get());
  EXPECT_EQ(static_cast<TimeIR*>(start_time_expr->expr())->val(), 1234);
}

TEST_F(PluginTest, get_end_time) {
  PluginConfig plugin_config{1234, 5678};

  ASSERT_OK_AND_ASSIGN(auto plugin,
                       PluginModule::Create(&plugin_config, ast_visitor.get(), graph.get()));
  var_table->Add("plugin", plugin);

  ASSERT_OK_AND_ASSIGN(auto end_time_ns, ParseExpression("plugin.end_time"));
  auto end_time_expr = static_cast<ExprObject*>(end_time_ns.get());
  EXPECT_EQ(static_cast<TimeIR*>(end_time_expr->expr())->val(), 5678);
}

TEST_F(PluginTest, get_random) {
  PluginConfig plugin_config{1234, 5678};

  ASSERT_OK_AND_ASSIGN(auto plugin,
                       PluginModule::Create(&plugin_config, ast_visitor.get(), graph.get()));
  var_table->Add("plugin", plugin);

  EXPECT_THAT(ParseExpression("plugin.random").status(),
              HasCompilerError("plugin does not contain attribute 'random'"));
}

TEST_F(PluginTest, null_plugin_config_throws_compiler_error) {
  ASSERT_OK_AND_ASSIGN(auto plugin, PluginModule::Create(nullptr, ast_visitor.get(), graph.get()));
  var_table->Add("plugin", plugin);

  EXPECT_THAT(
      ParseExpression("plugin.start_time").status(),
      HasCompilerError("No plugin config found. Make sure the script is run in a plugin context."));
  EXPECT_THAT(
      ParseExpression("plugin.end_time").status(),
      HasCompilerError("No plugin config found. Make sure the script is run in a plugin context."));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
