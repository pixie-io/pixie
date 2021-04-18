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

#include <absl/strings/str_join.h>
#include <pypa/parser/parser.hh>

#include "src/carnot/planner/parser/string_reader.h"

namespace px {
namespace carnot {
namespace planner {

using ::testing::_;

TEST(StringReaderTest, basic) {
  std::unique_ptr<pypa::Reader> reader = std::make_unique<StringReader>(
      "px.DataFrame(table='cpu', select=['cpu0'])\\\n.Range(time='-2m')");
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_FALSE(reader->eof());
  EXPECT_EQ("px.DataFrame(table='cpu', select=['cpu0'])\\\n", reader->next_line());
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_FALSE(reader->eof());
  EXPECT_EQ("px.DataFrame(table='cpu', select=['cpu0'])\\", reader->get_line(1));
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_EQ(".Range(time='-2m')", reader->get_line(2));
  EXPECT_EQ(1, reader->get_line_number());
  EXPECT_EQ(".Range(time='-2m')", reader->next_line());
  EXPECT_EQ(2, reader->get_line_number());
  EXPECT_TRUE(reader->eof());
}

TEST(StringReaderTest, pypa) {
  // Test that StringReader works with pypa's Lexer.
  pypa::Lexer lexer(std::make_unique<StringReader>(
      "px.DataFrame(table='cpu', select=['cpu0'])\\\n.range(time='-2m');"));
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  EXPECT_TRUE(pypa::parse(lexer, ast, symbols, options));
  EXPECT_EQ(pypa::AstType::Module, ast->type);
}

TEST(StringReaderTest, pypa_mult_statements) {
  // Test that StringReader works with pypa's Lexer.
  pypa::Lexer lexer(std::make_unique<StringReader>(
      "queryDF = px.DataFrame(table = 'cpu', select = [ 'cpu0', 'cpu1' "
      "])\nqueryDF.range(time ='-2m')"));
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  EXPECT_TRUE(pypa::parse(lexer, ast, symbols, options));
  EXPECT_EQ(pypa::AstType::Module, ast->type);
}

TEST(StringReaderTest, pypa_syntax_error) {
  // Test that StringReader works with pypa's Lexer and can detect syntax errors.
  pypa::Lexer lexer(std::make_unique<StringReader>("queryDF = px.DataFrame(table = 'cpu'"));
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
      "queryDF = px.DataFrame(table = 'cpu', select=['cpu0']\n.range(time='-2m')"));
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

TEST(StringReaderTest, pypa_args_before_kwargs_error) {
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  if (VLOG_IS_ON(1)) {
    options.printerrors = true;
  } else {
    options.printerrors = false;
  }

  // Expected to fail because keyword args should come before positional args.
  pypa::Lexer lexer(std::make_unique<StringReader>(
      absl::StrJoin({"queryDF = px.DataFrame(table = 'cpu', select=['cpu0'])",
                     "queryDF.range(time='-2m')", "rangeDF.agg(fn=2, 1)"},
                    "\n")));

  EXPECT_FALSE(pypa::parse(lexer, ast, symbols, options));
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
