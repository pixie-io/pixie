#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tuple>
#include <unordered_map>
#include <vector>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/parser/parser.h"

namespace pl {
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

}  // namespace planner
}  // namespace carnot
}  // namespace pl
