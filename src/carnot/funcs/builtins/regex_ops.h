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
#include <algorithm>
#include <memory>
#include <string>
#include <regex>
#include <rapidjson/document.h>
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
    regex_ = std::make_unique<re2::RE2>(regex);
    return Status::OK();
  }
  BoolValue Exec(FunctionContext*, StringValue input) { return RE2::FullMatch(input, *regex_); }

 private:
  std::unique_ptr<re2::RE2> regex_;
};


class MatchRegexRule : public udf::ScalarUDF {
 public:
  Status Init(FunctionContext*, StringValue encodedRegexRules) {
    rapidjson::ParseResult ok = regex_rules.Parse(encodedRegexRules.data());
    // TODO(zasgar/michellenguyen, PP-419): Replace with null when available.
    if (ok == nullptr) {
      return Status(statuspb::Code::INVALID_ARGUMENT, "unable to convert to json");
    }
    return Status::OK();
  }
  types::StringValue Exec(FunctionContext*, StringValue value) {
    RegexMatchUDF regex_match; 
    for (rapidjson::Value::ConstMemberIterator itr = regex_rules.MemberBegin(); itr != regex_rules.MemberEnd(); ++itr) {
        auto name = itr->name.GetString();
        PL_UNUSED(regex_match.Init(nullptr, itr->value.GetString()));
        auto is_match = regex_match.Exec(nullptr, value).val;
        if (is_match) {
            return name; 
        }
    }
    return "";
  }

 private:
  rapidjson::Document regex_rules;
};

void RegisterRegexOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
