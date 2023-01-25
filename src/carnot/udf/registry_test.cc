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

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <type_traits>

#include <absl/strings/match.h>
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace udf {

using ::px::testing::proto::EqualsProto;

class ScalarUDF1 : public ScalarUDF {
 public:
  types::Int64Value Exec(FunctionContext* ctx, types::BoolValue b1, types::Int64Value b2) {
    PX_UNUSED(ctx);
    return b1.val && (b2.val != 0) ? 3 : 0;
  }
  static ScalarUDFDocBuilder Doc() {
    return ScalarUDFDocBuilder("This function adds two numbers: c = a + b")
        .Arg("b1", "some arg")
        .Arg("b2", "some other arg")
        .Returns("returns something");
  }
};

class ScalarUDF1WithInit : public ScalarUDF {
 public:
  Status Init(FunctionContext* ctx, types::Int64Value v1) {
    PX_UNUSED(ctx);
    PX_UNUSED(v1);
    return Status::OK();
  }
  types::Int64Value Exec(FunctionContext* ctx, types::BoolValue b1, types::BoolValue b2) {
    PX_UNUSED(ctx);
    return b1.val && b2.val ? 3 : 0;
  }
};

class ScalarUDFWithConflictingInit : public ScalarUDF {
 public:
  Status Init(FunctionContext*, types::Int64Value, types::BoolValue) { return Status::OK(); }
  types::Int64Value Exec(FunctionContext*, types::BoolValue) { return 0; }
};

template <typename TOutput, typename TInput1, typename TInput2>
class AddUDF : public ScalarUDF {
 public:
  TOutput Exec(FunctionContext* ctx, TInput1 v1, TInput2 v2) {
    PX_UNUSED(ctx);
    return v1.val + v2.val;
  }
};

TEST(Registry, init_with_udfs) {
  Registry registry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<ScalarUDF1WithInit>("scalar1WithInit");

  auto statusor = registry.GetScalarUDFDefinition(
      "scalar1", std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64}));
  ASSERT_OK(statusor);
  auto def = statusor.ConsumeValueOrDie();
  ASSERT_NE(nullptr, def);
  EXPECT_EQ("scalar1", def->name());
  EXPECT_EQ(types::DataType::INT64, def->exec_return_type());
  EXPECT_EQ(std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64}),
            def->exec_arguments());

  const char* expected_debug_str =
      "udf::Registry: test registry\n"
      "scalar1\n"
      "scalar1WithInit\n";
  EXPECT_EQ(expected_debug_str, registry.DebugString());

  statusor = registry.GetScalarUDFDefinition(
      "scalar1WithInit",
      std::vector<types::DataType>(
          {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::BOOLEAN}));
  ASSERT_OK(statusor);
  def = statusor.ConsumeValueOrDie();
  ASSERT_NE(nullptr, def);
  EXPECT_EQ("scalar1WithInit", def->name());
  EXPECT_EQ(types::DataType::INT64, def->exec_return_type());
  EXPECT_EQ(std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::BOOLEAN}),
            def->exec_arguments());
  EXPECT_EQ(std::vector<types::DataType>({types::DataType::INT64}), def->init_arguments());
}

TEST(Registry, conflicting_init_exec_args) {
  Registry registry("test registry");
  registry.RegisterOrDie<ScalarUDF1WithInit>("scalar1WithInit");
  auto s = registry.Register<ScalarUDFWithConflictingInit>("scalar1WithInit");
  EXPECT_NOT_OK(s);
  EXPECT_EQ(
      "The UDF with name \"scalar1WithInit\" already exists with the same arg types "
      "\"scalar1WithInit(INT64,BOOLEAN,BOOLEAN)\".",
      s.msg());
}

TEST(Registry, templated_udfs) {
  auto registry = Registry("test registry");
  registry.RegisterOrDie<AddUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "add");
  registry.RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
      "add");

  auto statusor = registry.GetScalarUDFDefinition(
      "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64}));
  ASSERT_OK(statusor);
  EXPECT_NE(nullptr, statusor.ConsumeValueOrDie());

  statusor = registry.GetScalarUDFDefinition(
      "add", std::vector<types::DataType>({types::DataType::FLOAT64, types::DataType::FLOAT64}));
  ASSERT_OK(statusor);
  EXPECT_NE(nullptr, statusor.ConsumeValueOrDie());

  statusor = registry.GetScalarUDFDefinition(
      "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::INT64}));
  ASSERT_FALSE(statusor.ok());
  EXPECT_TRUE(error::IsNotFound(statusor.status()));
}

TEST(Registry, double_register) {
  auto registry = Registry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<ScalarUDF1>("scalar1WithInit");
  auto status = registry.Register<ScalarUDF1>("scalar1");
  EXPECT_NOT_OK(status);
  EXPECT_TRUE(error::IsAlreadyExists(status));
  EXPECT_TRUE(absl::StrContains(status.msg(), "scalar1"));
  EXPECT_TRUE(absl::StrContains(status.msg(), "already exists"));
}

TEST(Registry, no_such_udf) {
  auto registry = Registry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  auto statusor = registry.GetScalarUDFDefinition(
      "scalar1", std::vector<types::DataType>({types::DataType::INT64}));
  EXPECT_NOT_OK(statusor);
}

TEST(RegistryDeathTest, double_register) {
  auto registry = Registry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<ScalarUDF1>("scalar1WithInit");

  EXPECT_DEATH(registry.RegisterOrDie<ScalarUDF1>("scalar1"), ".*already exists.*");
}

class UDA1 : public UDA {
 public:
  Status Init(FunctionContext*, types::StringValue) { return Status::OK(); }
  void Update(FunctionContext*, types::Int64Value) {}
  void Merge(FunctionContext*, const UDA1&) {}
  types::Int64Value Finalize(FunctionContext*) { return 0; }
  static UDADocBuilder Doc() {
    return UDADocBuilder("This function computes the sum of a list of numbers.")
        .Details("The detailed version of this.")
        .Arg("a", "random init arg")
        .Arg("b", "The argument to sum")
        .Returns("The sum of all values of b.")
        .Example("df.sum = df.agg");
  }
};

class UDA1Overload : public UDA {
 public:
  Status Init(FunctionContext*, types::StringValue) { return Status::OK(); }
  void Update(FunctionContext*, types::Int64Value, types::Float64Value) {}
  void Merge(FunctionContext*, const UDA1Overload&) {}
  types::Float64Value Finalize(FunctionContext*) { return 0; }
};

TEST(Registry, init_with_udas) {
  auto registry = Registry("test registry");
  registry.RegisterOrDie<UDA1>("uda1");
  registry.RegisterOrDie<UDA1Overload>("uda1");

  auto statusor = registry.GetUDADefinition(
      "uda1", std::vector<types::DataType>({types::DataType::STRING, types::DataType::INT64}));
  ASSERT_OK(statusor);
  auto def = statusor.ConsumeValueOrDie();
  ASSERT_NE(nullptr, def);
  EXPECT_EQ("uda1", def->name());
  EXPECT_EQ(std::vector<types::DataType>({types::DataType::INT64}), def->update_arguments());
  EXPECT_EQ(types::DataType::INT64, def->finalize_return_type());
  EXPECT_EQ(std::vector<types::DataType>({types::DataType::STRING}), def->init_arguments());

  statusor = registry.GetUDADefinition(
      "uda1", std::vector<types::DataType>(
                  {types::DataType::STRING, types::DataType::INT64, types::DataType::FLOAT64}));
  ASSERT_OK(statusor);
  def = statusor.ConsumeValueOrDie();
  ASSERT_NE(nullptr, def);
  EXPECT_EQ("uda1", def->name());
  EXPECT_EQ(std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64}),
            def->update_arguments());
  EXPECT_EQ(types::DataType::FLOAT64, def->finalize_return_type());
  EXPECT_EQ(std::vector<types::DataType>({types::DataType::STRING}), def->init_arguments());

  const char* expected_debug_str =
      "udf::Registry: test registry\n"
      "uda1\n"
      "uda1\n";
  EXPECT_EQ(expected_debug_str, registry.DebugString());
}

TEST(Registry, double_register_uda) {
  auto registry = Registry("test registry");
  registry.RegisterOrDie<UDA1>("uda1");
  registry.RegisterOrDie<UDA1>("uda2");
  auto status = registry.Register<UDA1>("uda1");
  EXPECT_NOT_OK(status);
  EXPECT_TRUE(error::IsAlreadyExists(status));
  EXPECT_TRUE(absl::StrContains(status.msg(), "uda1"));
  EXPECT_TRUE(absl::StrContains(status.msg(), "already exists"));
}

TEST(Registry, no_such_uda) {
  auto registry = Registry("test registry");
  registry.RegisterOrDie<UDA1>("uda1");
  auto statusor =
      registry.GetUDADefinition("uda1", std::vector<types::DataType>({types::DataType::FLOAT64}));
  EXPECT_NOT_OK(statusor);
}

TEST(RegistryDeathTest, double_register_uda) {
  auto registry = Registry("test registry");
  registry.RegisterOrDie<UDA1>("uda1");
  EXPECT_DEATH(registry.RegisterOrDie<UDA1>("uda1"), ".*already exists.*");
}

constexpr char kExpectedUDFInfo[] = R"(
udas {
  name: "uda1"
  init_arg_types: STRING
  update_arg_types: INT64
  finalize_type: INT64
}
scalar_udfs {
  name: "add"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: FLOAT64
  executor: UDF_ALL
}
scalar_udfs {
  name: "scalar1"
  exec_arg_types: BOOLEAN
  exec_arg_types: INT64
  return_type: INT64
  executor: UDF_ALL
}
)";

TEST(Registry, ToProto) {
  auto registry = Registry("test registry");
  registry.RegisterOrDie<UDA1>("uda1");

  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
      "add");

  auto udf_info = registry.ToProto();

  udfspb::UDFInfo expected_udf_info;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &expected_udf_info));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_udf_info, udf_info));
}

class BasicUDTFTwoCol : public UDTF<BasicUDTFTwoCol> {
 public:
  static constexpr auto Executor() { return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("out_str", types::DataType::STRING, types::PatternType::GENERAL, "string result"),
        ColInfo("int_val", types::DataType::INT64, types::PatternType::GENERAL, "int result"));
  }

  bool NextRecord(FunctionContext*, RecordWriter*) { return false; }
};

class BasicUDTFTwoColOverload : public UDTF<BasicUDTFTwoColOverload> {
 public:
  static constexpr auto Executor() { return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("out_str", types::DataType::STRING, types::PatternType::GENERAL, "string result"));
  }

  bool NextRecord(FunctionContext*, RecordWriter*) { return false; }
};

TEST(Registry, init_with_udtf) {
  Registry registry("test registry");
  registry.RegisterOrDie<BasicUDTFTwoCol>("test_udtf");

  auto statusor = registry.GetUDTFDefinition("test_udtf");
  ASSERT_OK(statusor);
  auto def = statusor.ConsumeValueOrDie();
  ASSERT_NE(nullptr, def);
  EXPECT_EQ("test_udtf", def->name());

  const char* expected_debug_str =
      "udf::Registry: test registry\n"
      "test_udtf\n";
  EXPECT_EQ(expected_debug_str, registry.DebugString());
}

TEST(RegistryDeathTest, udtf_does_not_allow_overload) {
  Registry registry("test registry");
  registry.RegisterOrDie<BasicUDTFTwoCol>("test_udtf");
  EXPECT_DEATH(registry.RegisterOrDie<BasicUDTFTwoColOverload>("test_udtf"), ".*already exists.*");
}

class UDTFWithConstructor : public UDTF<UDTFWithConstructor> {
 public:
  UDTFWithConstructor() = delete;
  explicit UDTFWithConstructor(int x) { x_ = x; }
  static constexpr auto Executor() { return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("out_str", types::DataType::STRING, types::PatternType::GENERAL, "string result"));
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    while (idx++ < 2) {
      rw->Append<IndexOf("out_str")>("abc " + std::to_string(idx));
      return true;
    }
    return false;
  }

  int x() { return x_; }

  class Factory final : public UDTFFactory {
   public:
    Factory() = delete;
    explicit Factory(int x) : x_(x) {}
    std::unique_ptr<AnyUDTF> Make() override { return std::make_unique<UDTFWithConstructor>(x_); }

   private:
    int x_ = 0;
  };

 private:
  int idx = 0;
  int x_ = 0;
};

class BasicUDTFOneCol : public UDTF<BasicUDTFOneCol> {
 public:
  static constexpr auto Executor() { return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto InitArgs() {
    return MakeArray(UDTFArg::Make<types::INT64>("some_int", "Int arg", 123),
                     UDTFArg::Make<types::STRING>("some_string", "String arg"));
  }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("out_str", types::DataType::STRING, types::PatternType::GENERAL, "string result"));
  }

  Status Init(FunctionContext*, types::Int64Value, types::StringValue) { return Status::OK(); }

  bool NextRecord(FunctionContext*, RecordWriter*) { return false; }
};

TEST(Registry, init_with_factory) {
  Registry registry("test registry");
  registry.RegisterFactoryOrDie<UDTFWithConstructor, UDTFWithConstructor::Factory>("test_udtf",
                                                                                   /*x*/ 100);
  auto* def = registry.GetUDTFDefinition("test_udtf").ConsumeValueOrDie();
  auto inst = def->Make();
  UDTFWithConstructor* u = static_cast<UDTFWithConstructor*>(inst.get());
  EXPECT_EQ(u->x(), 100);

  const auto& output_rel = def->output_relation();
  EXPECT_EQ(output_rel.size(), 1);
  EXPECT_EQ(output_rel[0].name(), "out_str");
  EXPECT_EQ(output_rel[0].type(), types::STRING);
  EXPECT_EQ(output_rel[0].ptype(), types::PatternType::GENERAL);
  EXPECT_EQ(std::string(output_rel[0].desc()), "string result");

  EXPECT_EQ(def->executor(), udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS);
}

constexpr char kExpectedUDTFInfo[] = R"pbtxt(
udtfs {
  name: "test_udtf"
  args {
    name: "some_int"
    arg_type: INT64
    semantic_type: ST_NONE
    default_value {
      int64_value: 123
    }
  }
  args {
    name: "some_string"
    arg_type: STRING
    semantic_type: ST_NONE
  }
  executor: UDTF_ALL_AGENTS
  relation {
    columns {
      column_name: "out_str"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
)pbtxt";

TEST(Registry, udtf_to_proto) {
  Registry registry("test registry");
  registry.RegisterOrDie<BasicUDTFOneCol>("test_udtf");
  auto udf_info = registry.ToProto();

  udfspb::UDFInfo expected_udf_info;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedUDTFInfo, &expected_udf_info));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_udf_info, udf_info));
}

constexpr char kExpectedUDFInfoWithTypes[] = R"pbtxt(
udas {
  name: "uda1"
  update_arg_types: INT64
  finalize_type: INT64
}
scalar_udfs {
  name: "scalar1"
  exec_arg_types: BOOLEAN
  exec_arg_types: INT64
  return_type: INT64
  executor: UDF_ALL
}
semantic_type_rules {
  name: "scalar1"
  udf_exec_type: SCALAR_UDF
  exec_arg_types: ST_NONE
  exec_arg_types: ST_BYTES
  output_type: ST_BYTES
}
semantic_type_rules {
  name: "uda1"
  udf_exec_type: UDA
  update_arg_types: ST_BYTES
  output_type: ST_BYTES
}
)pbtxt";

class TypedUDA1 : public UDA {
 public:
  Status Init(FunctionContext*) { return Status::OK(); }
  void Update(FunctionContext*, types::Int64Value) {}
  void Merge(FunctionContext*, const TypedUDA1&) {}
  types::Int64Value Finalize(FunctionContext*) { return 0; }

  static InfRuleVec SemanticInferenceRules() {
    return {
        InheritTypeFromArgs<TypedUDA1>::Create({types::ST_BYTES}),
    };
  }
};

class TypedScalarUDF1 : public ScalarUDF {
 public:
  types::Int64Value Exec(FunctionContext* ctx, types::BoolValue b1, types::Int64Value b2) {
    PX_UNUSED(ctx);
    return b1.val && (b2.val != 0) ? b2.val : 0;
  }
  static InfRuleVec SemanticInferenceRules() {
    return {
        InheritTypeFromArgs<TypedScalarUDF1>::Create({types::ST_BYTES}, {1}),
    };
  }
};

TEST(Registry, semantic_type_rules) {
  Registry registry("test_registry");
  registry.RegisterOrDie<TypedUDA1>("uda1");
  registry.RegisterOrDie<TypedScalarUDF1>("scalar1");
  auto udf_info = registry.ToProto();

  udfspb::UDFInfo expected_udf_info;
  EXPECT_THAT(udf_info, EqualsProto(kExpectedUDFInfoWithTypes));
}

class TypedOverloadUDA1 : public UDA {
 public:
  Status Init(FunctionContext*) { return Status::OK(); }
  void Update(FunctionContext*, types::Float64Value) {}
  void Merge(FunctionContext*, const TypedOverloadUDA1&) {}
  types::Int64Value Finalize(FunctionContext*) { return 0; }

  static InfRuleVec SemanticInferenceRules() {
    return {
        InheritTypeFromArgs<TypedOverloadUDA1>::Create({types::ST_BYTES}),
    };
  }
};

constexpr char kExpectedUDFInfoWithOverloadedTypes[] = R"pbtxt(
udas {
  name: "uda1"
  update_arg_types: INT64
  finalize_type: INT64
}
udas {
  name: "uda1"
  update_arg_types: FLOAT64
  finalize_type: INT64
}
semantic_type_rules {
  name: "uda1"
  udf_exec_type: UDA
  update_arg_types: ST_BYTES
  output_type: ST_BYTES
}
)pbtxt";

TEST(Registry, semantic_type_rules_shouldnt_be_duped_for_overloaded_raw_types) {
  Registry registry("test_registry");
  registry.RegisterOrDie<TypedUDA1>("uda1");
  registry.RegisterOrDie<TypedOverloadUDA1>("uda1");
  auto udf_info = registry.ToProto();

  udfspb::UDFInfo expected_udf_info;
  EXPECT_THAT(udf_info, EqualsProto(kExpectedUDFInfoWithOverloadedTypes));
}

class TypedUDA2 : public UDA {
 public:
  Status Init(FunctionContext*) { return Status::OK(); }
  void Update(FunctionContext*, types::Int64Value) {}
  void Merge(FunctionContext*, const TypedUDA2&) {}
  types::Int64Value Finalize(FunctionContext*) { return 0; }

  static InfRuleVec SemanticInferenceRules() {
    return {
        InheritTypeFromArgs<TypedUDA2>::Create({types::ST_BYTES}),
    };
  }
};

class TypedOverloadUDA2 : public UDA {
 public:
  Status Init(FunctionContext*) { return Status::OK(); }
  void Update(FunctionContext*, types::Float64Value, types::Int64Value) {}
  void Merge(FunctionContext*, const TypedOverloadUDA2&) {}
  types::Int64Value Finalize(FunctionContext*) { return 0; }

  static InfRuleVec SemanticInferenceRules() {
    return {
        InheritTypeFromArgs<TypedOverloadUDA2>::Create({types::ST_BYTES}, {1}),
    };
  }
};

constexpr char kExpectedUDFInfoWithOverloadedTypes2[] = R"pbtxt(
udas {
  name: "uda2"
  update_arg_types: INT64
  finalize_type: INT64
}
udas {
  name: "uda2"
  update_arg_types: FLOAT64
  update_arg_types: INT64
  finalize_type: INT64
}
semantic_type_rules {
  name: "uda2"
  udf_exec_type: UDA
  update_arg_types: ST_NONE
  update_arg_types: ST_BYTES
  output_type: ST_BYTES
}
semantic_type_rules {
  name: "uda2"
  udf_exec_type: UDA
  update_arg_types: ST_BYTES
  output_type: ST_BYTES
}
)pbtxt";

TEST(Registry, semantic_type_rules_should_have_both_rules_if_overloaded_and_different) {
  Registry registry("test_registry");
  registry.RegisterOrDie<TypedUDA2>("uda2");
  registry.RegisterOrDie<TypedOverloadUDA2>("uda2");
  auto udf_info = registry.ToProto();

  udfspb::UDFInfo expected_udf_info;
  EXPECT_THAT(udf_info, EqualsProto(kExpectedUDFInfoWithOverloadedTypes2));
}

auto expectedDocsPbTxt = R"(
udf {
  name: "scalar1"
  brief: "This function adds two numbers: c = a + b"
  scalar_udf_doc {
    args {
      ident: "b1"
      desc: "some arg"
      type: BOOLEAN
    }
    args {
      ident: "b2"
      desc: "some other arg"
      type: INT64
    }
    retval {
      desc: "returns something"
      type: INT64
    }
  }
}
udf {
  name: "count"
  brief: "This function computes the sum of a list of numbers."
  desc: "The detailed version of this."
  examples {
    value: "df.sum = df.agg"
  }
  uda_doc {
    update_args {
      ident: "a"
      desc: "random init arg"
      type: STRING
    }
    update_args {
      ident: "b"
      desc: "The argument to sum"
      type: INT64
    }
    result {
      desc: "The sum of all values of b."
      type: INT64
    }
  }
}
)";

TEST(Registry, docs) {
  Registry registry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<UDA1>("count");
  udfspb::Docs docs = registry.ToDocsProto();
  EXPECT_THAT(docs, EqualsProto(expectedDocsPbTxt));
}

}  // namespace udf
}  // namespace carnot
}  // namespace px
