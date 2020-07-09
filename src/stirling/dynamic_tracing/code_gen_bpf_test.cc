#include <google/protobuf/text_format.h>

#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/code_gen.h"
#include "src/stirling/dynamic_tracing/dwarf_info.h"
#include "src/stirling/dynamic_tracing/goid.h"
#include "src/stirling/dynamic_tracing/ir/physical.pb.h"
#include "src/stirling/utils/linux_headers.h"

DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::stirling::bpf_tools::BCCWrapper;
using ::pl::stirling::bpf_tools::BPFProbeAttachType;
using ::pl::stirling::bpf_tools::UProbeSpec;
using ::pl::stirling::dynamic_tracing::ir::physical::Program;
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
                                name: "time_ns",
                                type { scalar: UINT64 }
                              }
                              fields {
                                name: "pid_start_time_ns",
                                type { scalar: UINT64 }
                              }
                              fields {
                                name: "i32"
                                type { scalar: INT32 }
                              }
                            }
                            structs {
                              name: "pid_goid_map_value_t"
                              fields {
                                name: "goid_"
                                type {
                                  scalar: INT64
                                }
                              }
                            }
                            maps {
                              name: "events"
                              key_type { scalar: INT32 }
                              value_type { struct_type: "event_t" }
                            }
                            maps {
                              name: "pid_goid_map"
                              key_type { scalar: UINT64 }
                              value_type { struct_type: "pid_goid_map_value_t"  }
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
                              vars {
                                name: "time_ns"
                                type: UINT64
                                builtin: KTIME
                              }
                              vars {
                                name: "pid_start_time_ns"
                                type: UINT64
                                builtin: TGID_START_TIME
                              }
                              st_vars {
                                name: "st_var"
                                type: "event_t"
                                field_assignments {
                                  field_name: "time_ns"
                                  variable_name: "time_ns"
                                }
                                field_assignments {
                                  field_name: "pid_start_time_ns"
                                  variable_name: "pid_start_time_ns"
                                }
                                field_assignments {
                                  field_name: "i32"
                                  variable_name: "var"
                                }
                              }
                              map_vars {
                                name: "events_map_var"
                                type: "event_t"
                                map_name: "events"
                                key_variable_name: "key"
                              }
                              member_vars {
                                name: "event_i32"
                                type: INT32
                                struct_base: "events_map_var"
                                is_struct_base_pointer: true
                                field: "i32"
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
                              printks { text: "hello world!" }
                              printks { scalar: "var" }
                            }
)proto";

TEST(CodeGenBPFTest, AttachOnDummyExe) {
  Program program;

  ASSERT_TRUE(TextFormat::ParseFromString(kProgram, &program));

  const std::filesystem::path dummy_exe_path =
      BazelBinTestFilePath("src/stirling/obj_tools/testdata/dummy_exe");

  // Reset the binary path.
  program.mutable_probes(0)->mutable_trace_point()->set_binary_path(dummy_exe_path);

  ASSERT_OK_AND_ASSIGN(BCCProgram bcc_program, GenProgram(program));
  ASSERT_THAT(bcc_program.uprobe_specs, SizeIs(1));

  const auto& spec = bcc_program.uprobe_specs[0];

  EXPECT_THAT(
      spec, Field(&UProbeSpec::binary_path, EndsWith("src/stirling/obj_tools/testdata/dummy_exe")));
  EXPECT_THAT(spec, Field(&UProbeSpec::symbol, "CanYouFindThis"));
  EXPECT_THAT(spec, Field(&UProbeSpec::attach_type, BPFProbeAttachType::kEntry));
  EXPECT_THAT(spec, Field(&UProbeSpec::probe_fn, "uprobe_canyoufindthis"));

  PL_LOG_VAR(bcc_program.code);

  BCCWrapper bcc_wrapper;

  ASSERT_OK(FindOrInstallLinuxHeaders({kDefaultHeaderSearchOrder}));
  ASSERT_OK(bcc_wrapper.InitBPFProgram(bcc_program.code));
  ASSERT_OK(bcc_wrapper.AttachUProbe(spec));
}

TEST(CodeGenBPFTest, AttachGOIDProbe) {
  ir::logical::Probe goid_probe = GenGOIDProbe();

  goid_probe.mutable_trace_point()->set_binary_path(
      pl::testing::TestFilePath(FLAGS_dummy_go_binary));

  ir::logical::Program intermediate_program;
  intermediate_program.add_maps()->CopyFrom(GenGOIDMap());
  intermediate_program.add_probes()->CopyFrom(std::move(goid_probe));

  ASSERT_OK_AND_ASSIGN(ir::physical::Program program, AddDwarves(intermediate_program));

  ASSERT_OK_AND_ASSIGN(BCCProgram bcc_program, GenProgram(program));
  ASSERT_THAT(bcc_program.uprobe_specs, SizeIs(1));

  const auto& spec = bcc_program.uprobe_specs[0];

  EXPECT_THAT(spec, Field(&UProbeSpec::binary_path, EndsWith(FLAGS_dummy_go_binary)));
  EXPECT_THAT(spec, Field(&UProbeSpec::symbol, "runtime.casgstatus"));
  EXPECT_THAT(spec, Field(&UProbeSpec::attach_type, BPFProbeAttachType::kEntry));
  EXPECT_THAT(spec, Field(&UProbeSpec::probe_fn, "probe_entry_runtime_casgstatus"));

  BCCWrapper bcc_wrapper;

  PL_LOG_VAR(bcc_program.code);

  // TODO(yzhao): Move this and any other common code into a test fixture.
  ASSERT_OK(FindOrInstallLinuxHeaders({kDefaultHeaderSearchOrder}));
  ASSERT_OK(bcc_wrapper.InitBPFProgram(bcc_program.code));
  ASSERT_OK(bcc_wrapper.AttachUProbe(spec));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
