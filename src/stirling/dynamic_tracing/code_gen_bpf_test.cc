#include <google/protobuf/text_format.h>

#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/code_gen.h"
#include "src/stirling/proto/physical_ir.pb.h"
#include "src/stirling/utils/linux_headers.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::stirling::bpf_tools::BCCWrapper;
using ::pl::stirling::bpf_tools::BPFProbeAttachType;
using ::pl::stirling::bpf_tools::UProbeSpec;
using ::pl::stirling::dynamictracingpb::Program;
using ::pl::stirling::utils::FindOrInstallLinuxHeaders;
using ::pl::stirling::utils::kDefaultHeaderSearchOrder;
using ::pl::testing::BazelBinTestFilePath;
using ::testing::EndsWith;
using ::testing::Field;
using ::testing::SizeIs;

constexpr char kProgram[] = R"proto(
                            structs {
                              name: "event_t"
                              fields {
                                name: "i32"
                                type { scalar: INT32 }
                              }
                            }
                            maps {
                              name: "events"
                              key_type { scalar: INT32 }
                              value_type { struct_type: "event_t" }
                            }
                            outputs {
                              name: "output"
                              type { struct_type: "event_t" }
                            }
                            probes {
                              name: "uprobe_canyoufindthis"
                              trace_point {
                                symbol: "CanYouFindThis"
                                type: ENTRY
                              }
                              vars {
                                name: "key"
                                type: UINT32
                                builtin: TGID
                              }
                              vars {
                                name: "var"
                                type: UINT32
                                reg: SP
                              }
                              st_vars {
                                name: "st_var"
                                type: "event_t"
                                variable_names { name: "var" }
                              }
                              map_stash_actions {
                                map_name: "events"
                                key_variable_name: "key"
                                value_variable_name: "st_var"
                              }
                              output_actions {
                                perf_buffer_name: "output"
                                variable_name: "st_var"
                              }
                            })proto";

TEST(CodeGenBPFTest, AttachOnDummyExe) {
  Program program;

  ASSERT_TRUE(TextFormat::ParseFromString(kProgram, &program));

  // Reset the binary path.
  program.mutable_probes(0)->mutable_trace_point()->set_binary_path(
      BazelBinTestFilePath("src/stirling/obj_tools/testdata/dummy_exe"));

  PL_LOG_VAR(program.DebugString());

  ASSERT_OK_AND_ASSIGN(BCCProgram bcc_program, GenProgram(program));
  ASSERT_THAT(bcc_program.uprobe_specs, SizeIs(1));

  const auto& spec = bcc_program.uprobe_specs[0];

  EXPECT_THAT(
      spec, Field(&UProbeSpec::binary_path, EndsWith("src/stirling/obj_tools/testdata/dummy_exe")));
  EXPECT_THAT(spec, Field(&UProbeSpec::symbol, "CanYouFindThis"));
  EXPECT_THAT(spec, Field(&UProbeSpec::attach_type, BPFProbeAttachType::kEntry));
  EXPECT_THAT(spec, Field(&UProbeSpec::probe_fn, "uprobe_canyoufindthis"));

  std::string bcc = absl::StrJoin(bcc_program.code_lines, "\n");

  PL_LOG_VAR(bcc);

  BCCWrapper bcc_wrapper;

  ASSERT_OK(FindOrInstallLinuxHeaders({kDefaultHeaderSearchOrder}));
  ASSERT_OK(bcc_wrapper.InitBPFProgram(bcc));
  ASSERT_OK(bcc_wrapper.AttachUProbe(spec));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
