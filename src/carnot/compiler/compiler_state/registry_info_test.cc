#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <type_traits>

#include <absl/strings/match.h>
#include "src/carnot/compiler/compiler_state/registry_info.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace compiler {
using ::testing::UnorderedElementsAre;

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
udtfs {
  name: "OpenNetworkConnections"
  args {
    name: "upid"
    arg_type: STRING
    semantic_type: ST_UPID
  }
  executor: UDTF_SUBSET_PEM
  relation {
    columns {
      column_name: "time_"
      column_type: TIME64NS
    }
    columns {
      column_name: "fd"
      column_type: INT64
    }
    columns {
      column_name: "name"
      column_type: STRING
    }
  }
}
)";

TEST(RegistryInfo, basic) {
  auto info = RegistryInfo();
  udfspb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info.Init(info_pb));

  EXPECT_EQ(UDFType::kUDA, info.GetUDFType("uda1").ConsumeValueOrDie());
  EXPECT_EQ(UDFType::kUDF, info.GetUDFType("scalar1").ConsumeValueOrDie());
  EXPECT_NOT_OK(info.GetUDFType("dne"));

  EXPECT_EQ(types::INT64,
            info.GetUDA("uda1", std::vector<types::DataType>({types::INT64})).ConsumeValueOrDie());
  EXPECT_NOT_OK(info.GetUDA("uda2", std::vector<types::DataType>({types::INT64})));
  EXPECT_EQ(types::INT64,
            info.GetUDF("scalar1", std::vector<types::DataType>({types::BOOLEAN, types::INT64}))
                .ConsumeValueOrDie());
  EXPECT_FALSE(
      info.GetUDF("scalar1", std::vector<types::DataType>({types::BOOLEAN, types::FLOAT64})).ok());
  EXPECT_EQ(types::FLOAT64,
            info.GetUDF("add", std::vector<types::DataType>({types::FLOAT64, types::FLOAT64}))
                .ConsumeValueOrDie());

  EXPECT_THAT(info.func_names(), UnorderedElementsAre("uda1", "add", "scalar1"));

  ASSERT_EQ(info.udtfs().size(), 1);
  EXPECT_EQ(info.udtfs()[0].name(), "OpenNetworkConnections");
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
