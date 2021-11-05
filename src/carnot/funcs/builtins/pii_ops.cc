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
#include <algorithm>
#include <map>
#include <vector>

#include "src/carnot/funcs/builtins/pii_ops.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterPIIOpsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  registry->RegisterOrDie<RedactPIIUDF>("redact_pii_best_effort");
  /*****************************************
   * Aggregate UDFs.
   *****************************************/
}

template <>
struct TagTypeTraits<Tag::Type::EMAIL_ADDR> {
  static constexpr std::string_view BuildRegexPattern() {
    // This email regex supports basic ASCII email addresses. It is not fully compliant with RFC
    // 5322. For example, it does not support quoting to include special characters, it does not
    // prevent double dots, and it does not allow a list of IP addresses as the domain.
    return "("
           "[0-9a-zA-Z!#$%&\'*+-/=?^_`{|}~.]+"  // addr
           "(?:@|%40)"                          // allow URL encoded "@"
           "(?:[a-zA-Z0-9-]+[.])*[a-zA-Z0-9-]+"
           ")";  // domain
  }
  static constexpr std::string_view SubstitutionStr() { return "<REDACTED_EMAIL>"; }
  static bool Filter(std::string_view) { return true; }
};

#define SUB_STR(tag_type) \
  { tag_type, TagTypeTraits<tag_type>::SubstitutionStr() }
static std::map<Tag::Type, std::string_view> type_to_sub_str_ = {
    SUB_STR(Tag::Type::EMAIL_ADDR),
};

Status RedactPIIUDF::Init(FunctionContext*) {
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::EMAIL_ADDR>>());
  return Status::OK();
}

// Replace all tagged sequences in the string with the corresponding substitution string. For
// overlapping tags, we take the longest tag.
static inline std::string ReplaceTagsWithSubs(std::string input, std::vector<Tag>* tags) {
  // Sort the tags chronologically.
  std::sort(tags->begin(), tags->end(), [](Tag a, Tag b) { return a.start_idx < b.start_idx; });

  // Remove overlapping tags by only keeping the biggest tag for each group of overlapping tags.
  std::vector<Tag> non_overlapping_tags;
  for (auto it = tags->begin(); it < tags->end();) {
    if (it->size == 0) {
      it++;
      continue;
    }
    std::vector<Tag> overlapping_tags;
    auto sub_it = it;
    while (sub_it != tags->end() &&
           sub_it->start_idx < (it->start_idx + static_cast<int>(it->size))) {
      overlapping_tags.push_back(*sub_it);
      sub_it++;
    }
    Tag max_size_tag;
    size_t max_size = 0;
    for (const auto& tag : overlapping_tags) {
      if (tag.size > max_size) {
        max_size_tag = tag;
        max_size = tag.size;
      }
    }
    non_overlapping_tags.push_back(max_size_tag);
    it = tags->erase(it, it + overlapping_tags.size());
  }

  // Calculate new string size.
  size_t new_string_size = input.size();
  for (auto tag : non_overlapping_tags) {
    new_string_size = new_string_size + type_to_sub_str_[tag.tag_type].size() - tag.size;
  }
  // Build new string from old string and non overlapping tags.
  std::string output(new_string_size, 0);
  int input_idx = 0;
  auto data_ptr = output.data();
  for (auto tag : non_overlapping_tags) {
    auto n_copy = input.copy(data_ptr, tag.start_idx - input_idx, input_idx);
    data_ptr += n_copy;
    input_idx = tag.start_idx;

    auto sub_str = type_to_sub_str_[tag.tag_type];
    n_copy = sub_str.copy(data_ptr, sub_str.length());
    data_ptr += n_copy;
    input_idx += tag.size;
  }
  input.copy(data_ptr, input.length() - input_idx, input_idx);
  return output;
}

StringValue RedactPIIUDF::Exec(FunctionContext*, StringValue input) {
  std::vector<Tag> tags;
  for (const auto& tagger : taggers_) {
    auto s = tagger->AddTags(&input, &tags);
    if (!s.ok()) {
      return "Invalid regex: " + s.msg();
    }
  }
  return ReplaceTagsWithSubs(input, &tags);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
