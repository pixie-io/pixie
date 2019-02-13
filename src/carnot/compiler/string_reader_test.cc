#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <pypa/parser/parser.hh>
#include <vector>

#include "src/carnot/compiler/string_reader.h"

namespace pl {
namespace carnot {
namespace compiler {

using testing::_;

TEST(StringReaderTest, basic) {
  std::unique_ptr<pypa::Reader> reader =
      std::make_unique<StringReader>("From(table='cpu', select=['cpu0'])\n.Range(time='-2m');");
  EXPECT_EQ(0, reader->get_line_number());
  EXPECT_FALSE(reader->eof());
  EXPECT_EQ("From(table='cpu', select=['cpu0'])", reader->next_line());
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_FALSE(reader->eof());
  EXPECT_EQ("From(table='cpu', select=['cpu0'])", reader->get_line(0));
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_EQ(".Range(time='-2m');", reader->get_line(1));
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_EQ(".Range(time='-2m');", reader->next_line());
  EXPECT_EQ(2, reader->get_line_number());
  EXPECT_TRUE(reader->eof());
}

TEST(StringReaderTest, pypa) {
  // Test that StringReader works with pypa's Lexer.
  pypa::Lexer lexer(
      std::make_unique<StringReader>("From(table='cpu', select=['cpu0'])\n.Range(time='-2m');"));
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  EXPECT_TRUE(pypa::parse(lexer, ast, symbols, options));
  EXPECT_EQ(pypa::AstType::Module, ast->type);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
