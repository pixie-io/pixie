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

#include <ostream>

#include "mysql_parser/MySQLLexer.h"
#include "mysql_parser/MySQLParser.h"
#include "pgsql_parser/PostgresSQLLexer.h"
#include "pgsql_parser/PostgresSQLParser.h"
#include "src/carnot/funcs/builtins/sql_parsing/antlr_parse.h"
#include "src/common/base/logging.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace builtins {
namespace sql_parsing {

class NopListener : public antlr4::tree::ParseTreeListener {
 public:
  void enterEveryRule(antlr4::ParserRuleContext*) {}
  void visitTerminal(antlr4::tree::TerminalNode*) {}
  void visitErrorNode(antlr4::tree::ErrorNode*) {}
  void exitEveryRule(antlr4::ParserRuleContext*) {}
};

class PgFastPathTest : public ::testing::TestWithParam<std::string> {};
class MySQLFastPathTest : public ::testing::TestWithParam<std::string> {};

// Test that all of the SQL queries specified don't error in SLL mode.
TEST_P(PgFastPathTest, postgres) {
  auto query = GetParam();

  AntlrParser<pgsql_parser::PostgresSQLParser, pgsql_parser::PostgresSQLLexer> parser(
      query, /* sll_only */ true);

  NopListener listener;
  auto status = parser.ParseWalk(&listener);

  ASSERT_OK(status);
}

TEST_P(MySQLFastPathTest, mysql) {
  auto query = GetParam();

  AntlrParser<mysql_parser::MySQLParser, mysql_parser::MySQLLexer, UpperCaseCharStream> parser(
      query, /* sll_only */ true);

  NopListener listener;
  auto status = parser.ParseWalk(&listener);

  ASSERT_OK(status);
}

INSTANTIATE_TEST_SUITE_P(
    PgFastPathVariants, PgFastPathTest,
    ::testing::Values(
        // Trivial test case.
        "SELECT 1", "BEGIN;",
        // SELECT test case.
        "SELECT * FROM test WHERE prop=1234 AND prop2='abcd'",
        // CREATE TABLE test case.
        "CREATE TABLE test (name varchar(20), address text, foo int, bar text)",
        // UPDATE test case.
        "UPDATE test SET age=10 where name='abcd'",
        // Comparison op
        "SELECT * from test where abcd >= 11",
        // SELECT INTO
        "SELECT 'abcd' as col1, 1234 as col2, 1.2345 as col3 into my_new_table",
        // Function call.
        "SELECT length(abcd) + 1 from test",
        // INSERT INTO test case.
        R"(INSERT INTO test (a, b, c, d, e) VALUES (1, 'abcd', 1.23, true, E'\\xDEADBEEF'))"));

INSTANTIATE_TEST_SUITE_P(
    MySQLFastPathVariants, MySQLFastPathTest,
    ::testing::Values(
        // Trivial test case.
        "SELECT 1", "BEGIN;",
        // SELECT
        "SELECT * FROM test WHERE prop=1234 AND prop2='abcd'",
        // CREATE TABLE
        "CREATE TABLE test (name text, address text, foo int, bar text)",
        // UPDATE
        "UPDATE test SET age=10 where name='abcd'",
        // Comparison op
        "SELECT * from test where abcd >= 11",
        // SELECT into
        "INSERT INTO my_new_table SELECT 'abcd' as col1, 1234 as col2, 1.2345 as col3",
        // Function call.
        "SELECT length(abcd) + 1 from test",
        // INSERT INTO
        R"(INSERT INTO test (a, b, c, d, e) VALUES (1, 'abcd', 1.23, true, X'DEADBEEF'))"));

}  // namespace sql_parsing
}  // namespace builtins
}  // namespace carnot
}  // namespace px
