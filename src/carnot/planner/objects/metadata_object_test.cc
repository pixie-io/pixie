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

#include <memory>
#include <string>

#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/metadata_object.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using MetadataObjectTest = QLObjectTest;

TEST_F(MetadataObjectTest, SubscriptWithString) {
  MemorySourceIR* src = MakeMemSource();
  auto metadata_or_s = MetadataObject::Create(src, ast_visitor.get());
  ASSERT_OK(metadata_or_s);
  auto metadata = metadata_or_s.ConsumeValueOrDie();

  ASSERT_TRUE(metadata->HasSubscriptMethod());
  std::shared_ptr<FuncObject> func = metadata->GetSubscriptMethod().ConsumeValueOrDie();

  auto func_result_or_s = func->Call(MakeArgMap({}, {MakeString("service")}), ast);
  ASSERT_OK(func_result_or_s);

  auto func_result = func_result_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_result->type_descriptor().type() == QLObjectType::kExpr);
  auto metadata_expr = static_cast<ExprObject*>(func_result.get());
  ASSERT_TRUE(metadata_expr->HasNode());
  ASSERT_MATCH(metadata_expr->node(), Metadata());
  auto metadata_node = static_cast<MetadataIR*>(metadata_expr->node());
  EXPECT_EQ(metadata_node->name(), "service");
}

TEST_F(MetadataObjectTest, ErrorsOnSubscriptWithNonString) {
  MemorySourceIR* src = MakeMemSource();
  auto metadata_or_s = MetadataObject::Create(src, ast_visitor.get());
  ASSERT_OK(metadata_or_s);
  auto metadata = metadata_or_s.ConsumeValueOrDie();

  ASSERT_TRUE(metadata->HasSubscriptMethod());
  std::shared_ptr<FuncObject> func = metadata->GetSubscriptMethod().ConsumeValueOrDie();

  auto func_result_or_s = func->Call(MakeArgMap({}, {MakeInt(2)}), ast);
  ASSERT_NOT_OK(func_result_or_s);
  EXPECT_THAT(func_result_or_s.status(),
              HasCompilerError("Expected arg 'key' as type 'String', received 'Int"));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
