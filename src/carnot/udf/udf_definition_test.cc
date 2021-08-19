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

#include <arrow/builder.h>
#include <arrow/pretty_print.h>

#include <algorithm>

#include "src/carnot/udf/udf_definition.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"

namespace px {
namespace carnot {
namespace udf {

using ::testing::ElementsAre;

class NoArgUDF : public ScalarUDF {
 public:
  types::Int64Value Exec(FunctionContext*) { return invoke_count++; }

 private:
  int invoke_count = 0;
};

class SubStrUDF : public ScalarUDF {
 public:
  types::StringValue Exec(FunctionContext*, types::StringValue str) { return str.substr(1, 2); }
};

class AddUDF : public ScalarUDF {
 public:
  types::Int64Value Exec(FunctionContext*, types::Int64Value v1, types::Int64Value v2) {
    return v1.val + v2.val;
  }
};

class InitArgUDF : public ScalarUDF {
 public:
  Status Init(FunctionContext*, types::StringValue, types::UInt128Value) { return Status::OK(); }
  types::StringValue Exec(FunctionContext*, types::StringValue) { return ""; }
};

TEST(UDFDefinition, no_args) {
  auto ctx = FunctionContext(nullptr, nullptr);
  ScalarUDFDefinition def("noargudf");
  EXPECT_OK(def.Init<NoArgUDF>());

  size_t size = 10;
  types::Int64ValueColumnWrapper out(size);
  auto u = def.Make();
  EXPECT_TRUE(def.ExecBatch(u.get(), &ctx, {}, &out, size).ok());

  EXPECT_EQ(0, out[0].val);
  EXPECT_EQ(1, out[1].val);
  EXPECT_EQ(2, out[2].val);
  EXPECT_EQ(9, out[9].val);
}

TEST(UDFDefinition, two_args) {
  auto ctx = FunctionContext(nullptr, nullptr);
  ScalarUDFDefinition def("add");
  EXPECT_OK(def.Init<AddUDF>());

  types::Int64ValueColumnWrapper v1({1, 2, 3});
  types::Int64ValueColumnWrapper v2({3, 4, 5});

  types::Int64ValueColumnWrapper out(v1.Size());
  auto u = def.Make();
  EXPECT_TRUE(def.ExecBatch(u.get(), &ctx, {&v1, &v2}, &out, v1.Size()).ok());
  EXPECT_EQ(4, out[0].val);
  EXPECT_EQ(6, out[1].val);
  EXPECT_EQ(8, out[2].val);
}

TEST(UDFDefinition, str_args) {
  auto ctx = FunctionContext(nullptr, nullptr);
  ScalarUDFDefinition def("substr");
  EXPECT_OK(def.Init<SubStrUDF>());

  types::StringValueColumnWrapper v1({"abcd", "defg", "hello"});

  types::StringValueColumnWrapper out(v1.Size());
  auto u = def.Make();
  EXPECT_TRUE(def.ExecBatch(u.get(), &ctx, {&v1}, &out, v1.Size()).ok());

  EXPECT_EQ("bc", out[0]);
  EXPECT_EQ("ef", out[1]);
  EXPECT_EQ("el", out[2]);
}

TEST(UDFDefinition, arrow_write) {
  auto ctx = FunctionContext(nullptr, nullptr);
  std::vector<types::Int64Value> v1 = {1, 2, 3};
  std::vector<types::Int64Value> v2 = {3, 4, 5};

  auto v1a = ToArrow(v1, arrow::default_memory_pool());
  auto v2a = ToArrow(v2, arrow::default_memory_pool());

  auto output_builder = std::make_shared<arrow::Int64Builder>();
  auto u = std::make_shared<AddUDF>();
  EXPECT_TRUE(ScalarUDFWrapper<AddUDF>::ExecBatchArrow(u.get(), &ctx, {v1a.get(), v2a.get()},
                                                       output_builder.get(), 3)
                  .ok());

  std::shared_ptr<arrow::Array> res;
  EXPECT_TRUE(output_builder->Finish(&res).ok());
  auto* resArr = static_cast<arrow::Int64Array*>(res.get());
  EXPECT_EQ(4, resArr->Value(0));
  EXPECT_EQ(6, resArr->Value(1));
}

TEST(UDFDefinition, init_args) {
  auto ctx = FunctionContext(nullptr, nullptr);
  ScalarUDFDefinition def("initargudf");
  EXPECT_OK(def.Init<InitArgUDF>());
  EXPECT_EQ(2, def.init_arguments().size());
  EXPECT_THAT(def.init_arguments(), ElementsAre(types::STRING, types::UINT128));
}

// Test UDA, takes the min of two arguments and then sums them.
class MinSumUDA : public udf::UDA {
 public:
  void Update(udf::FunctionContext*, types::Int64Value arg1, types::Int64Value arg2) {
    sum_ = sum_.val + std::min(arg1.val, arg2.val);
  }
  void Merge(udf::FunctionContext*, const MinSumUDA& other) { sum_ = sum_.val + other.sum_.val; }
  types::Int64Value Finalize(udf::FunctionContext*) { return sum_; }

 protected:
  types::Int64Value sum_ = 0;
};

class InitArgUDA : public udf::UDA {
 public:
  Status Init(udf::FunctionContext*, types::Int64Value, types::StringValue, types::BoolValue) {
    return Status::OK();
  }
  void Update(udf::FunctionContext*, types::Int64Value) {}
  void Merge(udf::FunctionContext*, const InitArgUDA&) {}
  types::Int64Value Finalize(udf::FunctionContext*) { return 0; }
};

TEST(UDADefinition, without_merge) {
  auto ctx = FunctionContext(nullptr, nullptr);
  UDADefinition def("minsum");
  EXPECT_OK(def.Init<MinSumUDA>());

  types::Int64ValueColumnWrapper v1({1, 2, 3});
  types::Int64ValueColumnWrapper v2({5, 1, 3});

  types::Int64Value out;
  auto u = def.Make();
  EXPECT_OK(def.ExecBatchUpdate(u.get(), &ctx, {&v1, &v2}));
  EXPECT_OK(def.FinalizeValue(u.get(), &ctx, &out));
  EXPECT_EQ(5, out.val);
}

TEST(UDADefinition, with_merge) {
  auto ctx = FunctionContext(nullptr, nullptr);
  UDADefinition def("minsum");
  EXPECT_OK(def.Init<MinSumUDA>());

  types::Int64ValueColumnWrapper v1({1, 2, 3});
  types::Int64ValueColumnWrapper v2({5, 1, 3});

  types::Int64Value out;
  // Create two uda instances. Send v1, v2 to first and just v1, v1 to second.
  // Then merge.
  auto u1 = def.Make();
  EXPECT_OK(def.ExecBatchUpdate(u1.get(), &ctx, {&v1, &v2}));
  auto u2 = def.Make();
  EXPECT_OK(def.ExecBatchUpdate(u2.get(), &ctx, {&v1, &v1}));
  EXPECT_OK(def.Merge(u1.get(), u2.get(), &ctx));
  EXPECT_OK(def.FinalizeValue(u1.get(), &ctx, &out));
  EXPECT_EQ(11, out.val);
}

TEST(UDADefinition, arrow_output) {
  auto ctx = FunctionContext(nullptr, nullptr);
  UDADefinition def("minsum");
  EXPECT_OK(def.Init<MinSumUDA>());

  types::Int64ValueColumnWrapper v1({1, 2, 3});
  types::Int64ValueColumnWrapper v2({5, 1, 3});

  auto output_builder = std::make_shared<arrow::Int64Builder>();
  auto u = def.Make();
  EXPECT_OK(def.ExecBatchUpdate(u.get(), &ctx, {&v1, &v2}));
  EXPECT_OK(def.FinalizeArrow(u.get(), &ctx, output_builder.get()));

  std::shared_ptr<arrow::Array> res;
  EXPECT_TRUE(output_builder->Finish(&res).ok());
  EXPECT_EQ(1, res->length());
  auto casted = static_cast<arrow::Int64Array*>(res.get());
  EXPECT_EQ(5, casted->Value(0));
}

TEST(UDADefinition, init_args) {
  auto ctx = FunctionContext(nullptr, nullptr);
  UDADefinition def("initarguda");
  EXPECT_OK(def.Init<InitArgUDA>());
  EXPECT_EQ(3, def.init_arguments().size());
  EXPECT_THAT(def.init_arguments(), ElementsAre(types::INT64, types::STRING, types::BOOLEAN));
}

}  // namespace udf
}  // namespace carnot
}  // namespace px
