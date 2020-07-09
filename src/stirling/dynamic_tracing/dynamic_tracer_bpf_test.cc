#include <absl/strings/substitute.h>
#include <google/protobuf/text_format.h>

#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/dynamic_tracer.h"
#include "src/stirling/utils/linux_headers.h"

DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::stirling::bpf_tools::BCCWrapper;
using ::pl::stirling::bpf_tools::BPFProbeAttachType;
using ::pl::stirling::bpf_tools::UProbeSpec;
using ::pl::stirling::utils::FindOrInstallLinuxHeaders;
using ::pl::stirling::utils::kDefaultHeaderSearchOrder;
using ::testing::EndsWith;
using ::testing::Field;

constexpr char kLogicalProgram[] = R"(
probes: {
  name: "dummy_probe_CrossScale"
  trace_point: {
    binary_path: "$0"
    symbol: "main.(*Vertex).CrossScale"
    type: LOGICAL
  }
  args {
    id: "f"
    expr: "f"
  }
}
)";

TEST(CodeGenBPFTest, AttachDummyProgram) {
  ir::logical::Program logical_program;

  std::string logical_program_str =
      absl::Substitute(kLogicalProgram, pl::testing::TestFilePath(FLAGS_dummy_go_binary).string());

  ASSERT_TRUE(TextFormat::ParseFromString(logical_program_str, &logical_program));

  // TODO(yzhao): CompileProgram() fails today, because:
  // * Intermediate program has a output actions in the return probe that reference values stashed
  //   in the ENTRY probe. But the map_unstash_actions added yet.
  //
  // The protobuf looks like:
  // probes {
  //   name: "dummy_probe_CrossScale_return"
  //   trace_point {
  //     binary_path: "src/stirling/obj_tools/testdata/linux_amd64/dummy_go_binary"
  //     symbol: "main.(*Vertex).CrossScale"
  //     type: RETURN
  //   }
  //   output_actions {
  //     output_name: "dummy_probe_CrossScale_table"
  //     variable_name: "f"
  //   }
  // }
  ASSERT_OK(CompileProgram(logical_program));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
