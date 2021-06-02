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

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/resolve_metadata_property_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ResolveMetadataPropertyRuleTest = RulesTest;

TEST_F(ResolveMetadataPropertyRuleTest, basic) {
  auto metadata_name = "pod_name";
  MetadataIR* metadata_ir = MakeMetadataIR(metadata_name, /* parent_op_idx */ 0);
  MakeMap(MakeMemSource(), {{"md", metadata_ir}});

  EXPECT_FALSE(metadata_ir->has_property());

  ResolveMetadataPropertyRule rule(compiler_state_.get(), md_handler.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_TRUE(metadata_ir->has_property());
  EXPECT_EQ(MetadataType::POD_NAME, metadata_ir->property()->metadata_type());
  EXPECT_EQ(types::DataType::STRING, metadata_ir->property()->column_type());
}

TEST_F(ResolveMetadataPropertyRuleTest, noop) {
  auto metadata_name = "pod_name";
  MetadataIR* metadata_ir = MakeMetadataIR(metadata_name, /* parent_op_idx */ 0);
  MakeMap(MakeMemSource(), {{"md", metadata_ir}});

  EXPECT_FALSE(metadata_ir->has_property());
  MetadataProperty* property = md_handler->GetProperty(metadata_name).ValueOrDie();
  metadata_ir->set_property(property);

  ResolveMetadataPropertyRule rule(compiler_state_.get(), md_handler.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
