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

#include "sql_parsing/normalization.h"
#include "src/carnot/funcs/builtins/sql_ops.h"
#include "src/carnot/udf/test_utils.h"

namespace px {
namespace carnot {
namespace builtins {

using sql_parsing::NormalizeResult;

struct NormPgSQLTestCase {
  std::string input_sql_str;
  std::string cmd_code;
  NormalizeResult expected_result;
};

std::ostream& operator<<(std::ostream& os, const NormPgSQLTestCase& test_case) {
  return os << "\ninput: " << test_case.input_sql_str << std::endl
            << "expected:" << std::endl
            << test_case.expected_result;
}

class NormPGSQLTest : public ::testing::TestWithParam<NormPgSQLTestCase> {};

TEST_P(NormPGSQLTest, postgres_basic) {
  auto udf_tester = udf::UDFTester<NormalizePostgresSQLUDF>();
  auto test_case = GetParam();
  udf_tester.ForInput(test_case.input_sql_str, test_case.cmd_code)
      .Expect(test_case.expected_result.ToJSON());
}

INSTANTIATE_TEST_SUITE_P(
    NormPGSQLVariants, NormPGSQLTest,
    ::testing::Values(
        // Trivial test case.
        NormPgSQLTestCase{"SELECT 1", kPgQueryCmdCode,
                          NormalizeResult{
                              "SELECT $1",
                              {"1"},
                          }},
        // Basic EXECUTE select statement.
        NormPgSQLTestCase{
            R"(query=[SELECT * FROM test WHERE prop=$1 AND prop2='abcd'] params=['1234'])",
            kPgExecCmdCode,
            NormalizeResult{
                "SELECT * FROM test WHERE prop=$1 AND prop2=$2",
                {"'1234'", "'abcd'"},
            }},
        // Basic QUERY insert statement, with all primitive types.
        NormPgSQLTestCase{
            R"(INSERT INTO test (a, b, c, d, e) VALUES (1, 'abcd', 1.23, true, E'\\xDEADBEEF'))",
            kPgQueryCmdCode,
            NormalizeResult{
                "INSERT INTO test (a, b, c, d, e) VALUES ($1, $2, $3, $4, $5)",
                {"1", "'abcd'", "1.23", "true", R"(E'\\xDEADBEEF')"},
            }},
        // Basic EXECUTE insert statement with all primitive types.
        NormPgSQLTestCase{R"(query=[INSERT INTO test (a, b, c, d, e) VALUES ($1, $2, $3, $4, $5)] )"
                          R"(params=[1, 'abcd', 1.23, true, E'\\xDEADBEEF'])",
                          kPgExecCmdCode,
                          NormalizeResult{
                              "INSERT INTO test (a, b, c, d, e) VALUES ($1, $2, $3, $4, $5)",
                              {"1", "'abcd'", "1.23", "true", R"(E'\\xDEADBEEF')"},
                          }},
        // Basic Query select statement.
        NormPgSQLTestCase{R"(SELECT * FROM test WHERE prop='abcd')", kPgQueryCmdCode,
                          NormalizeResult{
                              "SELECT * FROM test WHERE prop=$1",
                              {"'abcd'"},
                          }}));

struct NormMySQLTestCase {
  std::string input_sql_str;
  int64_t cmd_code;
  NormalizeResult expected_result;
};

std::ostream& operator<<(std::ostream& os, const NormMySQLTestCase& test_case) {
  return os << "\ninput: " << test_case.input_sql_str << std::endl
            << "expected:" << std::endl
            << test_case.expected_result;
}
class NormMySQLTest : public ::testing::TestWithParam<NormMySQLTestCase> {};

TEST_P(NormMySQLTest, mysql_basic) {
  auto udf_tester = udf::UDFTester<NormalizeMySQLUDF>();
  auto test_case = GetParam();
  udf_tester.ForInput(test_case.input_sql_str, test_case.cmd_code)
      .Expect(test_case.expected_result.ToJSON());
}

INSTANTIATE_TEST_SUITE_P(
    NormMySQLTestVariants, NormMySQLTest,
    ::testing::Values(
        NormMySQLTestCase{
            "SELECT 1",
            kMySQLQueryCmdCode,
            NormalizeResult{
                "SELECT ?",
                {"1"},
            },
        },
        NormMySQLTestCase{
            "SELECT * FROM test WHERE prop=@a AND prop2='abcd'",
            kMySQLQueryCmdCode,
            NormalizeResult{
                "SELECT * FROM test WHERE prop=@a AND prop2=?",
                {"'abcd'"},
            },
        },
        NormMySQLTestCase{
            "query=[SELECT * FROM test WHERE prop=? AND prop2='abcd'] params=[1234]",
            kMySQLExecuteCmdCode,
            NormalizeResult{
                "SELECT * FROM test WHERE prop=? AND prop2=?",
                {"1234", "'abcd'"},
            },
        },
        NormMySQLTestCase{
            "query=[INSERT INTO test (a, b, c, d, e) VALUES (?, ?, 1.23, ?, ?)] params=[1, 'abcd', "
            "true, X'DEADBEEF']",
            kMySQLExecuteCmdCode,
            NormalizeResult{
                "INSERT INTO test (a, b, c, d, e) VALUES (?, ?, ?, ?, ?)",
                {"1", "'abcd'", "1.23", "true", "X'DEADBEEF'"},
            },
        },
        NormMySQLTestCase{
            "query=[SELECT sock.sock_id AS id, sock.name, sock.description, sock.price, "
            "sock.count, "
            "sock.image_url_1, sock.image_url_2, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
            "JOIN sock_tag ON sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id "
            "WHERE sock.sock_id =? GROUP BY sock.sock_id;] "
            "params=[zzz4f044-b040-410d-8ead-4de0446aec7e]",
            kMySQLExecuteCmdCode,
            NormalizeResult{
                "SELECT sock.sock_id AS id, sock.name, sock.description, sock.price, sock.count, "
                "sock.image_url_1, sock.image_url_2, GROUP_CONCAT(tag.name) AS tag_name FROM "
                "sock JOIN sock_tag ON sock.sock_id=sock_tag.sock_id JOIN tag ON "
                "sock_tag.tag_id=tag.tag_id "
                "WHERE sock.sock_id =? GROUP BY sock.sock_id;",
                {"zzz4f044-b040-410d-8ead-4de0446aec7e"},
            },
        },
        NormMySQLTestCase{
            "INSERT INTO test (a, b, c, d, e) VALUES (1, 'abcd', 1.23, true, X'DEADBEEF')",
            kMySQLQueryCmdCode,
            NormalizeResult{
                "INSERT INTO test (a, b, c, d, e) VALUES (?, ?, ?, ?, ?)",
                {"1", "'abcd'", "1.23", "true", "X'DEADBEEF'"},
            },
        }));

TEST(NormPGSQL, invalid_utf8) {
  std::string invalid(
      "\xbf\xef\xef\xbd\xbd\xbf\xbf\xef\xef\xbd\xbd\xbf\xbf\xef\xef\xbd\xbd\xbf\xbf\xef");

  auto expected_result = NormalizeResult{
      "",
      {},
      "Invalid UTF-8 byte sequence in SQL query",
  };

  auto udf_tester = udf::UDFTester<NormalizePostgresSQLUDF>();
  udf_tester.ForInput(invalid, kPgQueryCmdCode).Expect(expected_result.ToJSON());
}

TEST(NormMySQL, invalid_utf8) {
  std::string invalid(
      "\xbf\xef\xef\xbd\xbd\xbf\xbf\xef\xef\xbd\xbd\xbf\xbf\xef\xef\xbd\xbd\xbf\xbf\xef");

  auto expected_result = NormalizeResult{
      "",
      {},
      "Invalid UTF-8 byte sequence in SQL query",
  };

  auto udf_tester = udf::UDFTester<NormalizeMySQLUDF>();
  udf_tester.ForInput(invalid, kMySQLQueryCmdCode).Expect(expected_result.ToJSON());
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
