#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <type_traits>

#include "absl/strings/match.h"
#include "src/carnot/compiler/registry_info.h"
#include "src/common/error.h"
#include "src/common/macros.h"
#include "src/common/status.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace compiler {

const char *kExpectedUDFInfo = R"(
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

TEST(RegistryInfo, basic) {
  auto info = RegistryInfo();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info.Init(info_pb));
  EXPECT_EQ(types::INT64,
            info.GetUDA("uda1", std::vector<types::DataType>({types::INT64})).ConsumeValueOrDie());
  EXPECT_FALSE(info.GetUDA("uda2", std::vector<types::DataType>({types::INT64})).ok());
  EXPECT_EQ(types::INT64,
            info.GetUDF("scalar1", std::vector<types::DataType>({types::BOOLEAN, types::INT64}))
                .ConsumeValueOrDie());
  EXPECT_FALSE(
      info.GetUDF("scalar1", std::vector<types::DataType>({types::BOOLEAN, types::FLOAT64})).ok());
  EXPECT_EQ(types::FLOAT64,
            info.GetUDF("add", std::vector<types::DataType>({types::FLOAT64, types::FLOAT64}))
                .ConsumeValueOrDie());
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
