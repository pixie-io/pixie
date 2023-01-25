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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <type_traits>

#include "src/carnot/udf/udf_wrapper.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace udf {

using ::testing::ElementsAre;

class InvalidUDTF1 : public UDTF<InvalidUDTF1> {
 public:
  static constexpr std::array<int, 1> InitArgs() { return {0}; }
};

TEST(InvalidUDTF1, bad_init_args_fn) {
  using TR = UDTFTraits<InvalidUDTF1>;
  EXPECT_TRUE(TR::HasInitArgsFn());
  EXPECT_FALSE(TR::HasCorrectInitArgsSignature());
}

class InvalidUDTF2 : public UDTF<InvalidUDTF2> {
 public:
  static constexpr auto InitArgs() {
    return MakeArray(UDTFArg::Make<types::INT64>("some_int", "Int arg", 123),
                     UDTFArg::Make<types::STRING>("some_str", "string arg", "init str"));
  }
};

TEST(InvalidUDTF2, missing_init_fn) {
  using TR = UDTFTraits<InvalidUDTF2>;
  EXPECT_TRUE(TR::HasInitArgsFn());
  EXPECT_TRUE(TR::HasCorrectInitArgsSignature());
  EXPECT_FALSE(TR::HasInitFn());
  EXPECT_FALSE(TR::HasExecutorFn());
  EXPECT_FALSE(TR::HasNextRecordFn());
}

class PartialUDTF3 : public UDTF<PartialUDTF3> {
 public:
  static constexpr auto InitArgs() {
    return MakeArray(UDTFArg::Make<types::INT64>("some_int", "Int arg"));
  }

  Status Init(FunctionContext*, types::Int64Value) { return Status::OK(); }
};

TEST(PartialUDTF3, correct_init_types) {
  using TR = UDTFTraits<PartialUDTF3>;
  EXPECT_TRUE(TR::HasInitArgsFn());
  EXPECT_TRUE(TR::HasCorrectInitArgsSignature());
  EXPECT_TRUE(TR::HasInitFn());
  EXPECT_TRUE(TR::HasConsistentInitArgs());
  EXPECT_FALSE(TR::HasExecutorFn());
  EXPECT_FALSE(TR::HasNextRecordFn());
}

class InvalidUDTF4 : public UDTF<InvalidUDTF4> {
 public:
  static constexpr auto InitArgs() {
    return MakeArray(UDTFArg::Make<types::INT64>("some_int", "Int arg"));
  }

  Status Init(FunctionContext*, types::StringValue) { return Status::OK(); }
};

TEST(PartialUDTF3, inconsistent_init_args) {
  using TR = UDTFTraits<InvalidUDTF4>;
  EXPECT_TRUE(TR::HasInitArgsFn());
  EXPECT_TRUE(TR::HasCorrectInitArgsSignature());
  EXPECT_TRUE(TR::HasInitFn());
  EXPECT_FALSE(TR::HasConsistentInitArgs());
  EXPECT_FALSE(TR::HasExecutorFn());
  EXPECT_FALSE(TR::HasNextRecordFn());
}

class InvalidUDTF5 : public UDTF<InvalidUDTF4> {
 public:
  static constexpr int Executor() { return 0; }
};

TEST(PartialUDTF3, bad_executor_func) {
  using TR = UDTFTraits<InvalidUDTF5>;
  EXPECT_TRUE(TR::HasExecutorFn());
  EXPECT_FALSE(TR::HasCorrectExectorFnReturnType());
}

class BasicUDTFOneCol : public UDTF<BasicUDTFOneCol> {
 public:
  static constexpr auto Executor() { return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto InitArgs() {
    return MakeArray(UDTFArg::Make<types::INT64>("some_int", "Int arg"),
                     UDTFArg::Make<types::STRING>("some_string", "String arg"));
  }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("out_str", types::DataType::STRING, types::PatternType::GENERAL, "string result"));
  }

  Status Init(FunctionContext*, types::Int64Value init1, types::StringValue init2) {
    EXPECT_EQ(init1, 1337);
    EXPECT_EQ(init2, "abc");
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    while (idx++ < 2) {
      rw->Append<IndexOf("out_str")>("abc " + std::to_string(idx));
      return true;
    }
    return false;
  }

 private:
  int idx = 0;
};

TEST(BasicUDTFOneCol, can_run) {
  using TR = UDTFTraits<BasicUDTFOneCol>;
  UDTFTraits<BasicUDTFOneCol> traits;
  constexpr auto init_args = traits.InitArgumentTypes();
  ASSERT_THAT(init_args, ElementsAre(types::DataType::INT64, types::DataType::STRING));
  constexpr auto output_rel = traits.OutputRelationTypes();
  ASSERT_THAT(output_rel, ElementsAre(types::DataType::STRING));

  // Check the static assertions.
  constexpr BasicUDTFOneCol::Checker check;
  PX_UNUSED(check);

  EXPECT_TRUE(TR::HasInitArgsFn());
  EXPECT_TRUE(TR::HasCorrectInitArgsSignature());
  EXPECT_TRUE(TR::HasInitFn());
  EXPECT_TRUE(TR::HasConsistentInitArgs());
  EXPECT_TRUE(TR::HasExecutorFn());
  EXPECT_TRUE(TR::HasCorrectExectorFnReturnType());
  EXPECT_TRUE(TR::HasNextRecordFn());

  UDTFWrapper<BasicUDTFOneCol> wrapper;
  PX_UNUSED(wrapper);
  types::Int64Value init1 = 1337;
  types::StringValue init2 = "abc";

  auto u = wrapper.Make();
  ASSERT_NE(u, nullptr);
  EXPECT_OK(wrapper.Init(u.get(), nullptr, {&init1, &init2}));

  arrow::StringBuilder string_builder(0);
  std::vector<arrow::ArrayBuilder*> outs{&string_builder};

  EXPECT_FALSE(wrapper.ExecBatchUpdate(u.get(), nullptr, 100, &outs));

  std::shared_ptr<arrow::StringArray> out;
  EXPECT_TRUE(string_builder.Finish(&out).ok());

  EXPECT_EQ(out->length(), 2);
  EXPECT_EQ(out->GetString(0), "abc 1");
  EXPECT_EQ(out->GetString(1), "abc 2");
}

class BasicUDTFTwoColBad : public UDTF<BasicUDTFTwoColBad> {
 public:
  static constexpr auto Executor() { return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("out_str", types::DataType::STRING, types::PatternType::GENERAL, "string result"),
        ColInfo("int_val", types::DataType::INT64, types::PatternType::GENERAL, "int result"));
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    while (idx++ < 2) {
      rw->Append<IndexOf("out_str")>("abc " + std::to_string(idx));
      return true;
    }
    return false;
  }

 private:
  int idx = 0;
};

TEST(BasicUDTFTwoColBadDeathTest, record_writer_should_catch_bad_append) {
  constexpr BasicUDTFTwoColBad::Checker check;
  PX_UNUSED(check);

  UDTFWrapper<BasicUDTFTwoColBad> wrapper;
  auto u = wrapper.Make();
  ASSERT_NE(u, nullptr);
  EXPECT_OK(wrapper.Init(u.get(), nullptr, {}));

  arrow::StringBuilder string_builder(0);
  arrow::Int64Builder int64_builder(0);
  std::vector<arrow::ArrayBuilder*> outs{&string_builder, &int64_builder};

  EXPECT_DEATH(wrapper.ExecBatchUpdate(u.get(), nullptr, 100, &outs), ".*wrong number.*");
}

class ValidOneColUDTFEmptyInit : public UDTF<ValidOneColUDTFEmptyInit> {
 public:
  static constexpr auto Executor() { return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("out_str", types::DataType::STRING, types::PatternType::GENERAL, "string result"));
  }

  Status Init(FunctionContext*) { return Status::OK(); }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    while (idx++ < 2) {
      rw->Append<IndexOf("out_str")>("abc " + std::to_string(idx));
      return true;
    }
    return false;
  }

 private:
  int idx = 0;
};

TEST(ValidOneColUDTFEmptyInit, empty_init_is_valid) {
  using TR = UDTFTraits<ValidOneColUDTFEmptyInit>;
  constexpr ValidOneColUDTFEmptyInit::Checker check;
  PX_UNUSED(check);

  EXPECT_FALSE(TR::HasInitArgsFn());
  EXPECT_TRUE(TR::HasInitFn());
  EXPECT_FALSE(TR::HasConsistentInitArgs());
  EXPECT_TRUE(TR::HasExecutorFn());
  EXPECT_TRUE(TR::HasCorrectExectorFnReturnType());
  EXPECT_TRUE(TR::HasNextRecordFn());

  constexpr std::array<types::DataType, 0> init_args = TR::InitArgumentTypes();
  EXPECT_EQ(init_args.size(), 0);
}

}  // namespace udf
}  // namespace carnot
}  // namespace px
