#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <type_traits>

#include <absl/strings/match.h>
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace udf {

class ScalarUDF1 : public ScalarUDF {
 public:
  types::Int64Value Exec(FunctionContext* ctx, types::BoolValue b1, types::Int64Value b2) {
    PL_UNUSED(ctx);
    return b1.val && (b2.val != 0) ? 3 : 0;
  }
};

class ScalarUDF1WithInit : public ScalarUDF {
 public:
  Status Init(FunctionContext* ctx, types::Int64Value v1) {
    PL_UNUSED(ctx);
    PL_UNUSED(v1);
    return Status::OK();
  }
  types::Int64Value Exec(FunctionContext* ctx, types::BoolValue b1, types::BoolValue b2) {
    PL_UNUSED(ctx);
    return b1.val && b2.val ? 3 : 0;
  }
};

template <typename TOutput, typename TInput1, typename TInput2>
class AddUDF : public ScalarUDF {
 public:
  TOutput Exec(FunctionContext* ctx, TInput1 v1, TInput2 v2) {
    PL_UNUSED(ctx);
    return v1.val + v2.val;
  }
};

TEST(ScalarUDFRegistry, init_with_udfs) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<ScalarUDF1WithInit>("scalar1WithInit");
  EXPECT_EQ(kScalarUDF, registry.Type());

  auto statusor = registry.GetDefinition(
      "scalar1", std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64}));
  ASSERT_OK(statusor);
  auto def = statusor.ConsumeValueOrDie();
  ASSERT_NE(nullptr, def);
  EXPECT_EQ("scalar1", def->name());
  EXPECT_EQ(types::DataType::INT64, def->exec_return_type());
  EXPECT_EQ(std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64}),
            def->exec_arguments());

  const char* expected_debug_str =
      "Registry(kScalarUDF): test registry\n"
      "scalar1\n"
      "scalar1WithInit\n";
  EXPECT_EQ(expected_debug_str, registry.DebugString());
}

TEST(ScalarUDFRegistry, templated_udfs) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<AddUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "add");
  registry.RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
      "add");

  auto statusor = registry.GetDefinition(
      "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64}));
  ASSERT_OK(statusor);
  EXPECT_NE(nullptr, statusor.ConsumeValueOrDie());

  statusor = registry.GetDefinition(
      "add", std::vector<types::DataType>({types::DataType::FLOAT64, types::DataType::FLOAT64}));
  ASSERT_OK(statusor);
  EXPECT_NE(nullptr, statusor.ConsumeValueOrDie());

  statusor = registry.GetDefinition(
      "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::INT64}));
  ASSERT_FALSE(statusor.ok());
  EXPECT_TRUE(error::IsNotFound(statusor.status()));
}

TEST(ScalarUDFRegistry, double_register) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<ScalarUDF1>("scalar1WithInit");
  auto status = registry.Register<ScalarUDF1>("scalar1");
  EXPECT_NOT_OK(status);
  EXPECT_TRUE(error::IsAlreadyExists(status));
  EXPECT_TRUE(absl::StrContains(status.msg(), "scalar1"));
  EXPECT_TRUE(absl::StrContains(status.msg(), "already exists"));
}

TEST(ScalarUDFRegistry, no_such_udf) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  auto statusor =
      registry.GetDefinition("scalar1", std::vector<types::DataType>({types::DataType::INT64}));
  EXPECT_NOT_OK(statusor);
}

TEST(ScalarUDFRegistryDeathTest, double_register) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<ScalarUDF1>("scalar1WithInit");

  EXPECT_DEATH(registry.RegisterOrDie<ScalarUDF1>("scalar1"), ".*already exists.*");
}

class UDA1 : public UDA {
 public:
  Status Init(FunctionContext*) { return Status::OK(); }
  void Update(FunctionContext*, types::Int64Value) {}
  void Merge(FunctionContext*, const UDA1&) {}
  types::Int64Value Finalize(FunctionContext*) { return 0; }
};

class UDA1Overload : public UDA {
 public:
  Status Init(FunctionContext*) { return Status::OK(); }
  void Update(FunctionContext*, types::Int64Value, types::Float64Value) {}
  void Merge(FunctionContext*, const UDA1Overload&) {}
  types::Float64Value Finalize(FunctionContext*) { return 0; }
};

TEST(UDARegistry, init_with_udas) {
  auto registry = UDARegistry("test registry");
  registry.RegisterOrDie<UDA1>("uda1");
  registry.RegisterOrDie<UDA1Overload>("uda1");

  EXPECT_EQ(kUDA, registry.Type());
  auto statusor =
      registry.GetDefinition("uda1", std::vector<types::DataType>({types::DataType::INT64}));
  ASSERT_OK(statusor);
  auto def = statusor.ConsumeValueOrDie();
  ASSERT_NE(nullptr, def);
  EXPECT_EQ("uda1", def->name());
  EXPECT_EQ(std::vector<types::DataType>({types::DataType::INT64}), def->update_arguments());
  EXPECT_EQ(types::DataType::INT64, def->finalize_return_type());

  const char* expected_debug_str =
      "Registry(kUDA): test registry\n"
      "uda1\n"
      "uda1\n";
  EXPECT_EQ(expected_debug_str, registry.DebugString());
}

TEST(UDARegistry, double_register) {
  auto registry = UDARegistry("test registry");
  registry.RegisterOrDie<UDA1>("uda1");
  registry.RegisterOrDie<UDA1>("uda2");
  auto status = registry.Register<UDA1>("uda1");
  EXPECT_NOT_OK(status);
  EXPECT_TRUE(error::IsAlreadyExists(status));
  EXPECT_TRUE(absl::StrContains(status.msg(), "uda1"));
  EXPECT_TRUE(absl::StrContains(status.msg(), "already exists"));
}

TEST(UDARegistry, no_such_uda) {
  auto registry = UDARegistry("test registry");
  registry.RegisterOrDie<UDA1>("uda1");
  auto statusor =
      registry.GetDefinition("uda1", std::vector<types::DataType>({types::DataType::FLOAT64}));
  EXPECT_NOT_OK(statusor);
}

TEST(UDARegistryDeathTest, double_register) {
  auto registry = UDARegistry("test registry");
  registry.RegisterOrDie<UDA1>("uda1");
  EXPECT_DEATH(registry.RegisterOrDie<UDA1>("uda1"), ".*already exists.*");
}

const char* kExpectedUDFInfo = R"(
udas {
  name: "uda1"
  update_arg_types: INT64
  finalize_type: INT64
}
scalar_udfs {
  name: "add"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: FLOAT64
}
scalar_udfs {
  name: "scalar1"
  exec_arg_types: BOOLEAN
  exec_arg_types: INT64
  return_type: INT64
}
)";

TEST(RegistryInfoExporter, export_uda_and_udf) {
  auto uda_registry = UDARegistry("test registry");
  uda_registry.RegisterOrDie<UDA1>("uda1");

  auto scalar_udf_registry = ScalarUDFRegistry("test registry");
  scalar_udf_registry.RegisterOrDie<ScalarUDF1>("scalar1");
  scalar_udf_registry
      .RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Float64Value>>("add");

  auto udf_info =
      RegistryInfoExporter().Registry(uda_registry).Registry(scalar_udf_registry).ToProto();

  udfspb::UDFInfo expected_udf_info;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &expected_udf_info));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_udf_info, udf_info));
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
