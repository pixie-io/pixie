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

#include <memory>
#include <string>
#include <vector>

#include "re2/re2.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace builtins {

using types::StringValue;
using udf::FunctionContext;

struct Tag {
  enum Type {
    IPv4,
    IPv6,
    MAC_ADDR,
    EMAIL_ADDR,
    CC_NUMBER,
    IMEI,
    IMEISV,
  };
  Type tag_type;
  int start_idx;
  size_t size;
};

template <Tag::Type TTag>
struct TagTypeTraits {};

class Tagger {
 public:
  virtual ~Tagger() = default;
  virtual Status AddTags(std::string* input, std::vector<Tag>* tags) = 0;
};

class RedactPIIUDF : public udf::ScalarUDF {
 public:
  Status Init(FunctionContext*);
  StringValue Exec(FunctionContext*, StringValue input);

 private:
  std::vector<std::unique_ptr<Tagger>> taggers_;
};

void RegisterPIIOpsOrDie(udf::Registry* registry);

template <Tag::Type TTag>
class RegexTagger : public Tagger {
 public:
  RegexTagger() : regex_(TagTypeTraits<TTag>::BuildRegexPattern()) {
    // Since the regex patterns are defined at compile time, using a DCHECK is ok here.
    DCHECK_EQ(regex_.error_code(), RE2::NoError) << regex_.error();
  }

  Status AddTags(std::string* input, std::vector<Tag>* tags) {
    re2::StringPiece input_piece(input->data(), input->length());
    auto prev_length = input_piece.length();
    int curr_idx = 0;
    std::string match;
    while (RE2::FindAndConsume(&input_piece, regex_, &match)) {
      auto consumed = prev_length - input_piece.length();
      if (consumed == 0) {
        return Status(statuspb::Code::INVALID_ARGUMENT,
                      "RegexTagger has a regex pattern which matches an empty string.");
      }
      if (!TagTypeTraits<TTag>::Filter(match)) {
        continue;
      }
      auto match_size = match.length();
      // FindAndConsume is not anchored at the start of the StringPiece. To find the start we look
      // at how much more was consumed than matched.
      int start_idx = curr_idx + (consumed - match_size);
      prev_length = input_piece.length();
      curr_idx += consumed;

      tags->push_back(Tag{TTag, start_idx, match_size});
    }
    return Status::OK();
  }

 private:
  re2::RE2 regex_;
};

}  // namespace builtins
}  // namespace carnot
}  // namespace px
