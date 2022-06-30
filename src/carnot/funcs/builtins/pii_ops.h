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

#include <map>
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
    IBAN,
    SSN,
  };
  Type tag_type;
  int start_idx;
  size_t size;
};

template <Tag::Type TTag>
struct TagTypeTraits {};

static std::map<std::string, uint32_t> country_codes{
    {"AL", 28}, {"AD", 24}, {"AT", 20}, {"AZ", 28}, {"BE", 16}, {"BH", 22}, {"BY", 28}, {"BA", 20},
    {"BR", 29}, {"BG", 22}, {"CR", 22}, {"HR", 21}, {"CY", 28}, {"CZ", 24}, {"DK", 18}, {"DO", 28},
    {"EE", 20}, {"FO", 18}, {"FI", 18}, {"FR", 27}, {"GE", 22}, {"DE", 22}, {"GI", 23}, {"GR", 27},
    {"GL", 18}, {"GT", 28}, {"HU", 28}, {"IS", 26}, {"IE", 22}, {"IL", 23}, {"IT", 27}, {"IQ", 23},
    {"JO", 30}, {"KZ", 20}, {"KW", 30}, {"LV", 21}, {"LB", 28}, {"LI", 21}, {"LT", 20}, {"LU", 20},
    {"MK", 19}, {"MT", 31}, {"MR", 27}, {"MU", 30}, {"MC", 27}, {"MD", 24}, {"ME", 22}, {"NL", 18},
    {"NO", 15}, {"PK", 24}, {"PS", 29}, {"PL", 28}, {"PT", 25}, {"RO", 24}, {"SM", 27}, {"SA", 24},
    {"SV", 28}, {"RS", 22}, {"SK", 24}, {"SI", 19}, {"ES", 24}, {"SE", 24}, {"CH", 21}, {"TN", 24},
    {"TR", 26}, {"AE", 23}, {"GB", 22}, {"VG", 24}, {"XK", 20}, {"UA", 29}, {"VA", 22}, {"QA", 29},
    {"LC", 32}, {"ST", 25}, {"TL", 23}, {"SC", 31}};

class Tagger {
 public:
  virtual ~Tagger() = default;
  virtual Status AddTags(std::string* input, std::vector<Tag>* tags) = 0;
};

class RedactPIIUDF : public udf::ScalarUDF {
 public:
  Status Init(FunctionContext*);
  StringValue Exec(FunctionContext*, StringValue input);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Make a best effort to redact Personally Identifiable Information (PII).")
        .Details(
            "Replace instances of PII with '<REDACTED_$TYPE>' eg. '<REDACTED_EMAIL>' or "
            "'<REDACTED_IPv4>'. Currently, it will (on a best effort basis) redact IP addresses, "
            "email addresses, MAC addresses, IMEI numbers, credit card numbers, and IBAN numbers. "
            "However, the "
            "redaction is not perfect, so it should not be used in a context where privacy needs "
            "to be guaranteed.")
        .Example(R"doc(df.redacted = px.redact_pii_best_effort(df.req_body))doc")
        .Arg("input_str", "The string to redact PII from.")
        .Returns("The input string with instances of PII replaced with <REDACTED>.");
  }

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
