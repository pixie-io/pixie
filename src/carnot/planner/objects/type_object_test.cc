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
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/test_utils.h"
#include "src/carnot/planner/objects/type_object.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ElementsAre;
class TypeObjectTest : public QLObjectTest {};
TEST_F(TypeObjectTest, NodeMatches) {
  std::shared_ptr<TypeObject> type =
      TypeObject::Create(IRNodeType::kString, ast_visitor.get()).ConsumeValueOrDie();
  EXPECT_OK(type->NodeMatches(MakeString("blah")));
  auto match = type->NodeMatches(MakeInt(123));
  EXPECT_NOT_OK(match);
  EXPECT_THAT(match.status(), HasCompilerError("Expected 'string', received 'int64'"));
}

using CastHandlerTest = TypeObjectTest;
TEST_F(CastHandlerTest, CastTest) {
  ExpressionIR* expr = MakeString("test");
  auto expr_obj = ExprObject::Create(expr, ast_visitor.get()).ConsumeValueOrDie();
  ParsedArgs args;
  args.AddArg("expr", expr_obj);
  auto status =
      CastHandler::Eval(types::STRING, types::ST_SERVICE_NAME, ast, args, ast_visitor.get());
  ASSERT_OK(status);

  std::shared_ptr<QLObject> obj = status.ConsumeValueOrDie();
  ASSERT_EQ(expr_obj, obj);

  // Check to make sure that the output has a type cast.
  ASSERT_TRUE(expr->HasTypeCast());
  EXPECT_EQ(*ValueType::Create(types::STRING, types::ST_SERVICE_NAME), *expr->type_cast());
}
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
