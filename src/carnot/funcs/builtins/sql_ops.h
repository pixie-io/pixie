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

#pragma once

#include <absl/strings/strip.h>
#include <regex>
#include <string>
#include "src/carnot/funcs/builtins/sql_parsing/normalization.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/status.h"
#include "src/common/base/utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace builtins {

static constexpr char kPgExecCmdCode[] = "Execute";
static constexpr char kPgQueryCmdCode[] = "Query";
static constexpr int64_t kMySQLQueryCmdCode = 0x03;
static constexpr int64_t kMySQLExecuteCmdCode = 0x17;

class NormalizePostgresSQLUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue sql_str, StringValue cmd_code);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Normalizes PostgresSQL queries by replacing constants with placeholders.")
        .Details(
            "Replaces constants with '$n' placeholders."
            "Outputs the normalized query along with the values of the parameters")
        .Example(R"doc(
        | # Normalize the SQL query.
        | df.normalized_sql_json = px.normalize_pgsql('SELECT * FROM test WHERE prop='abcd', 'Query')
        | # Pluck the relevant values from the json.
        | # Value: 'SELECT * FROM test WHERE prop=$1'
        | df.normed_query = px.pluck(df.normalized_sql_json, 'query')
        | # Value: ['abcd']
        | df.params = px.pluck(df.normalized_sql_json, 'params')
        )doc")
        .Arg("sql_string", "The PostgresSQL query string")
        .Arg("cmd_code", "The PostgresSQL command tag for this sql request.")
        .Returns(
            "The normalized query with the values of the parameters in the query "
            "as JSON. Available keys: ['query', 'params', 'error']. Error will be non-empty if "
            "the query could not be normalized.");
  }
};

class NormalizeMySQLUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue sql_str, Int64Value cmd_code);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Normalizes MySQL queries by replacing constants with placeholders.")
        .Details(
            "Replaces constants with '?' placeholder."
            "Outputs the normalized query along with the values of the parameters")
        .Example(R"doc(
        | # Normalize the SQL query.
        | # px.mysql_command_code(3) == 'Query'
        | df.normalized_sql_json = px.normalize_mysql("SELECT * FROM test WHERE prop=@a AND prop2='abcd'", 3)
        | # Pluck the relevant values from the json.
        | # Value: 'SELECT * FROM test WHERE prop=@a AND prop2=?'
        | df.normed_query = px.pluck(df.normalized_sql_json, 'query')
        | # Value: ['abcd']
        | df.params = px.pluck(df.normalized_sql_json, 'params')
        )doc")
        .Arg("sql_string", "The MySQL query string")
        .Arg("cmd_code", "The MySQL command code for this sql request.")
        .Returns(
            "The normalized query with the values of the parameters in the query "
            "as JSON. Available keys: ['query', 'params', 'error']. Error will be non-empty if "
            "the query could not be normalized.");
  }
};

void RegisterSQLOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
