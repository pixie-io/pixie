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

#include "src/carnot/funcs/builtins/json_ops.h"
#include "src/carnot/udf/test_utils.h"

namespace px {
namespace carnot {
namespace builtins {

using types::StringValue;

constexpr char kTestJSONStr[] = R"(
{
  "str_key": {"abc": "def"},
  "int64_key": 34243242341,
  "float64_key": 123423.5234,
  "str_plain": "abc"
})";

constexpr char kTestJSONArray[] = R"(["foo", "bar", {"pixie":"labs"}])";

TEST(JSONOps, PluckUDF) {
  auto udf_tester = udf::UDFTester<PluckUDF>();
  udf_tester.ForInput(kTestJSONStr, "str_key").Expect(R"({"abc":"def"})");
  udf_tester.ForInput(kTestJSONStr, "int64_key").Expect("34243242341");
  udf_tester.ForInput(kTestJSONStr, "float64_key").Expect("123423.5234");
  udf_tester.ForInput(kTestJSONStr, "str_plain").Expect("abc");
}

TEST(JSONOps, PluckUDF_non_existent_key_return_empty) {
  auto udf_tester = udf::UDFTester<PluckUDF>();
  udf_tester.ForInput(kTestJSONStr, "blah").Expect("");
}

TEST(JSONOps, PluckUDF_bad_input_return_empty) {
  auto udf_tester = udf::UDFTester<PluckUDF>();
  udf_tester.ForInput("asdad", "str_key").Expect("");
}

TEST(JSONOps, PluckUDF_non_object_input_return_empty) {
  auto udf_tester = udf::UDFTester<PluckUDF>();
  udf_tester.ForInput("[\"asdad\"]", "str_key").Expect("");
}

TEST(JSONOps, PluckAsInt64UDF) {
  auto udf_tester = udf::UDFTester<PluckAsInt64UDF>();
  udf_tester.ForInput(kTestJSONStr, "str_key").Expect(0);
  udf_tester.ForInput(kTestJSONStr, "int64_key").Expect(34243242341);
  udf_tester.ForInput(kTestJSONStr, "float64_key").Expect(0);
  udf_tester.ForInput(kTestJSONStr, "str_plain").Expect(0);
}

TEST(JSONOps, PluckAsInt64UDF_bad_input_return_empty) {
  auto udf_tester = udf::UDFTester<PluckAsInt64UDF>();
  udf_tester.ForInput("sdasdsa", "int64_key").Expect(0);
}

TEST(JSONOps, PluckAsIntUDF_non_object_input_return_empty) {
  auto udf_tester = udf::UDFTester<PluckAsInt64UDF>();
  udf_tester.ForInput("[\"asdad\"]", "int64_key").Expect(0);
}

TEST(JSONOps, PluckAsFloat64UDF) {
  auto udf_tester = udf::UDFTester<PluckAsFloat64UDF>();
  udf_tester.ForInput(kTestJSONStr, "str_key").Expect(0.0);
  udf_tester.ForInput(kTestJSONStr, "int64_key").Expect(0.0);
  udf_tester.ForInput(kTestJSONStr, "float64_key").Expect(123423.5234);
  udf_tester.ForInput(kTestJSONStr, "str_plain").Expect(0.0);
}

TEST(JSONOps, PluckAsFloat64UDF_bad_input_return_empty) {
  auto udf_tester = udf::UDFTester<PluckAsFloat64UDF>();
  udf_tester.ForInput("sdadasd", "float64_key").Expect(0.0);
}

TEST(JSONOps, PluckAsFloatUDF_non_object_input_return_empty) {
  auto udf_tester = udf::UDFTester<PluckAsFloat64UDF>();
  udf_tester.ForInput("[\"asdad\"]", "float64_key").Expect(0.0);
}

TEST(JSONOps, PluckArrayUDF) {
  auto udf_tester = udf::UDFTester<PluckArrayUDF>();
  udf_tester.ForInput(kTestJSONArray, 2).Expect(R"({"pixie":"labs"})");
}

TEST(JSONOps, PluckArrayUDF_input_is_not_array) {
  auto udf_tester = udf::UDFTester<PluckArrayUDF>();
  udf_tester.ForInput(kTestJSONStr, 0).Expect("");
}

TEST(JSONOps, PluckArrayUDF_index_out_of_bound) {
  auto udf_tester = udf::UDFTester<PluckArrayUDF>();
  udf_tester.ForInput(kTestJSONArray, 3).Expect("");
}

TEST(JSONOps, ScriptReferenceUDF_no_args) {
  auto udf_tester = udf::UDFTester<ScriptReferenceUDF<>>();
  auto res = udf_tester.ForInput("text", "px/script").Result();

  rapidjson::Document d;
  d.Parse(res.data());

  EXPECT_TRUE(d.IsObject());
  auto elements = std::distance(d.MemberBegin(), d.MemberEnd());
  EXPECT_EQ(elements, 3);

  EXPECT_TRUE(d["label"].IsString());
  EXPECT_STREQ(d["label"].GetString(), "text");

  EXPECT_TRUE(d["script"].IsString());
  EXPECT_STREQ(d["script"].GetString(), "px/script");

  EXPECT_TRUE(d["args"].IsObject());
  auto args = std::distance(d["args"].MemberBegin(), d["args"].MemberEnd());
  EXPECT_EQ(args, 0);
}

TEST(JSONOps, ScriptReferenceUDF_with_args) {
  auto udf_tester =
      udf::UDFTester<ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue>>();
  auto res = udf_tester
                 .ForInput("text", "px/script", "my_arg_name", "my_arg_value", "my_arg_name2",
                           "my_arg_value2")
                 .Result();

  rapidjson::Document d;
  d.Parse(res.data());

  EXPECT_TRUE(d.IsObject());
  auto elements = std::distance(d.MemberBegin(), d.MemberEnd());
  EXPECT_EQ(elements, 3);

  EXPECT_TRUE(d["label"].IsString());
  EXPECT_STREQ(d["label"].GetString(), "text");

  EXPECT_TRUE(d["script"].IsString());
  EXPECT_STREQ(d["script"].GetString(), "px/script");

  EXPECT_TRUE(d["args"].IsObject());
  auto args = std::distance(d["args"].MemberBegin(), d["args"].MemberEnd());
  EXPECT_EQ(args, 2);

  EXPECT_TRUE(d["args"]["my_arg_name"].IsString());
  EXPECT_STREQ(d["args"]["my_arg_name"].GetString(), "my_arg_value");

  EXPECT_TRUE(d["args"]["my_arg_name2"].IsString());
  EXPECT_STREQ(d["args"]["my_arg_name2"].GetString(), "my_arg_value2");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
