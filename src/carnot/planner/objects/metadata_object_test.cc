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

class MetadataObjectTest : public QLObjectTest {
 protected:
  void SetUp() override {
    QLObjectTest::SetUp();
    var_table->Add("ctx",
                   MetadataObject::Create(MakeMemSource(), ast_visitor.get()).ConsumeValueOrDie());
  }
};

TEST_F(MetadataObjectTest, SubscriptWithString) {
  ASSERT_OK_AND_ASSIGN(auto result, ParseExpression("ctx['service']"));

  ASSERT_TRUE(ExprObject::IsExprObject(result));
  auto metadata_expr = static_cast<ExprObject*>(result.get());
  ASSERT_MATCH(metadata_expr->expr(), Metadata());
  auto metadata_node = static_cast<MetadataIR*>(metadata_expr->expr());
  EXPECT_EQ(metadata_node->name(), "service");
}

TEST_F(MetadataObjectTest, ErrorInput) {
  // Non-string input.
  EXPECT_COMPILER_ERROR(ParseExpression("ctx[2]"),
                        "Expected arg 'key' as type 'String', received 'Int");
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
