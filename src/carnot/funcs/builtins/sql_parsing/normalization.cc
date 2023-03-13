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

#include <string>

#include "mysql_parser/MySQLLexer.h"
#include "mysql_parser/MySQLParser.h"
#include "pgsql_parser/PostgresSQLLexer.h"
#include "pgsql_parser/PostgresSQLParser.h"
#include "src/carnot/funcs/builtins/sql_parsing/normalization.h"
#include "src/common/base/logging.h"
#include "src/common/base/statusor.h"

namespace px {
namespace carnot {
namespace builtins {
namespace sql_parsing {

std::string ParserTypeTraits<pgsql_parser::PostgresSQLParser>::NextPlaceholder(
    std::string curr_placeholder) {
  size_t curr_num;
  if (!absl::SimpleAtoi(curr_placeholder.substr(1), &curr_num)) {
    // Error placeholder.
    return "$-1";
  }
  return absl::StrCat("$", curr_num + 1);
}

int ParserTypeTraits<pgsql_parser::PostgresSQLParser>::PlaceholderToParamIndex(
    std::string placeholder) {
  int index;
  if (!absl::SimpleAtoi(placeholder.substr(1), &index)) {
    return -1;
  }
  // Postgres parameters are 1-indexed so we need to subtract 1.
  return index - 1;
}

void ParserRuleFragmentListener::enterEveryRule(antlr4::ParserRuleContext* ctx) {
  auto index = ctx->getRuleIndex();
  if (index_to_type_.contains(index)) {
    auto start_token = ctx->getStart();
    if (start_token == nullptr) {
      // If start_token is null, then the error listener should've received an error during
      // parsing. So we can silently return here.
      return;
    }
    fragments_.push_back(SQLFragment{
        start_token->getLine(),
        start_token->getCharPositionInLine(),
        ctx->getText(),
        index_to_type_[index],
    });
  }
}

std::string NormalizeResult::ToJSON() const {
  // Copy output to json array.
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartObject();
  if (!error.empty()) {
    writer.Key(kErrorKey);
    writer.String(error.c_str());
  } else {
    writer.Key(kQueryKey);
    writer.String(normalized_query.c_str());
    writer.Key(kParamsKey);
    writer.StartArray();
    for (const auto& p : params) {
      writer.String(p.c_str());
    }
    writer.EndArray();
  }
  writer.EndObject();
  return sb.GetString();
}

NormalizeResult NormalizeResult::FromJSON(std::string json_str) {
  NormalizeResult result;
  rapidjson::Document d;
  rapidjson::ParseResult ok = d.Parse(json_str.data());
  if (ok == nullptr) {
    result.error = "Failed to parse JSON while constructing NormalizeResult";
    return result;
  }
  for (rapidjson::Value::ConstMemberIterator itr = d.MemberBegin(); itr != d.MemberEnd(); ++itr) {
    auto name = itr->name.GetString();
    if (name == kErrorKey) {
      result.error = std::string(itr->value.GetString());
    }
    if (name == kQueryKey) {
      result.normalized_query = std::string(itr->value.GetString());
    }
    if (name == kParamsKey) {
      for (rapidjson::SizeType i = 0; i < itr->value.Size(); i++) {
        result.params.emplace_back(itr->value[i].GetString());
      }
    }
  }
  return result;
}

void SQLFragmentHandler::ReplaceFragmentWithPlaceholder(const SQLFragment& fragment,
                                                        absl::string_view placeholder) {
  result_->normalized_query.replace(state_->line_start_offsets[fragment.line - 1] +
                                        fragment.start_char_index - state_->n_shift_query,
                                    fragment.text.length(), placeholder);
  state_->n_shift_query += fragment.text.length() - placeholder.length();
}

StatusOr<NormalizeResult> normalize_pgsql(std::string sql,
                                          const std::vector<std::string>& param_values) {
  return normalize_sql<pgsql_parser::PostgresSQLParser, pgsql_parser::PostgresSQLLexer>(
      sql, param_values);
}

StatusOr<NormalizeResult> normalize_mysql(std::string sql,
                                          const std::vector<std::string>& param_values) {
  return normalize_sql<mysql_parser::MySQLParser, mysql_parser::MySQLLexer, UpperCaseCharStream>(
      sql, param_values);
}

std::ostream& operator<<(std::ostream& os, const NormalizeResult& result) {
  if (result.error != "") {
    return os << "error: " << result.error;
  }
  return os << absl::Substitute("query: $0\nparams:\n\t$1\n", result.normalized_query,
                                absl::StrJoin(result.params, "\n\t"));
}

}  // namespace sql_parsing
}  // namespace builtins
}  // namespace carnot
}  // namespace px
