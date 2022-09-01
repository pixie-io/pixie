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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/ir/ast_utils.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace planner {

struct AstToStringTestCase {
  std::string name;
  std::string pxl;
};
class AstToStringTest : public ::testing::Test,
                        public ::testing::WithParamInterface<AstToStringTestCase> {};

TEST_P(AstToStringTest, GetPxDisplayLines) {
  Parser parser;
  // Disable docstrings for the test, otherwise a string as an expression would be considered a docstring.
  ASSERT_OK_AND_ASSIGN(auto ast, parser.Parse(GetParam().pxl, /* parse_doc_strings */ false));
  ASSERT_EQ(ast->body->items.size(), 1);
  auto stmt = ast->body->items[0];
  ASSERT_EQ(stmt->type, pypa::AstType::ExpressionStatement) << GetAstTypeName(stmt->type);
  ASSERT_OK_AND_EQ(AstToString(PYPA_PTR_CAST(ExpressionStatement, stmt)->expr), GetParam().pxl);
}

INSTANTIATE_TEST_SUITE_P(
    AstToStringTestSuite, AstToStringTest,
    ::testing::ValuesIn(std::vector<AstToStringTestCase>{{"bin_op", "b + c"},
                                                         {"bool_op", "b and c"},
                                                         {"unary_op", "-1"},
                                                         {"attribute", "px.display"},
                                                         {"attribute_call", "px.display(blah)"},
                                                         {"dict", "{'a': 1, 'b': 2}"},
                                                         {"list", "[1, 2, 3]"},
                                                         {"tuple", "('a', 'b', 'c')"},
                                                         {"int", "1"},
                                                         {"float", "1.1"},
                                                         {"str", "'a'"},
                                                         {"subscript", "df['subscript']"}}),
    [](const ::testing::TestParamInfo<AstToStringTestCase>& info) { return info.param.name; });

}  // namespace planner
}  // namespace carnot
}  // namespace px
