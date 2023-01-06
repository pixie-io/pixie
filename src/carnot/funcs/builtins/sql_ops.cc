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
#include <vector>

#include "src/carnot/funcs/builtins/sql_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace {
static inline px::Status ParseExecuteCommand(std::string execute, std::string* query,
                                             std::vector<std::string>* param_values) {
  absl::string_view execute_view(execute);
  absl::ConsumePrefix(&execute_view, "query=[");
  auto query_close_offset = execute_view.find("]");
  *query = execute_view.substr(0, query_close_offset);
  absl::ConsumePrefix(&execute_view, *query);
  absl::ConsumePrefix(&execute_view, "] params=[");
  auto params_close_offset = execute_view.rfind(']');
  auto params_str = execute_view.substr(0, params_close_offset);
  *param_values = absl::StrSplit(params_str, ", ");
  return px::Status::OK();
}

}  // namespace

namespace px {
namespace carnot {
namespace builtins {

void RegisterSQLOpsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  registry->RegisterOrDie<NormalizePostgresSQLUDF>("normalize_pgsql");
  registry->RegisterOrDie<NormalizeMySQLUDF>("normalize_mysql");
  /*****************************************
   * Aggregate UDFs.
   *****************************************/
}

types::StringValue NormalizePostgresSQLUDF::Exec(FunctionContext*, StringValue sql_str,
                                                 StringValue cmd_code) {
  std::string query;
  std::vector<std::string> param_values;

  if (cmd_code.compare(kPgExecCmdCode) == 0) {
    auto status = ParseExecuteCommand(sql_str, &query, &param_values);
    if (!status.ok()) {
      sql_parsing::NormalizeResult result;
      result.error = status.msg();
      return result.ToJSON();
    }
  } else if (cmd_code.compare(kPgQueryCmdCode) == 0) {
    query = sql_str;
  } else {
    sql_parsing::NormalizeResult result;
    result.error =
        absl::Substitute("cmd_code must be one of '$0' or '$1'", kPgQueryCmdCode, kPgExecCmdCode);
    return result.ToJSON();
  }

  auto result_or_s = sql_parsing::normalize_pgsql(query, param_values);
  if (!result_or_s.ok()) {
    sql_parsing::NormalizeResult result;
    result.error = result_or_s.status().msg();
    return result.ToJSON();
  }
  return result_or_s.ConsumeValueOrDie().ToJSON();
}

types::StringValue NormalizeMySQLUDF::Exec(FunctionContext*, StringValue sql_str,
                                           Int64Value cmd_code) {
  std::string query;
  std::vector<std::string> param_values;

  if (cmd_code == kMySQLExecuteCmdCode) {
    auto status = ParseExecuteCommand(sql_str, &query, &param_values);
    if (!status.ok()) {
      sql_parsing::NormalizeResult result;
      result.error = status.msg();
      return result.ToJSON();
    }
  } else if (cmd_code == kMySQLQueryCmdCode) {
    query = sql_str;
  } else {
    sql_parsing::NormalizeResult result;
    result.error = absl::Substitute("cmd_code must be one of '$0' or '$1'", kMySQLQueryCmdCode,
                                    kMySQLExecuteCmdCode);
    return result.ToJSON();
  }

  auto result_or_s = sql_parsing::normalize_mysql(query, param_values);
  if (!result_or_s.ok()) {
    sql_parsing::NormalizeResult result;
    result.error = result_or_s.status().msg();
    return result.ToJSON();
  }
  return result_or_s.ConsumeValueOrDie().ToJSON();
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
