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

#include <gtest/gtest.h>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/carnot/funcs/builtins/pii_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {

using PIITypeGen = std::pair<std::string, std::vector<std::string>>;

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

class RedactionTest : public testing::TestWithParam<TestCase> {};

TEST_P(RedactionTest, basic) {
  auto test_case = GetParam();
  udf::UDFTester<RedactPIIUDF>().Init().ForInput(test_case.first).Expect(test_case.second);
}

INSTANTIATE_TEST_SUITE_P(TemplatedRedactionTest, RedactionTest,
                         testing::ValuesIn(TestCaseGen({EmailGen(), NegativeExampleGen()})));

}  // namespace builtins
}  // namespace carnot
}  // namespace px
