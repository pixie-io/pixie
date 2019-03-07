#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include <pypa/parser/parser.hh>

#include "absl/strings/str_join.h"
#include "src/carnot/compiler/string_reader.h"

namespace pl {
namespace carnot {
namespace compiler {

using testing::_;

TEST(StringReaderTest, basic) {
  std::unique_ptr<pypa::Reader> reader =
      std::make_unique<StringReader>("From(table='cpu', select=['cpu0'])\\\n.Range(time='-2m')");
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_FALSE(reader->eof());
  EXPECT_EQ("From(table='cpu', select=['cpu0'])\\\n", reader->next_line());
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_FALSE(reader->eof());
  EXPECT_EQ("From(table='cpu', select=['cpu0'])\\", reader->get_line(1));
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_EQ(".Range(time='-2m')", reader->get_line(2));
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_EQ(".Range(time='-2m')", reader->next_line());
  EXPECT_EQ(2, reader->get_line_number());
  EXPECT_TRUE(reader->eof());
}

TEST(StringReaderTest, pypa) {
  // Test that StringReader works with pypa's Lexer.
  pypa::Lexer lexer(
      std::make_unique<StringReader>("From(table='cpu', select=['cpu0'])\\\n.Range(time='-2m');"));
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  EXPECT_TRUE(pypa::parse(lexer, ast, symbols, options));
  EXPECT_EQ(pypa::AstType::Module, ast->type);
}

TEST(StringReaderTest, pypa_mult_statements) {
  // Test that StringReader works with pypa's Lexer.
  pypa::Lexer lexer(std::make_unique<StringReader>(
      "queryDF = From(table = 'cpu', select = [ 'cpu0', 'cpu1' ])\nqueryDF.Range(time ='-2m')"));
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  EXPECT_TRUE(pypa::parse(lexer, ast, symbols, options));
  EXPECT_EQ(pypa::AstType::Module, ast->type);
}

TEST(StringReaderTest, pypa_syntax_error) {
  // Test that StringReader works with pypa's Lexer and can detect syntax errors.
  pypa::Lexer lexer(std::make_unique<StringReader>("queryDF = From(table = 'cpu'"));
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  if (VLOG_IS_ON(1)) {
    options.printerrors = true;
  } else {
    options.printerrors = false;
  }

  EXPECT_FALSE(pypa::parse(lexer, ast, symbols, options));
}

TEST(StringReaderTest, pypa_newline_error) {
  // Test that StringReader works with pypa's Lexer and can detect when a newline is improperly
  // placed.
  pypa::Lexer lexer(std::make_unique<StringReader>(
      "queryDF = From(table = 'cpu', select=['cpu0']\n.Range(time='-2m')"));
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  if (VLOG_IS_ON(1)) {
    options.printerrors = true;
  } else {
    options.printerrors = false;
  }

  EXPECT_FALSE(pypa::parse(lexer, ast, symbols, options));

  pypa::Lexer lexer2(std::make_unique<StringReader>(
      absl::StrJoin({"queryDF = From(table = 'cpu', select=['cpu0'])", "queryDF.Range(time='-2m')",
                     "rangeDF.Map(fn=1, 20)", "rangeDF.Agg(fn=2)"},
                    "\n")));

  EXPECT_FALSE(pypa::parse(lexer2, ast, symbols, options));

  pypa::Lexer lexer3(std::make_unique<StringReader>(
      absl::StrJoin({"queryDF = From(table = 'cpu', select=['cpu0'])", "queryDF.Range(time='-2m')",
                     "rangeDF.Map(fn=1, 20)"},
                    "\n")));

  EXPECT_FALSE(pypa::parse(lexer3, ast, symbols, options));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
