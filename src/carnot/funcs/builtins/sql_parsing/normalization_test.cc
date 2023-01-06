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
#include "src/carnot/funcs/builtins/sql_parsing/normalization.h"
#include "src/common/base/logging.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace builtins {
namespace sql_parsing {

struct NormSQLTestCase {
  std::string input_sql_str;
  std::vector<std::string> input_params;
  NormalizeResult expected_result;
};

class NormPGSQLTest : public ::testing::TestWithParam<NormSQLTestCase> {};

TEST_P(NormPGSQLTest, basic) {
  auto test_case = GetParam();

  auto result_or_s = normalize_sql<pgsql_parser::PostgresSQLParser, pgsql_parser::PostgresSQLLexer>(
      test_case.input_sql_str, test_case.input_params);

  ASSERT_OK(result_or_s);
  auto result = result_or_s.ConsumeValueOrDie();

  EXPECT_EQ(result.normalized_query, test_case.expected_result.normalized_query);
  EXPECT_EQ(result.params, test_case.expected_result.params);
  EXPECT_EQ(result.error, test_case.expected_result.error);
}

INSTANTIATE_TEST_SUITE_P(
    NormPGSQLVariants, NormPGSQLTest,
    ::testing::Values(
        // Trivial test case.
        NormSQLTestCase{"SELECT 1", {}, NormalizeResult{"SELECT $1", {"1"}, {}}},
        // SELECT test case.
        NormSQLTestCase{
            "SELECT * FROM test WHERE prop=1234 AND prop2='abcd'",
            {},
            NormalizeResult{
                "SELECT * FROM test WHERE prop=$1 AND prop2=$2",
                {"1234", "'abcd'"},
            },
        },
        // CREATE TABLE test case.
        NormSQLTestCase{
            "CREATE TABLE test (name varchar(20), address text, foo int, bar text)",
            {},
            NormalizeResult{
                "CREATE TABLE test (name varchar(20), address text, foo int, bar text)",
                {},
            },
        },
        // UPDATE test case.
        NormSQLTestCase{
            "UPDATE test SET age=10 where name='abcd'",
            {},
            NormalizeResult{
                "UPDATE test SET age=$1 where name=$2",
                {"10", "'abcd'"},
            },
        },
        // Comparison op
        NormSQLTestCase{
            "SELECT * from test where abcd >= 11",
            {},
            NormalizeResult{
                "SELECT * from test where abcd >= $1",
                {"11"},
            },
        },
        // SELECT INTO
        NormSQLTestCase{
            "SELECT 'abcd' as col1, 1234 as col2, 1.2345 as col3 into my_new_table",
            {},
            NormalizeResult{
                "SELECT $1 as col1, $2 as col2, $3 as col3 into my_new_table",
                {"'abcd'", "1234", "1.2345"},
            },
        },
        // Function call.
        NormSQLTestCase{
            "SELECT length(abcd) + 1 from test",
            {},
            NormalizeResult{
                "SELECT length(abcd) + $1 from test",
                {"1"},
            },
        },
        // Preexisting parameters test case.
        NormSQLTestCase{
            "SELECT * from test WHERE name=$1 AND tag=1234 AND property=$2",
            {"'abcd'", "1.23"},
            NormalizeResult{
                "SELECT * from test WHERE name=$1 AND tag=$2 AND property=$3",
                {"'abcd'", "1234", "1.23"},
            },
        },
        // Preexisting parameters test case with multiple of the same param.
        NormSQLTestCase{
            "SELECT * from test WHERE name=$1 AND tag=1234 AND property=$1",
            {"'abcd'"},
            NormalizeResult{
                "SELECT * from test WHERE name=$1 AND tag=$2 AND property=$3",
                {"'abcd'", "1234", "'abcd'"},
            },
        },
        // INSERT INTO test case.
        NormSQLTestCase{
            R"(INSERT INTO test (a, b, c, d, e) VALUES (1, 'abcd', 1.23, true, E'\\xDEADBEEF'))",
            {},
            NormalizeResult{
                "INSERT INTO test (a, b, c, d, e) VALUES ($1, $2, $3, $4, $5)",
                {"1", "'abcd'", "1.23", "true", R"(E'\\xDEADBEEF')"},
            },
        }));

class NormMySQLTest : public ::testing::TestWithParam<NormSQLTestCase> {};

TEST_P(NormMySQLTest, basic) {
  auto test_case = GetParam();

  auto result_or_s =
      normalize_sql<mysql_parser::MySQLParser, mysql_parser::MySQLLexer, UpperCaseCharStream>(
          test_case.input_sql_str, test_case.input_params);

  ASSERT_OK(result_or_s);
  auto result = result_or_s.ConsumeValueOrDie();

  EXPECT_EQ(result.normalized_query, test_case.expected_result.normalized_query);
  EXPECT_EQ(result.params, test_case.expected_result.params);
  EXPECT_EQ(result.error, test_case.expected_result.error);
}

INSTANTIATE_TEST_SUITE_P(
    NormMySQLVariants, NormMySQLTest,
    ::testing::Values(
        // Trivial test case.
        NormSQLTestCase{"SELECT 1", {}, NormalizeResult{"SELECT ?", {"1"}, {}}},
        // SELECT
        NormSQLTestCase{
            "SELECT * FROM test WHERE prop=1234 AND prop2='abcd'",
            {},
            NormalizeResult{
                "SELECT * FROM test WHERE prop=? AND prop2=?",
                {"1234", "'abcd'"},
            },
        },
        // CREATE TABLE
        NormSQLTestCase{
            "CREATE TABLE test (name text, address text, foo int, bar text)",
            {},
            NormalizeResult{
                "CREATE TABLE test (name text, address text, foo int, bar text)",
                {},
            },
        },
        // UPDATE
        NormSQLTestCase{
            "UPDATE test SET age=10 where name='abcd'",
            {},
            NormalizeResult{
                "UPDATE test SET age=? where name=?",
                {"10", "'abcd'"},
            },
        },
        // Comparison op
        NormSQLTestCase{
            "SELECT * from test where abcd >= 11",
            {},
            NormalizeResult{
                "SELECT * from test where abcd >= ?",
                {"11"},
            },
        },
        // SELECT into
        // MySQL doesn't support SELECT into, instead it has INSERT INTO ... SELECT syntax.
        NormSQLTestCase{
            "INSERT INTO my_new_table SELECT 'abcd' as col1, 1234 as col2, 1.2345 as col3",
            {},
            NormalizeResult{
                "INSERT INTO my_new_table SELECT ? as col1, ? as col2, ? as col3",
                {"'abcd'", "1234", "1.2345"},
            },
        },
        // Function call.
        NormSQLTestCase{
            "SELECT length(abcd) + 1 from test",
            {},
            NormalizeResult{
                "SELECT length(abcd) + ? from test",
                {"1"},
            },
        },
        // Preexisting parameters test case.
        NormSQLTestCase{
            "SELECT * from test WHERE name=? AND tag=1234 AND property=?",
            {"'abcd'", "1.23"},
            NormalizeResult{
                "SELECT * from test WHERE name=? AND tag=? AND property=?",
                {"'abcd'", "1234", "1.23"},
            },
        },
        // INSERT INTO
        NormSQLTestCase{
            R"(INSERT INTO test (a, b, c, d, e) VALUES (1, 'abcd', 1.23, true, X'DEADBEEF'))",
            {},
            NormalizeResult{
                "INSERT INTO test (a, b, c, d, e) VALUES (?, ?, ?, ?, ?)",
                {"1", "'abcd'", "1.23", "true", "X'DEADBEEF'"},
            },
        }));

}  // namespace sql_parsing
}  // namespace builtins
}  // namespace carnot
}  // namespace px
