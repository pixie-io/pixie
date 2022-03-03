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

#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

using CollectionTest = QLObjectTest;

TEST_F(CollectionTest, GetItemTest) {
  auto element = ParseExpression("['a', 'b', 1][1]").ConsumeValueOrDie();

  ASSERT_TRUE(ExprObject::IsExprObject(element));
  auto expr = static_cast<ExprObject*>(element.get());
  ASSERT_TRUE(StringIR::NodeMatches(expr->expr()));
  EXPECT_EQ(static_cast<StringIR*>(expr->expr())->str(), "b");
}

TEST_F(CollectionTest, OutOfRangeError) {
  EXPECT_COMPILER_ERROR(ParseExpression("['a', 'b', 1][3]"), "list index out of range");
}

TEST_F(CollectionTest, WrongIndexTypeError) {
  EXPECT_COMPILER_ERROR(ParseExpression("['a', 'b', 1]['blah']"),
                        "list indices must be integers, not String");
}

// Other tests make sure List works, tuple test just in case.
TEST_F(CollectionTest, TupleIndex) {
  auto element = ParseExpression("('a', 'b', 1)[1]").ConsumeValueOrDie();

  ASSERT_TRUE(ExprObject::IsExprObject(element));
  auto expr = static_cast<ExprObject*>(element.get());
  ASSERT_TRUE(StringIR::NodeMatches(expr->expr()));
  EXPECT_EQ(static_cast<StringIR*>(expr->expr())->str(), "b");
}

// Test that Subscript Method can be saved even after List is gone.
TEST_F(CollectionTest, SaveSubscriptMethodToUseElsewhere) {
  // The object where we will save the get_item.
  std::shared_ptr<FuncObject> subscript_fn;
  {
    auto list = ParseExpression("('a', 'b', 1)").ConsumeValueOrDie();
    subscript_fn = list->GetSubscriptMethod().ConsumeValueOrDie();
  }
  // Test 2, we can call outside the scope and get the same result, even though the list is
  // deallocated.
  auto element = subscript_fn->Call({{}, {ToQLObject(MakeInt(1))}}, ast).ConsumeValueOrDie();
  auto expr = static_cast<ExprObject*>(element.get());
  EXPECT_EQ(static_cast<StringIR*>(expr->expr())->str(), "b");
}

TEST_F(CollectionTest, ObjectAsCollectionWithCollection) {
  auto list = ParseExpression("('a', 'b', 'c')").ConsumeValueOrDie();
  std::vector<QLObjectPtr> objects = ObjectAsCollection(list);
  EXPECT_EQ(objects.size(), 3);
  std::vector<std::string> object_strings;
  for (const auto& o : objects) {
    object_strings.push_back(GetArgAs<StringIR>(o, "arg").ConsumeValueOrDie()->str());
  }
  EXPECT_THAT(object_strings, ElementsAre("a", "b", "c"));
}

TEST_F(CollectionTest, ObjectAsCollectionWithNonCollection) {
  std::vector<QLObjectPtr> objects = ObjectAsCollection(ParseExpression("'a'").ConsumeValueOrDie());
  EXPECT_EQ(objects.size(), 1);
  std::vector<std::string> object_strings;
  for (const auto& o : objects) {
    object_strings.push_back(GetArgAs<StringIR>(o, "arg").ConsumeValueOrDie()->str());
  }
  EXPECT_THAT(object_strings, ElementsAre("a"));
}
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
