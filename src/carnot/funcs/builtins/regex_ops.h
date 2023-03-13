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

#include <rapidjson/document.h>

#include <absl/strings/strip.h>

#include <algorithm>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>
#include "re2/re2.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace builtins {

class RegexMatchUDF : public udf::ScalarUDF {
 public:
  Status Init(FunctionContext*, StringValue regex) {
    re2::RE2::Options opts;
    opts.set_dot_nl(true);
    opts.set_log_errors(false);
    regex_ = std::make_unique<re2::RE2>(regex, opts);
    return Status::OK();
  }
  BoolValue Exec(FunctionContext*, StringValue input) {
    if (regex_->error_code() != RE2::NoError) {
      return false;
    }
    return RE2::FullMatch(input, *regex_);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Check for a match to a regex pattern in a string.")
        .Details(
            "This function checks the input string (second arg) for a match with the regex pattern "
            "(first arg). "
            "The regex pattern must match the full string. For example, the pattern 'abc' doesn't "
            "match the string 'abcd' but the pattern 'abc*' does match that string. "
            "We support google RE2 syntax. More details on that syntax can be found "
            "[here](https://github.com/google/re2/wiki/Syntax). ")
        .Example("df.is_match = px.regex_match('.*my_regex_pattern.*', df.resp_body)")
        .Arg("arg1", "The regex pattern to match.")
        .Arg("arg2", "The string column to match the pattern against.")
        .Returns("boolean representing whether the pattern matched the input or not.");
  }

 private:
  std::unique_ptr<re2::RE2> regex_;
};

class RegexReplaceUDF : public udf::ScalarUDF {
 public:
  Status Init(FunctionContext*, StringValue regex_pattern) {
    re2::RE2::Options opts;
    opts.set_log_errors(false);
    regex_ = std::make_unique<re2::RE2>(regex_pattern, opts);
    return Status::OK();
  }
  StringValue Exec(FunctionContext*, StringValue input, StringValue sub) {
    if (regex_->error_code() != RE2::NoError) {
      return absl::Substitute("Invalid regex expr: $0", regex_->error());
    }
    std::string err_str;
    if (!regex_->CheckRewriteString(sub, &err_str)) {
      return absl::Substitute("Invalid regex in substitution string: $0", err_str);
    }
    RE2::GlobalReplace(&input, *regex_, sub);
    return input;
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Replace all matches of a regex pattern in a string with another string.")
        .Details(
            "This function replaces all matches of the regex pattern (first arg) in the string "
            "(second arg) with the substitution string (third arg). "
            "We support google RE2 syntax. More details on that syntax can be found "
            "[here](https://github.com/google/re2/wiki/Syntax). "
            "Note that numbered capture groups are supported and can be accessed in the "
            "substitution string with \\1...\\N. See the google RE2 docs for more details on "
            "capture "
            "groups. However, named capture groups are not supported.")
        .Example(R"(df.replaced_str = px.replace('10\.0\.0\.[0-9]+', df.resp_body, 'IP_ADDR'))")
        .Arg("arg1", "The regex pattern to replace.")
        .Arg("arg2", "The string column to replace pattern occurrences in.")
        .Arg("arg3", "The string to replace the pattern with.")
        .Returns(
            "The original string with all occurrences of the pattern replaced by the substitution "
            "string.");
  }

 private:
  std::unique_ptr<re2::RE2> regex_;
};

class MatchRegexRule : public udf::ScalarUDF {
 public:
  Status Init(FunctionContext* ctx, StringValue encodedRegexRules) {
    // Parse encodedRegexRules as json.
    rapidjson::Document regex_rules_json;
    rapidjson::ParseResult parse_result = regex_rules_json.Parse(encodedRegexRules.data());
    if (!parse_result) {
      return Status(statuspb::Code::INVALID_ARGUMENT, "unable to parse string as json");
    }
    // Populate the parse regular expressions into self::regex_rules.
    for (rapidjson::Value::ConstMemberIterator itr = regex_rules_json.MemberBegin();
         itr != regex_rules_json.MemberEnd(); ++itr) {
      RegexMatchUDF regex_match_udf;
      std::string name = itr->name.GetString();
      std::string regex_pattern = itr->value.GetString();
      PX_RETURN_IF_ERROR(regex_match_udf.Init(ctx, regex_pattern));
      regex_rules.emplace_back(make_pair(name, std::move(regex_match_udf)));
      regex_rules_length++;
    }
    return Status::OK();
  }

  types::StringValue Exec(FunctionContext* ctx, StringValue value) {
    for (int i = 0; i < regex_rules_length; i++) {
      if (regex_rules[i].second.Exec(ctx, value).val) {
        return regex_rules[i].first;
      }
    }
    return "";
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Check for a match to a json of regex pattern rules in a string.")
        .Details(
            "This function checks the input string (second arg) for a match with the regex pattern "
            "rules"
            "(first arg). "
            "The regex pattern must match the full string. For example, the pattern 'abc' doesn't "
            "match the string 'abcd' but the pattern 'abc*' does match that string. "
            "We support google RE2 syntax. More details on that syntax can be found "
            "[here](https://github.com/google/re2/wiki/Syntax). ")
        .Example(
            "df.is_match = px.regex_match('{\"rule1\": \".*my_regex_pattern.*\"}', df.resp_body)")
        .Arg("arg1",
             "The encoded json map from the name of the rule to the regex pattern for the rule.")
        .Arg("arg2", "The string column to match the pattern against.")
        .Returns(
            "string representing the name of the first rule that matched or an empty string if no "
            "match.");
  }

 private:
  int regex_rules_length = 0;
  std::vector<std::pair<std::string, RegexMatchUDF> > regex_rules;
};

void RegisterRegexOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
