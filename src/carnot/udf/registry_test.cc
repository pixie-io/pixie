#include <gtest/gtest.h>
#include <type_traits>

#include "absl/strings/match.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/utils/error.h"
#include "src/utils/macros.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace udf {

class ScalarUDF1 : ScalarUDF {
 public:
  Int64Value Exec(FunctionContext *ctx, BoolValue b1, Int64Value b2) {
    PL_UNUSED(ctx);
    return b1.val && b2.val ? 3 : 0;
  }
};

class ScalarUDF1WithInit : ScalarUDF {
 public:
  Status Init(FunctionContext *ctx, Int64Value v1) {
    PL_UNUSED(ctx);
    PL_UNUSED(v1);
    return Status::OK();
  }
  Int64Value Exec(FunctionContext *ctx, BoolValue b1, BoolValue b2) {
    PL_UNUSED(ctx);
    return b1.val && b2.val ? 3 : 0;
  }
};

template <typename TOutput, typename TInput1, typename TInput2>
class AddUDF : ScalarUDF {
 public:
  TOutput Exec(FunctionContext *ctx, TInput1 v1, TInput2 v2) {
    PL_UNUSED(ctx);
    return v1.val + v2.val;
  }
};

TEST(Registry, init_with_udfs) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<ScalarUDF1WithInit>("scalar1WithInit");

  auto statusor = registry.GetDefinition(
      "scalar1", std::vector<UDFDataType>({UDFDataType::BOOLEAN, UDFDataType::INT64}));
  ASSERT_TRUE(statusor.ok());
  auto def = statusor.ConsumeValueOrDie();
  ASSERT_NE(nullptr, def);
  EXPECT_EQ("scalar1", def->name());
  EXPECT_EQ(UDFDataType::INT64, def->exec_return_type());
  EXPECT_EQ(std::vector<UDFDataType>({UDFDataType::BOOLEAN, UDFDataType::INT64}),
            def->exec_arguments());

  const char *expected_debug_str =
      "Registry: test registry\n"
      "scalar1\n"
      "scalar1WithInit\n";
  EXPECT_EQ(expected_debug_str, registry.DebugString());
}

TEST(Registry, templated_udfs) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<AddUDF<Float64Value, Int64Value, Float64Value>>("add");
  registry.RegisterOrDie<AddUDF<Float64Value, Float64Value, Float64Value>>("add");

  auto statusor = registry.GetDefinition(
      "add", std::vector<UDFDataType>({UDFDataType::INT64, UDFDataType::FLOAT64}));
  ASSERT_TRUE(statusor.ok());
  EXPECT_NE(nullptr, statusor.ConsumeValueOrDie());

  statusor = registry.GetDefinition(
      "add", std::vector<UDFDataType>({UDFDataType::FLOAT64, UDFDataType::FLOAT64}));
  ASSERT_TRUE(statusor.ok());
  EXPECT_NE(nullptr, statusor.ConsumeValueOrDie());

  statusor = registry.GetDefinition(
      "add", std::vector<UDFDataType>({UDFDataType::INT64, UDFDataType::INT64}));
  ASSERT_FALSE(statusor.ok());
  EXPECT_TRUE(error::IsNotFound(statusor.status()));
}

TEST(RegistryTest, double_register) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<ScalarUDF1>("scalar1WithInit");
  auto status = registry.Register<ScalarUDF1>("scalar1");
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(error::IsAlreadyExists(status));
  EXPECT_TRUE(absl::StrContains(status.msg(), "scalar1"));
  EXPECT_TRUE(absl::StrContains(status.msg(), "already exists"));
}

TEST(RegistryTest, no_such_udf) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  auto statusor = registry.GetDefinition("scalar1", std::vector<UDFDataType>({UDFDataType::INT64}));
  EXPECT_FALSE(statusor.ok());
}

TEST(RegistryDeathTest, double_register) {
  auto registry = ScalarUDFRegistry("test registry");
  registry.RegisterOrDie<ScalarUDF1>("scalar1");
  registry.RegisterOrDie<ScalarUDF1>("scalar1WithInit");

  EXPECT_DEATH(registry.RegisterOrDie<ScalarUDF1>("scalar1"), ".*already exists.*");
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
