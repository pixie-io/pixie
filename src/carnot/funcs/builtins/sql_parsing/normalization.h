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

#include <antlr4-runtime.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/substitute.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <numeric>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "mysql_parser/MySQLLexer.h"
#include "mysql_parser/MySQLParser.h"
#include "pgsql_parser/PostgresSQLLexer.h"
#include "pgsql_parser/PostgresSQLParser.h"
#include "src/carnot/funcs/builtins/sql_parsing/antlr_parse.h"
#include "src/common/base/logging.h"
#include "src/common/base/statusor.h"
#include "src/common/base/utils.h"

namespace px {
namespace carnot {
namespace builtins {
namespace sql_parsing {

template <typename TParser>
struct ParserTypeTraits {};

template <>
struct ParserTypeTraits<pgsql_parser::PostgresSQLParser> {
  static std::string NextPlaceholder(std::string curr_placeholder);
  static std::string FirstPlaceholder() { return "$1"; }
  static bool IsNamedPlaceholder(std::string) {
    // Currently, we only pull out Dollar_number placeholder from postgres, so no placeholders are
    // named.
    return false;
  }
  static bool IsGenericPlaceholder(std::string) {
    // Postgres has no generic placeholder like mysql does.
    return false;
  }
  static int PlaceholderToParamIndex(std::string placeholder);

  static constexpr size_t constant_rule_index =
      pgsql_parser::PostgresSQLParser::RuleUnsigned_value_specification;
  static constexpr size_t param_placeholder_rule_index =
      pgsql_parser::PostgresSQLParser::RuleDollar_number;
};

template <>
struct ParserTypeTraits<mysql_parser::MySQLParser> {
  static std::string FirstPlaceholder() { return "?"; }
  static std::string NextPlaceholder(std::string) { return "?"; }
  static bool IsNamedPlaceholder(std::string placeholder) { return placeholder[0] == '@'; }
  static bool IsGenericPlaceholder(std::string placeholder) {
    return placeholder.compare("?") == 0;
  }
  // MySQL has no indexed placeholders, so this function will never be called.
  static int PlaceholderToParamIndex(std::string) { return -1; }

  static constexpr size_t constant_rule_index = mysql_parser::MySQLParser::RuleConstant;
  static constexpr size_t param_placeholder_rule_index =
      mysql_parser::MySQLParser::RuleMysqlVariable;
};

struct SQLFragment {
  enum FragmentType {
    CONSTANT = 0,
    PARAM_PLACEHOLDER = 1,
  };
  size_t line;
  size_t start_char_index;
  std::string text;
  FragmentType type;
};

class ParserRuleFragmentListener : public antlr4::tree::ParseTreeListener {
 public:
  template <size_t N>
  explicit ParserRuleFragmentListener(const size_t (&rule_indices)[N],
                                      const SQLFragment::FragmentType (&rule_fragment_types)[N]) {
    for (size_t i = 0; i < N; ++i) {
      index_to_type_.insert({rule_indices[i], rule_fragment_types[i]});
    }
  }
  void enterEveryRule(antlr4::ParserRuleContext* ctx);

  const std::vector<SQLFragment>& fragments() { return fragments_; }

  void visitTerminal(antlr4::tree::TerminalNode*) {}
  void visitErrorNode(antlr4::tree::ErrorNode*) {}
  void exitEveryRule(antlr4::ParserRuleContext*) {}

 private:
  absl::flat_hash_map<size_t, SQLFragment::FragmentType> index_to_type_;
  std::vector<SQLFragment> fragments_;
};

struct NormalizeResult {
  std::string normalized_query;
  std::vector<std::string> params;
  std::string error = "";

  static inline constexpr char kErrorKey[] = "error";
  static inline constexpr char kQueryKey[] = "query";
  static inline constexpr char kParamsKey[] = "params";

  std::string ToJSON() const;

  static NormalizeResult FromJSON(std::string json_str);
};

std::ostream& operator<<(std::ostream& os, const NormalizeResult& result);

struct NormalizationState {
  // Number of characters to shift character indices into the query by, to account for replacements
  // that have already occured.
  int n_shift_query = 0;
  std::vector<int> line_start_offsets;
  std::string next_placeholder;
};

static inline void CalculateLineOffsets(std::string query, NormalizationState* state) {
  // Calculate line start offsets.
  state->line_start_offsets.push_back(0);
  for (auto line : absl::StrSplit(query, '\n')) {
    // Add 1 for the newline char.
    state->line_start_offsets.push_back(line.length() + 1);
  }
  std::partial_sum(state->line_start_offsets.begin(), state->line_start_offsets.end(),
                   state->line_start_offsets.begin());
}

class SQLFragmentHandler {
 public:
  SQLFragmentHandler(NormalizationState* state, NormalizeResult* result)
      : state_(state), result_(result) {}
  virtual Status HandleFragment(const SQLFragment&) = 0;

 protected:
  void ReplaceFragmentWithPlaceholder(const SQLFragment& frag, absl::string_view placeholder);
  NormalizationState* state_;
  NormalizeResult* result_;
};

template <typename TParser>
class ParamFragmentHandler : public SQLFragmentHandler {
 public:
  ParamFragmentHandler(const std::vector<std::string>& param_values, NormalizationState* state,
                       NormalizeResult* result)
      : SQLFragmentHandler(state, result), param_values_(param_values) {}
  Status HandleFragment(const SQLFragment& frag) override {
    if (ParserTypeTraits<TParser>::IsNamedPlaceholder(frag.text)) {
      // TODO(james): Until we do stitching in PxL we'll ignore named placeholders since we don't
      // have access to their values.
      return Status::OK();
    }
    int index;
    if (ParserTypeTraits<TParser>::IsGenericPlaceholder(frag.text)) {
      index = count_++;
    } else {
      index = ParserTypeTraits<TParser>::PlaceholderToParamIndex(frag.text);
    }
    if (index == -1) {
      return error::InvalidArgument("Placeholder $0 invalid", frag.text);
    }
    if (static_cast<size_t>(index) >= param_values_.size()) {
      return error::InvalidArgument(
          "Query has more parameter placeholders in it than parameter values were passed in");
    }
    ReplaceFragmentWithPlaceholder(frag, state_->next_placeholder);
    state_->next_placeholder = ParserTypeTraits<TParser>::NextPlaceholder(state_->next_placeholder);
    result_->params.push_back(param_values_[index]);
    return Status::OK();
  }

 private:
  const std::vector<std::string>& param_values_;
  size_t count_ = 0;
};

template <typename TParser>
class ConstantFragmentHandler : public SQLFragmentHandler {
 public:
  using SQLFragmentHandler::SQLFragmentHandler;
  Status HandleFragment(const SQLFragment& frag) override {
    ReplaceFragmentWithPlaceholder(frag, state_->next_placeholder);
    result_->params.push_back(frag.text);
    state_->next_placeholder = ParserTypeTraits<TParser>::NextPlaceholder(state_->next_placeholder);
    return Status::OK();
  }
};

/**
 * normalize_sql replaces table names and constants in a sql query with placeholders, inplace.
 * @param sql: Unnormalized SQL query.
 * @param param_values: Parameters already account for in the unnormalized version of the query. For
 * non-EXECUTE type queries this should be empty.
 * @return status or result, whether the query was successful or not and if it was the normalization
 * result.
 */
template <typename TParser, typename TLexer, typename TCharStream = antlr4::ANTLRInputStream>
StatusOr<NormalizeResult> normalize_sql(std::string sql,
                                        const std::vector<std::string>& param_values) {
  AntlrParser<TParser, TLexer, TCharStream> parser(sql);
  ParserRuleFragmentListener listener({ParserTypeTraits<TParser>::constant_rule_index,
                                       ParserTypeTraits<TParser>::param_placeholder_rule_index},
                                      {SQLFragment::CONSTANT, SQLFragment::PARAM_PLACEHOLDER});
  PX_RETURN_IF_ERROR(parser.ParseWalk(&listener));

  NormalizationState state;
  NormalizeResult result;
  result.normalized_query = sql;
  CalculateLineOffsets(sql, &state);
  state.next_placeholder = ParserTypeTraits<TParser>::FirstPlaceholder();

  ConstantFragmentHandler<TParser> constant_handler(&state, &result);
  ParamFragmentHandler<TParser> param_handler(param_values, &state, &result);

  // Sort fragments into the order they appear in the query.
  std::vector<SQLFragment> sorted_fragments(listener.fragments());
  std::sort(sorted_fragments.begin(), sorted_fragments.end(), [](SQLFragment a, SQLFragment b) {
    if (a.line != b.line) {
      return a.line < b.line;
    }
    return a.start_char_index < b.start_char_index;
  });

  for (const auto& fragment : sorted_fragments) {
    switch (fragment.type) {
      case SQLFragment::CONSTANT:
        PX_RETURN_IF_ERROR(constant_handler.HandleFragment(fragment));
        break;
      case SQLFragment::PARAM_PLACEHOLDER:
        PX_RETURN_IF_ERROR(param_handler.HandleFragment(fragment));
        break;
    }
  }
  return result;
}

StatusOr<NormalizeResult> normalize_pgsql(std::string sql,
                                          const std::vector<std::string>& param_values);

StatusOr<NormalizeResult> normalize_mysql(std::string sql,
                                          const std::vector<std::string>& param_values);

}  // namespace sql_parsing
}  // namespace builtins
}  // namespace carnot
}  // namespace px
