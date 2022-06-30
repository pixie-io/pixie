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

#include <absl/strings/numbers.h>
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
struct TagTypeTraits<Tag::Type::IPv6> {
  static constexpr std::string_view BuildRegexPattern() {
    // IPv6 has eight groups of 4 hexadecimal digits. However, there are many ways these addresses
    // can be shortened. I try to list and account for all possible shortenings/different
    // representations here:
    //  - leading zeros are suppressed, eg. 0000:0001:0002:0003:0004:0005:0006:0007 =>
    //  0:1:2:3:4:5:6:7
    //  (except an all zero group which is represented as a single zero).
    //  - The longest sequence of all-zero groups is replaced by '::', except if the longest
    //  sequence is only one all-zero group. Also, if there are multiple longest sequences the
    //  leftmost one is shortened. Eg:
    //     abcd:0000:0000:0000:abcd:abcd:abcd:abcd => abcd::abcd:abcd:abcd:abcd
    //     but abcd:0000:1234:abcd:1234:abcd:abcd:abcd => abcd:0:1234:abcd:1234:abcd:abcd:abcd
    //  - The lowest 32bits of a ipv6 address can be represented like an IPv4 address. For example:
    //     0000:0000:0000:0000:0000:1234:ffff:ffff => ::1234:255.255.255.255
    // There's also CIDR notations and zone-indices that can be appended after an IPv6 addr but we
    // will ignore those for this redaction.
    // The regex below was generated in python by enumerating all possible positions of the double
    // colon (including no double colon) and for each possible position there are two cases, one for
    // if the last two groups are IPv4 and one for if they're normal IPv6 syntax.
    //
    // This regex could be greatly simplified if we allowed for an invalid combination of groups
    // before and after the double colon. Since its still really fast to run, for now we'll avoid
    // the false positives of a simpler approach.

    return "((?:(?:(?:[a-fA-F0-9]{1,4}):){6}(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]"
           "|2["
           "0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|2[0-"
           "4][0-9]|[01]?[0-9]{1,2})))"  // No double colon w/ ipv4 syntax
           "|"
           // No groups before double colon w/ ipv4 syntax
           "(?:::(?:(?:[a-fA-F0-9]{1,4}):){0,4}(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25["
           "0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-"
           "5]|2[0-4][0-9]|[01]?[0-9]{1,2})))"
           "|"
           // 1 group before double colon w/ ipv4 syntax
           "(?:(?:[a-fA-F0-9]{1,4})::(?:(?:[a-fA-F0-9]{1,4}):){0,3}(?:(?:25[0-5]|2[0-4][0-9]|[01]?["
           "0-9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-"
           "9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})))"
           "|"
           // 2 groups before double colon w/ ipv4 syntax
           "(?:(?:[a-fA-F0-9]{1,4}):(?:[a-fA-F0-9]{1,4})::(?:(?:[a-fA-F0-9]{1,4}):){0,2}(?:(?:25[0-"
           "5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|"
           "2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})))"
           "|"
           // 3 groups before double colon w/ ipv4 syntax
           "(?:(?:(?:[a-fA-F0-9]{1,4}):){2}(?:[a-fA-F0-9]{1,4})::(?:(?:[a-fA-F0-9]{1,4}):){0,1}(?:("
           "25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25["
           "0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})))"
           "|"
           // 4 groups before double colon w/ ipv4 syntax
           "(?:(?:(?:[a-fA-F0-9]{1,4}):){3}(?:[a-fA-F0-9]{1,4})::(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-"
           "9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{"
           "1,2})\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})))"
           "|"
           // No double colon
           "(?:(?:(?:[a-fA-F0-9]{1,4}):){7}(?:[a-fA-F0-9]{1,4}))"
           "|"
           // No groups before double colon
           "(?:::(?:(?:[a-fA-F0-9]{1,4}):){0,5}(?:[a-fA-F0-9]{1,4})?)"
           "|"
           // 1 group before double colon
           "(?:(?:[a-fA-F0-9]{1,4})::(?:(?:[a-fA-F0-9]{1,4}):){0,4}(?:[a-fA-F0-9]{1,4})?)"
           "|"
           // 2 groups before double colon
           "(?:(?:[a-fA-F0-9]{1,4}):(?:[a-fA-F0-9]{1,4})::(?:(?:[a-fA-F0-9]{1,4}):){0,3}(?:[a-fA-"
           "F0-9]{1,4})?)"
           "|"
           // 3 groups before double colon
           "(?:(?:(?:[a-fA-F0-9]{1,4}):){2}(?:[a-fA-F0-9]{1,4})::(?:(?:[a-fA-F0-9]{1,4}):){0,2}(?:["
           "a-fA-F0-9]{1,4})?)"
           "|"
           // 4 groups before double colon
           "(?:(?:(?:[a-fA-F0-9]{1,4}):){3}(?:[a-fA-F0-9]{1,4})::(?:(?:[a-fA-F0-9]{1,4}):){0,1}(?:["
           "a-fA-F0-9]{1,4})?)"
           "|"
           // 5 groups before double colon
           "(?:(?:(?:[a-fA-F0-9]{1,4}):){4}(?:[a-fA-F0-9]{1,4})::(?:[a-fA-F0-9]{1,4})?)"
           "|"
           // 6 groups before double colon
           "(?:(?:(?:[a-fA-F0-9]{1,4}):){5}(?:[a-fA-F0-9]{1,4})::))";
  }
  static constexpr std::string_view SubstitutionStr() { return "<REDACTED_IPV6>"; }
  static bool Filter(std::string_view) { return true; }
};

template <>
struct TagTypeTraits<Tag::Type::IPv4> {
  static constexpr std::string_view BuildRegexPattern() {
    // Simple dot-decimal regex that allows any number between 0-255 for each group.
    return "((?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})"  // first group
           "\\."
           "(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})"  // second group
           "\\."
           "(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})"  // third group
           "\\."
           "(?:25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2}))";  // fourth group
  }
  static constexpr std::string_view SubstitutionStr() { return "<REDACTED_IPV4>"; }
  static bool Filter(std::string_view) { return true; }
};

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

template <>
struct TagTypeTraits<Tag::Type::MAC_ADDR> {
  static constexpr std::string_view BuildRegexPattern() {
    // MAC Addresses are 48bits and usually represented as 6 octets (6 2 character hexadecimal
    // groups) This regex supports both - and : as delimiters for the 6 octet groups.
    return "((?:[0-9a-fA-F]{2}[-:]){5}[0-9a-fA-F]{2})";
  }
  static constexpr std::string_view SubstitutionStr() { return "<REDACTED_MAC_ADDR>"; }
  static bool Filter(std::string_view) { return true; }
};

static inline bool CheckLuhn(std::string digits) {
  int sum = 0;
  int ndigits = digits.length();
  for (int idx = 0; idx < ndigits - 1; ++idx) {
    int val = digits[(ndigits - 2) - idx] - '0';
    if (idx % 2 == 0) {
      val *= 2;
    }
    if (val > 9) {
      // If val is more than a single digit, we take the sum of its digits. Since val is always less
      // than or eq to 18, we can take the sum of the digits by subtracting 9.
      val -= 9;
    }
    sum += val;
  }
  auto checksum = (10 - (sum % 10)) % 10;
  return (digits[ndigits - 1] - '0') == checksum;
}

template <>
struct TagTypeTraits<Tag::Type::CC_NUMBER> {
  static constexpr std::string_view BuildRegexPattern() {
    // We match all sequences of numbers w/ (spaces or dashes in between), and then filter them down
    // to valid credit cards later.
    return "((?:[0-9][ -]*){12,18}[0-9])";
  }
  static constexpr std::string_view SubstitutionStr() { return "<REDACTED_CC_NUMBER>"; }
  static bool Filter(std::string_view match) {
    std::string match_no_delims(match);
    match_no_delims.erase(std::remove_if(match_no_delims.begin(), match_no_delims.end(),
                                         [](char c) { return c == '-' || c == ' '; }),
                          match_no_delims.end());
    return CheckLuhn(match_no_delims);
  }
};

template <>
struct TagTypeTraits<Tag::Type::IMEI> {
  static constexpr std::string_view BuildRegexPattern() {
    return "([0-9]{2}-[0-9]{6}-[0-9]{6}-[0-9])";
  }
  static constexpr std::string_view SubstitutionStr() { return "<REDACTED_IMEI>"; }
  static bool Filter(std::string_view match) {
    std::string match_no_delims(match);
    match_no_delims.erase(std::remove_if(match_no_delims.begin(), match_no_delims.end(),
                                         [](char c) { return c == '-' || c == ' '; }),
                          match_no_delims.end());
    return CheckLuhn(match_no_delims);
  }
};

template <>
struct TagTypeTraits<Tag::Type::IMEISV> {
  static constexpr std::string_view BuildRegexPattern() {
    return "([0-9]{2}-[0-9]{6}-[0-9]{6}-[0-9]{2})";
  }
  static constexpr std::string_view SubstitutionStr() { return "<REDACTED_IMEI>"; }
  // IMEISV doesn't have a Luhn check digit.
  static bool Filter(std::string_view) { return true; }
};

template <>
struct TagTypeTraits<Tag::Type::IBAN> {
  static constexpr std::string_view BuildRegexPattern() {
    // IBAN as defined in ISO 13616-1:2007 verified via MOD 97 (ISO 7064).
    // Match all sequences beginning with two characters and two digits followed by up to 28 symbols
    // Note: currently the max length of an IBAN is 32 for LC, or 28 symbols after the country code.
    // This may need to be updated as more countries adopt IBAN.
    return "([A-Za-z]{2}[ -]?[0-9]{2}[ -]?([A-Za-z0-9][ -]?){11,28})";
  }
  static constexpr std::string_view SubstitutionStr() { return "<REDACTED_IBAN>"; }
  static bool Filter(std::string_view match) {
    // A International Bank Account Number (IBAN) begins with a two char country code and a two
    // digit checksum
    std::string m(match);
    std::string country_code = m.substr(0, 2);
    std::transform(country_code.begin(), country_code.end(), country_code.begin(), ::toupper);
    // are the first two letters a valid country code?
    if (country_codes.find(country_code) == country_codes.end()) return false;
    m.erase(std::remove_if(m.begin(), m.end(), [](char c) { return c == '-' || c == ' '; }),
            m.end());
    // is the string long enough to be an IBAN for the specified country code?
    if (m.length() < country_codes.at(country_code)) return false;
    // validate IBAN by converting to an int and performing mod-97
    // 1. move country code and checksum to end of string (first four characters)
    // 2. replace each letter with two digits (interpreting char as int)
    // 3. compute remainder of this number when divided by 97. If remainder is one, iban is valid
    // e.g. IBAN: GB82 WEST 1234 5698 7654 32
    // Rearrange: WEST 1234 5698 7654 32GB 82
    // Convert to integer: 3214282912345698765432161182
    // Compute remainder: 3214282912345698765432161182 mod 97 == 1
    std::rotate(m.begin(), m.begin() + 4, m.end());
    std::string nums;
    for (const auto& c : m) {
      if (std::isdigit(c)) nums += c;
      if (std::isupper(c)) nums += std::to_string(static_cast<int>(c) - 55);
    }
    uint64_t start = 0, step = 9, number = 0;
    std::string prepended;
    while (start < nums.length() - step) {
      number = std::stol(prepended + nums.substr(start, step));
      int remainder = number % 97;
      prepended = std::to_string(remainder);
      if (remainder < 10) prepended = "0" + prepended;
      start += step;
      step = 7;
    }
    number = std::stol(prepended + nums.substr(start));
    return (number % 97 == 1);
  }
};

template <>
struct TagTypeTraits<Tag::Type::SSN> {
  static constexpr std::string_view BuildRegexPattern() {
    // US Social Security Numbers are nine-digit numbers in the format AAA-GG-SSSS:
    // AA: Area Code can't be 000, 666, 900-999. GG: Group number can be between 01-99,
    // SSSS: Serial number can be 0001-9999
    // This regex identifies SSNs with spaces or dashes as delimiters e.g. 303-57-7256 or 303 57
    // 7256
    return R"((\d{3}( |-)+\d{2}( |-)+\d{4}))";
  }
  static constexpr std::string_view SubstitutionStr() { return "<REDACTED_SSN>"; }
  static bool Filter(std::string_view match) {
    std::string match_no_delims(match);
    match_no_delims.erase(std::remove_if(match_no_delims.begin(), match_no_delims.end(),
                                         [](char c) { return c == '-' || c == ' '; }),
                          match_no_delims.end());
    std::string_view area = std::string_view(match_no_delims).substr(0, 3);
    std::string_view group = std::string_view(match_no_delims).substr(3, 2);
    std::string_view serial = std::string_view(match_no_delims).substr(5, 4);
    int32_t area_int;
    bool parsed_area_correctly = absl::SimpleAtoi(area, &area_int);
    if (!parsed_area_correctly) return false;

    // AA: Area Code can't be 000, 666, [900-999]
    if (!area.compare("000") || !area.compare("666") || (area_int >= 900 && area_int <= 999))
      return false;
    // GG: Group number can be [01-99]
    if (!group.compare("00")) return false;
    // SSSS: Serial number can be [0001-9999]
    if (!serial.compare("0000")) return false;
    return true;
  }
};

#define SUB_STR(tag_type) \
  { tag_type, TagTypeTraits<tag_type>::SubstitutionStr() }
static std::map<Tag::Type, std::string_view> type_to_sub_str_ = {
    SUB_STR(Tag::Type::IPv6),     SUB_STR(Tag::Type::IPv4),      SUB_STR(Tag::Type::EMAIL_ADDR),
    SUB_STR(Tag::Type::MAC_ADDR), SUB_STR(Tag::Type::CC_NUMBER), SUB_STR(Tag::Type::IMEI),
    SUB_STR(Tag::Type::IMEISV),   SUB_STR(Tag::Type::IBAN),      SUB_STR(Tag::Type::SSN),
};

Status RedactPIIUDF::Init(FunctionContext*) {
  // Order is important here. For example, IPv6 has to go before IPv4 to support IPv6 addresses with
  // the lowest 32 bits written like IPv4. Also Email has to go before IP since IP addresses can be
  // part of valid emails.
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::IBAN>>());
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::EMAIL_ADDR>>());
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::IPv6>>());
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::IPv4>>());
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::MAC_ADDR>>());
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::IMEI>>());
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::IMEISV>>());
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::CC_NUMBER>>());
  taggers_.push_back(std::make_unique<RegexTagger<Tag::Type::SSN>>());
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
