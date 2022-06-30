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

#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/carnot/funcs/builtins/pii_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace builtins {

using PIITypeGen = std::pair<std::string, std::vector<std::string>>;

PIITypeGen IPv4Gen() {
  std::vector<std::string> vals{
      "192.168.0.1",
      "255.255.255.255",
      "0.0.0.0",
      "10.0.0.1",
  };
  return std::make_pair("<REDACTED_IPV4>", vals);
}

PIITypeGen IPv6Gen() {
  std::vector<std::string> vals{
      "ab:12:34:56:789:1011:1213:0",
      "00ab:1234:5678:9100:1112:ffff:192.168.0.1",
      "::abcd:1234:ffff",
      "::abcd:1234:ffff:0000:1.1.1.1",
      "0123::abcd:1234:ffff:ab",
      "0123::abcd:1234:ffff:0.0.0.0",
      "0123:abcd::eeee:beef:feed",
      "0123:abcd::eeee:10.0.0.1",
      "0000:abcd:1234::beef:feed:0101",
      "0000:abcd:1234::beef:255.255.255.255",
      "0:1:2:3::beef:feed",
      "0:1:2:3::10.1.2.3",
      "1234:0:1234:a:b::beef",
      "1:2:abcd:a:b:c::",
  };
  return std::make_pair("<REDACTED_IPV6>", vals);
}

PIITypeGen EmailGen() {
  std::vector<std::string> vals{
      "test@pixie.io",
      "test12342341234-abcd____**@mydomain.com.au",
      "dot.dot.dot.dot.dot.dot.dot.dot.dot@abcd-1234-domain",
      "valid_ip_in_email255.255.255.255@mydomain.com",
      "test%40pixie.io",
  };
  return std::make_pair("<REDACTED_EMAIL>", vals);
}

PIITypeGen MACGen() {
  std::vector<std::string> vals{
      "ab:12:00:64:99:ff",
      "00:00:00:00:00:00",
      "ff-ff-ff-ff-ff-ff",
  };
  return std::make_pair("<REDACTED_MAC_ADDR>", vals);
}

PIITypeGen CCGen() {
  std::vector<std::string> vals{
      "3782 8224631 0005",   "3714 4963539 8431",  "3787 3449367 1000",  "5610 5910810 18250",
      "3056 9309025 904",    "3852 0000023 237",   "6011 1111111 11117", "6011 0009901 39424",
      "3530 1113333 00000",  "3566 0020203 60505", "5555 5555555 54444", "5105 1051051 05100",
      "4111 1111111 11111",  "4012 8888888 81881", "4222-2222-222-22",   "5019-7170-1010-3742",
      "6331-1019-9999-0016",
  };
  return std::make_pair("<REDACTED_CC_NUMBER>", vals);
}

PIITypeGen IMEIGen() {
  std::vector<std::string> vals{
      "01-786058-312233-6",  "01-786058-312233-22", "52-635024-498175-3",
      "52-635024-498175-97", "51-942642-588642-2",  "51-942642-588642-55",
  };
  return std::make_pair("<REDACTED_IMEI>", vals);
}

PIITypeGen IBANGen() {
  std::vector<std::string> vals{
      "GB29NWBK60161331926819",
      "GB29 NWBK 6016 1331 9268 19",
      "GB29-NWBK-6016-1331-9268-19",
      "AL47 2121 1009 0000 0002 3569 8741",
      "AD12 0001 2030 2003 5910 0100",
      "AT61 1904 3002 3457 3201",
      "AZ21 NABZ 0000 0000 1370 1000 1944",
      "BH67 BMAG 0000 1299 1234 56",
      "BY13 NBRB 3600 9000 0000 2Z00 AB00",
      "BE68 5390 0754 7034",
      "BA39 1290 0794 0102 8494",
      "BR18 0036 0305 0000 1000 9795 493C 1",
      "BG80 BNBG 9661 1020 3456 78",
      "CR05 0152 0200 1026 2840 66",
      "HR12 1001 0051 8630 0016 0",
      "CY17 0020 0128 0000 0012 0052 7600",
      "CZ65 0800 0000 1920 0014 5399",
      "DK50 0040 0440 1162 43",
      "DO28 BAGR 0000 0001 2124 5361 1324",
      "SV 62 CENR 00000000000000700025",
      "EE38 2200 2210 2014 5685",
      "FO62 6460 0001 6316 34",
      "FI21 1234 5600 0007 85",
  };
  return std::make_pair("<REDACTED_IBAN>", vals);
}

PIITypeGen SSNGen() {
  std::vector<std::string> vals{
      "001-01-0001", "011-11-0011", "111-11-1111", "201-21-0021", "211-11-2011",
      "211-21-2222", "303-58-8256", "255-77-3200", "593-78-6329",
  };
  return std::make_pair("<REDACTED_SSN>", vals);
}

// These values will be injected into templates, but shouldn't be redacted by the pipeline.
PIITypeGen NegativeExampleGen() {
  std::vector<std::string> vals{
      // Hexadecimal string on its own shouldn't be replaced.
      "abcdef00123459689",
      // Invalid IPv4 should not be replaced.
      "999.999.0.0",
      // Invalid IPv6 should not be replaced.
      "1:2:3:4:5:6",
      // Number that looks like a CC but doesn't pass luhn's algo shouldn't be replaced.
      "5555 0000 0000 0000",
      // Invalid SSNs should not be replaced.
      "000-12-1234",  // area number can't be 000
      "666-12-1234",  // area number can't be 666
      "900-12-1234",  // area number can't be [900-999]
      "955-12-1234",  // area number can't be [900-999]
      "999-12-1234",  // area number can't be [900-999]
      "999-00-9999",  // group number can't be 00
      "999-12-0000",  // serial number can't be 0000
  };
  return std::make_pair("UNREDACTED", vals);
}

// Each template is a (number, string) pair where the number represents the number of unique PII
// values that should be inserted into the template.
using TemplGen = std::array<std::pair<size_t, const char*>, 2>;

constexpr TemplGen TemplateGen() {
  TemplGen templates = {{
      {4, R"tmpl(
1 {
  2: $0,
  3: $1,
1 {
  2: $2
  3: $3
}
)tmpl"},
      {3, "$0, $1, $2"},
  }};
  return templates;
}

// TestCase is a (input, expected) pair.
using TestCase = std::pair<std::string, std::string>;

template <size_t... J>
std::string Substitute(std::string templ, const std::vector<std::string>& values,
                       std::index_sequence<J...>) {
  return absl::Substitute(templ, values[J]...);
}

template <size_t... J>
void GenTestCase(std::string templ, const std::vector<PIITypeGen>& pii_types,
                 std::vector<bool>* pii_type_finished, std::vector<size_t>* pii_type_val_indices,
                 std::vector<size_t>* pii_type_indices, std::vector<TestCase>* test_cases,
                 std::index_sequence<J...> index) {
  auto n_values = index.size();
  std::vector<std::string> values(n_values);
  std::vector<std::string> expected_values(n_values);
  for (size_t i = 0; i < n_values; i++) {
    auto idx = i;
    if (idx >= pii_types.size()) {
      idx = idx % pii_types.size();
    }
    auto pii_type_idx = (*pii_type_indices)[idx];
    const auto& pair = pii_types[pii_type_idx];
    auto val_idx = (*pii_type_val_indices)[pii_type_idx];
    values[i] = pair.second[val_idx];
    expected_values[i] = pair.first;
    if (pair.first == "UNREDACTED") {
      expected_values[i] = values[i];
    }
    (*pii_type_val_indices)[pii_type_idx] += 1;
    if ((*pii_type_val_indices)[pii_type_idx] == pair.second.size()) {
      (*pii_type_val_indices)[pii_type_idx] = 0;
      (*pii_type_finished)[pii_type_idx] = true;
    }
  }
  auto test_case_input = Substitute(templ, values, index);
  auto test_case_expected = Substitute(templ, expected_values, index);
  test_cases->emplace_back(test_case_input, test_case_expected);
  std::next_permutation(pii_type_indices->begin(), pii_type_indices->end());
}

template <size_t... I>
void GenTestCaseFromTemplates(const std::vector<PIITypeGen>& pii_types,
                              std::vector<bool>* pii_type_finished,
                              std::vector<size_t>* pii_type_val_indices,
                              std::vector<size_t>* pii_type_indices,
                              std::vector<TestCase>* test_cases, std::index_sequence<I...>) {
  constexpr auto templates = TemplateGen();
  (GenTestCase(std::string(templates[I].second), pii_types, pii_type_finished, pii_type_val_indices,
               pii_type_indices, test_cases, std::make_index_sequence<std::get<0>(templates[I])>()),
   ...);
}

// TestCaseGen will keep creating test cases until each value in each PIITypeGen has been used at
// least once.
std::vector<TestCase> TestCaseGen(std::vector<PIITypeGen> pii_types) {
  std::vector<TestCase> test_cases;
  std::vector<bool> pii_type_finished(pii_types.size(), false);
  std::vector<size_t> pii_type_vals_indices(pii_types.size(), 0);
  std::vector<size_t> pii_type_indices;
  for (size_t i = 0; i < pii_types.size(); ++i) {
    pii_type_indices.push_back(i);
  }
  constexpr auto templates = TemplateGen();
  while (!std::all_of(pii_type_finished.begin(), pii_type_finished.end(),
                      [](bool finished) { return finished; })) {
    GenTestCaseFromTemplates(pii_types, &pii_type_finished, &pii_type_vals_indices,
                             &pii_type_indices, &test_cases,
                             std::make_index_sequence<templates.size()>());
  }
  return test_cases;
}

class RedactionTest : public ::testing::TestWithParam<TestCase> {};

TEST_P(RedactionTest, basic) {
  auto test_case = GetParam();
  udf::UDFTester<RedactPIIUDF>().Init().ForInput(test_case.first).Expect(test_case.second);
}

INSTANTIATE_TEST_SUITE_P(TemplatedRedactionTest, RedactionTest,
                         ::testing::ValuesIn(TestCaseGen({IBANGen(), IPv4Gen(), IPv6Gen(),
                                                          EmailGen(), CCGen(), IMEIGen(), SSNGen(),
                                                          NegativeExampleGen()})));

}  // namespace builtins
}  // namespace carnot
}  // namespace px
