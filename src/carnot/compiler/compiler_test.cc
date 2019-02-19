#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <pypa/parser/parser.hh>
#include <unordered_map>
#include <vector>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"

namespace pl {
namespace carnot {
namespace compiler {

using testing::_;

const char *kExpectedUDFInfo = R"(
scalar_udfs {
  name: "divide"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: INT64
}
)";

TEST(CompilerTest, basic) {
  auto info = std::make_shared<RegistryInfo>();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info->Init(info_pb));

  auto lookup_map =
      std::unordered_map<std::string, std::unordered_map<std::string, types::DataType>>();
  auto cpu_lookup = std::unordered_map<std::string, types::DataType>();
  cpu_lookup.emplace("cpu0", types::DataType::FLOAT64);
  cpu_lookup.emplace("cpu1", types::DataType::FLOAT64);
  lookup_map.emplace("cpu", cpu_lookup);

  auto compiler_state = std::make_unique<CompilerState>(lookup_map, info.get());

  auto compiler = Compiler();
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          ".Map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1}).Result()",
      },
      "\n");
  EXPECT_OK(compiler.Compile(query, compiler_state.get()));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
