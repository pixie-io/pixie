#include "src/carnot/udf/udf_definition.h"
#include <arrow/builder.h>
#include <arrow/pretty_print.h>
#include <gtest/gtest.h>
#include <iostream>

namespace pl {
namespace carnot {
namespace udf {

class NoArgUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext *) { return invoke_count++; }

 private:
  int invoke_count = 0;
};

class SubStrUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext *, StringValue str) { return str.substr(1, 2); }
};

class AddUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext *, Int64Value v1, Int64Value v2) { return v1.val + v2.val; }
};

TEST(UDFDefinition, no_args) {
  FunctionContext ctx;
  ScalarUDFDefinition def;
  EXPECT_OK(def.Init<NoArgUDF>("noargudf"));

  size_t size = 10;
  std::vector<Int64Value> out(size);
  auto u = def.Make();
  EXPECT_TRUE(def.ExecBatch(u.get(), &ctx, {}, out.data(), size).ok());

  EXPECT_EQ(0, out[0].val);
  EXPECT_EQ(1, out[1].val);
  EXPECT_EQ(2, out[2].val);
  EXPECT_EQ(9, out[9].val);
}

TEST(UDFDefinition, two_args) {
  FunctionContext ctx;
  ScalarUDFDefinition def;
  EXPECT_OK(def.Init<AddUDF>("add"));

  std::vector<Int64Value> v1 = {1, 2, 3};
  std::vector<Int64Value> v2 = {3, 4, 5};

  std::vector<Int64Value> out(v1.size());
  auto u = def.Make();
  EXPECT_TRUE(def.ExecBatch(u.get(), &ctx, {v1.data(), v2.data()}, out.data(), v1.size()).ok());
  EXPECT_EQ(4, out[0].val);
  EXPECT_EQ(6, out[1].val);
  EXPECT_EQ(8, out[2].val);
}

TEST(UDFDefinition, str_args) {
  FunctionContext ctx;
  ScalarUDFDefinition def;
  EXPECT_OK(def.Init<SubStrUDF>("substr"));

  std::vector<StringValue> v1 = {"abcd", "defg", "hello"};

  std::vector<StringValue> out(v1.size());
  auto u = def.Make();
  EXPECT_TRUE(def.ExecBatch(u.get(), &ctx, {v1.data()}, out.data(), v1.size()).ok());

  EXPECT_EQ("bc", out[0]);
  EXPECT_EQ("ef", out[1]);
  EXPECT_EQ("el", out[2]);
}

std::shared_ptr<arrow::Array> ToArrow(const std::vector<int64_t> &vals) {
  std::shared_ptr<arrow::Array> out;
  arrow::Int64Builder builder1;
  CHECK(builder1.AppendValues(vals).ok());
  CHECK(builder1.Finish(&out).ok());
  return out;
}

TEST(UDFDefinition, arrow_write) {
  FunctionContext ctx;
  ScalarUDFDefinition def;
  std::vector<int64_t> v1 = {1, 2, 3};
  std::vector<int64_t> v2 = {3, 4, 5};

  auto v1a = ToArrow(v1);
  auto v2a = ToArrow(v2);

  auto output_builder = std::make_shared<arrow::Int64Builder>();
  auto u = std::make_shared<AddUDF>();
  EXPECT_TRUE(ScalarUDFWrapper<AddUDF>::ExecBatchArrow(u.get(), &ctx, {v1a.get(), v2a.get()},
                                                       output_builder.get(), 3)
                  .ok());

  std::shared_ptr<arrow::Array> res;
  EXPECT_TRUE(output_builder->Finish(&res).ok());
  arrow::Int64Array *resArr = static_cast<arrow::Int64Array *>(res.get());
  EXPECT_EQ(4, resArr->Value(0));
  EXPECT_EQ(6, resArr->Value(1));
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
