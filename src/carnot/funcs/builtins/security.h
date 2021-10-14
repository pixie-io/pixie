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

#include <rapidjson/document.h>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/shared/types/types.h"
#include "src/carnot/funcs/builtins/regex_ops.h"

namespace px {
namespace carnot {
namespace builtins {

class MatchRegexRule : public udf::ScalarUDF {
 public:
  types::StringValue Exec(FunctionContext*, StringValue value, StringValue regexRules) {
    rapidjson::Document d;
    rapidjson::ParseResult ok = d.Parse(regexRules.data());
    // TODO(zasgar/michellenguyen, PP-419): Replace with null when available.
    if (ok == nullptr) {
      return "";
    }
    RegexMatchUDF regex_match; 
    for (rapidjson::Value::ConstMemberIterator itr = d.MemberBegin(); itr != d.MemberEnd(); ++itr) {
        auto name = itr->name.GetString();
        PL_UNUSED(regex_match.Init(nullptr, itr->value.GetString()));
        auto is_match = regex_match.Exec(nullptr, value).val;
        if (is_match) {
            return name; 
        }
    }
    return "";
  }
};
   
void RegisterSecurityFuncsOrDie(udf::Registry* registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace px
