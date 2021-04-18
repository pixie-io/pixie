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

#include "src/common/json/json.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <utility>
#include <vector>

namespace px {
namespace utils {

using ::testing::StrEq;

TEST(ToJSONStringTest, MapStringString) {
  std::map<std::string, std::string> map = {
      {"m", "13"}, {"w", "23"}, {"i", "9"},  {"e", "5"}, {"r", "18"}, {"z", "26"}, {"u", "21"},
      {"k", "11"}, {"s", "19"}, {"q", "17"}, {"d", "4"}, {"a", "1"},  {"y", "25"}, {"g", "7"},
      {"p", "16"}, {"b", "2"},  {"c", "3"},  {"h", "8"}, {"x", "24"}, {"v", "22"}, {"j", "10"},
      {"n", "14"}, {"t", "20"}, {"o", "15"}, {"f", "6"}, {"l", "12"}};

  // Note that the output is sorted by key, since std::map is sorted by key.
  EXPECT_THAT(
      ToJSONString(map),
      StrEq(R"({"a":"1","b":"2","c":"3","d":"4","e":"5","f":"6","g":"7","h":"8","i":"9","j":"10",)"
            R"("k":"11","l":"12","m":"13","n":"14","o":"15","p":"16","q":"17","r":"18","s":"19",)"
            R"("t":"20","u":"21","v":"22","w":"23","x":"24","y":"25","z":"26"})"));
}

TEST(ToJSONStringTest, VectorString) {
  std::vector<std::string> vec = {"foo", "bar"};
  EXPECT_THAT(ToJSONString(vec), StrEq(R"(["foo","bar"])"));
}

TEST(ToJSONStringTest, PairStringInt) {
  std::pair<std::string, int> kv_pair = {"pixie", 9};
  EXPECT_THAT(ToJSONString(kv_pair), StrEq(R"({"pixie":9})"));
}

TEST(ToJSONStringTest, VectorPair) {
  std::vector<std::pair<std::string, int>> kv_pairs = {{"pixie", 9}, {"foo", 12}};
  EXPECT_THAT(ToJSONString(kv_pairs), StrEq(R"([{"pixie":9},{"foo":12}])"));
}

TEST(ToJSONStringTest, Nested) {
  std::map<std::string, std::vector<std::string>> obj = {
      {"USA", {"New York", "San Francisco"}}, {"Canada", {"Toronto", "Montreal", "Vancouver"}}};
  EXPECT_THAT(
      ToJSONString(obj),
      StrEq(R"({"Canada":["Toronto","Montreal","Vancouver"],"USA":["New York","San Francisco"]})"));
}

// Tests that JSONObjectBuilder APIs work as expected.
TEST(JSONBuilderTest, ResultsAreAsExpected) {
  JSONObjectBuilder builder;

  builder.WriteKV("year", "2013");
  builder.WriteKV("league", "national");

  std::vector<std::string> names = {"bob", "jack"};
  builder.WriteKV("team", VectorView<std::string>(names));

  EXPECT_THAT(builder.GetString(),
              StrEq(R"({"year":"2013","league":"national","team":["bob","jack"]})"));
}

// Note: if making a test using std::unordered_map, ensure that the test is robust to ordering.

}  // namespace utils
}  // namespace px
