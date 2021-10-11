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

#include <cmath>
#include <limits>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace builtins {

udf::ScalarUDFDocBuilder AddDoc();
template <typename TReturn, typename TArg1, typename TArg2>

class ValueListener : public antlr4::tree::ParseTreeListener {
 public:
  void enterEveryRule(antlr4::ParserRuleContext* ctx) {
    auto rule_index = ctx->getRuleIndex();
    if (rule_index == pgsql_parser::PostgresSQLParser::RuleValue_expression_primary) {
      values_.push_back(ctx->getText());
    }
  }

  void visitTerminal(antlr4::tree::TerminalNode*) {}
  void visitErrorNode(antlr4::tree::ErrorNode*) {}
  void exitEveryRule(antlr4::ParserRuleContext*) {}

  const std::vector<std::string>& values() const { return values_; }

 private:
  std::vector<std::string> values_;

 static udf::ScalarUDFDocBuilder Doc() { return AddDoc(); }
};

types::StringValue PluckValueExprPostgresSQLUDF::Exec(FunctionContext*, StringValue sql_str) {
  sql_parsing::AntlrParser<pgsql_parser::PostgresSQLParser, pgsql_parser::PostgresSQLLexer,
                           antlr4::ANTLRInputStream>
      parser(sql_str);
  ValueListener listener;
  auto s = parser.ParseWalk(&listener);
  if (!s.ok()) {
    // If I were submitting production code i would do something with this error.
    LOG(ERROR) << s.msg();
  }
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartArray();
  for (const auto& value : listener.values()) {
    writer.String(value.c_str());
  }
  writer.EndArray();
  return sb.GetString();
};

void RegisterSecurityFuncsOrDie(udf::Registry* registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace px
