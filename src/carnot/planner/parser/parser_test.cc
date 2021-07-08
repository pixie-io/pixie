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

#include <tuple>
#include <unordered_map>
#include <vector>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/parser/parser.h"

namespace px {
namespace carnot {
namespace planner {

class ParserTest : public ::testing::Test {};

// Test sh
TEST_F(ParserTest, AndExpressionFailsGracefully) {
  auto query =
      absl::StrJoin({"df = px.DataFrame('bar')", "df = df[df['service'] != '' && px.asid() != 10]",
                     "px.display(df, 'out')"},
                    "\n");
  Parser parser;
  auto ast_or_s = parser.Parse(query);
  ASSERT_NOT_OK(ast_or_s);

  EXPECT_THAT(ast_or_s.status(),
              HasCompilerError("SyntaxError: Expected expression after operator"));
}

TEST_F(ParserTest, FuncWithNoArgsDoesntError) {
  auto query = absl::StrJoin({"def func():", "    return 'test'", "func()"}, "\n");
  Parser parser;
  auto ast_or_s = parser.Parse(query);
  ASSERT_OK(ast_or_s);
}

TEST_F(ParserTest, FuncWithValidArgAndTrailingComma) {
  auto query = absl::StrJoin(
      {
          "def func(a,):",
          "    return 'test'",
          "func(1)",
      },
      "\n");
  Parser parser;
  auto ast_or_s = parser.Parse(query);
  ASSERT_OK(ast_or_s);
}

TEST_F(ParserTest, FuncWithNumbersAsArgsFails) {
  auto query = absl::StrJoin(
      {
          "def func(1):",
          "    return 'test'",
          "func(1)",
      },
      "\n");
  Parser parser;
  auto ast_or_s = parser.Parse(query);
  ASSERT_NOT_OK(ast_or_s);
  EXPECT_THAT(ast_or_s.status(), HasCompilerError("SyntaxError: Expected name identifier in args"));
}

TEST_F(ParserTest, FuncWithTypedArgs) {
  auto query = absl::StrJoin(
      {
          "def func(a: type, b: type, c: type = 3):",
          "    return 'test'",
          "func(1, 2)",
      },
      "\n");
  Parser parser;
  auto ast_or_s = parser.Parse(query);
  ASSERT_OK(ast_or_s);
}

constexpr const char* kMixedSpacesThenTabs = R"pxl(
# Spacing will be obvious if you have render whitespace in your editor.
def func():
    # couple of normal lines
    df = px.DataFrame('http_events')
    df = df[['resp_body']]
    # indent with a tab instead
	df.abc = 1
    return 'test'
)pxl";

constexpr const char* kMixedTabsThenSpaces = R"pxl(
# Spacing will be obvious if you have render whitespace in your editor.
def func():
	# couple of normal lines
	df = px.DataFrame('http_events')
	df = df[['resp_body']]
	# indent with a tab instead
    df.abc = 1
    return 'test'
)pxl";

constexpr const char* kUnexpectedIndent = R"pxl(
def func():
    # couple of normal lines
    df = px.DataFrame('http_events')
    df = df[['resp_body']]
    # indent with a tab instead
      df.abc = 1
    return 'test'
)pxl";

constexpr const char* kUnexpectedDedent = R"pxl(
def func():
    # couple of normal lines
    df = px.DataFrame('http_events')
    df = df[['resp_body']]
    # indent with a tab instead
   df.abc = 1
    return 'test'
)pxl";

TEST_F(ParserTest, mixed_indent_styles) {
  Parser parser;
  auto ast_or_s = parser.Parse(kMixedSpacesThenTabs);
  EXPECT_THAT(ast_or_s.status(),
              HasCompilerErrorAt(
                  8, 1, "IndentationError: inconsistent use of tabs and spaces in indentation"));

  ast_or_s = parser.Parse(kMixedTabsThenSpaces);
  EXPECT_THAT(ast_or_s.status(),
              HasCompilerErrorAt(
                  8, 4, "IndentationError: unindent does not match any outer indentation level"));
}

TEST_F(ParserTest, indent_dedent_error) {
  Parser parser;
  auto ast_or_s = parser.Parse(kUnexpectedIndent);
  EXPECT_THAT(ast_or_s.status(), HasCompilerErrorAt(7, 7, "IndentationError: unexpected indent"));

  ast_or_s = parser.Parse(kUnexpectedDedent);
  EXPECT_THAT(ast_or_s.status(),
              HasCompilerErrorAt(
                  7, 3, "IndentationError: unindent does not match any outer indentation level"));
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
