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

#include <regex>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/shared/types/types.h"
#include "src/common/json/json.h"

namespace px {
namespace carnot {
namespace builtins {

udf::ScalarUDFDocBuilder AddDoc();
template <typename TReturn, typename TArg1, typename TArg2>

template <>
class MatchRegexRule<types::StringValue, types::StringValue, types::StringArray> : public udf::ScalarUDF {
 public:
  types::StringValue GetValuesFromPostgreSQLQuery::Exec(FunctionContext*, StringValue value, StringValue regexRules) {
    Json::Reader reader;
    Json::Value regexRulesParsed;

    bool parseSuccess = reader.parse(regexRules, regexRulesParsed, false);

    if (parseSuccess) {
      const Json::Value regexRulesMap = root["result"];
    }
    
    for (auto item = regexRulesMap.begin(); item != regexRulesMap.end(); ++item) {
        if (std::regex_match(value, std::regex(item.value()) )) {
            return item.key();
        }
    }
    return "";
  }

  static udf::ScalarUDFDocBuilder Doc() { return AddDoc(); }
};

void RegisterSecurityFuncsOrDie(udf::Registry* registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace px
