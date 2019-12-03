#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tuple>
#include <unordered_map>
#include <vector>

#include "src/carnot/compiler/parser/parser.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

class ParserTest : public ::testing::Test {};

// Test sh
TEST_F(ParserTest, AndExpressionFailsGracefully) {
  auto query =
      absl::StrJoin({"df = pl.DataFrame('bar')", "df = df[df['service'] != '' && pl.asid() != 10]",
                     "display(df, 'out')"},
                    "\n");
  Parser parser;
  auto ast_or_s = parser.Parse(query);
  ASSERT_NOT_OK(ast_or_s);

  EXPECT_THAT(ast_or_s.status(),
              HasCompilerError("SyntaxError: Expected expression after operator"));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
