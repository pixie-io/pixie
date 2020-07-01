#include <string>

#include <google/protobuf/util/message_differencer.h>

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/dwarf_info.h"

// The binary location cannot be hard-coded because its location depends on -c opt/dbg/fastbuild.
DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;

constexpr std::string_view kEntryProbeIRTmpl = R"(
  trace_point: {
    binary_path: "$0"
    symbol: "main.MixedArgTypes"
    type: ENTRY
  }
  args {
    id: "arg0"
    expr: "i1"
  }
  args {
    id: "arg1"
    expr: "i2"
  }
  args {
    id: "arg2"
    expr: "i3"
  }
  args {
    id: "arg3"
    expr: "b1"
  }
  args {
    id: "arg4"
    expr: "b2.B0"
  }
  args {
    id: "arg5"
    expr: "b2.B3"
  }
)";

constexpr std::string_view kEntryProbePhysIRTmpl = R"(
  trace_point: {
    binary_path: "$0"
    symbol: "main.MixedArgTypes"
    type: ENTRY
  }
  vars {
    name: "sp"
    type: VOID_POINTER
    reg: SP
  }
  vars {
    name: "arg0"
    type: INT
    memory: {
      base: "sp"
      offset: 8
    }
  }
  vars {
    name: "arg1"
    type: INT
    memory: {
      base: "sp"
      offset: 24
    }
  }
  vars {
    name: "arg2"
    type: INT
    memory: {
      base: "sp"
      offset: 32
    }
  }
  vars {
    name: "arg3"
    type: BOOL
    memory: {
      base: "sp"
      offset: 16
    }
  }
  vars {
    name: "arg4_D_B0"
    type: BOOL
    memory: {
      base: "sp"
      offset: 17
    }
  }
  vars {
    name: "arg5_D_B3"
    type: BOOL
    memory: {
      base: "sp"
      offset: 20
    }
  }
)";

constexpr std::string_view kReturnProbeIRTmpl = R"(
  trace_point: {
    binary_path: "$0"
    symbol: "main.MixedArgTypes"
    type: RETURN
  }
  ret_vals {
    id: "retval0"
    index: 6
  }
  ret_vals {
    id: "retval1"
    index: 7
  }
)";

constexpr std::string_view kReturnProbePhysIRTmpl = R"(
  trace_point: {
    binary_path: "$0"
    symbol: "main.MixedArgTypes"
    type: RETURN
  }
  vars {
    name: "sp"
    type: VOID_POINTER
    reg: SP
  }
  vars {
    name: "retval0"
    type: INT
    memory: {
      base: "sp"
      offset: 48
    }
  }
vars {
    name: "retval1"
    type: BOOL
    memory: {
      base: "sp"
      offset: 56
    }
  }
)";

constexpr std::string_view kNestedArgEntryProbeIRTmpl = R"(
  trace_point: {
    binary_path: "$0"
    symbol: "main.PointerWrapperWrapperWrapperFunc"
    type: ENTRY
  }
  args {
    id: "arg0"
    expr: "p.Ptr.Val.Ptr"
  }
  args {
    id: "arg1"
    expr: "p.Ptr.Val.V0"
  }
)";

constexpr std::string_view kNestedArgEntryProbePhysIRTmpl = R"(
  trace_point: {
    binary_path: "$0"
    symbol: "main.PointerWrapperWrapperWrapperFunc"
    type: ENTRY
  }
  vars {
    name: "sp"
    type: VOID_POINTER
    reg: SP
  }
  vars {
    name: "arg0_D_Ptr_X_"
    type: VOID_POINTER
    memory: {
      base: "sp"
      offset: 16
    }
  }
  vars {
    name: "arg0_D_Ptr_X__D_Val_D_Ptr_X_"
    type: VOID_POINTER
    memory: {
      base: "arg0_D_Ptr_X_"
      offset: 40
    }
  }
  vars {
    name: "arg0_D_Ptr_X__D_Val_D_Ptr_X__X_"
    type: INT
    memory: {
      base: "arg0_D_Ptr_X__D_Val_D_Ptr_X_"
      offset: 0
    }
  }
  vars {
    name: "arg1_D_Ptr_X_"
    type: VOID_POINTER
    memory: {
      base: "sp"
      offset: 16
    }
  }
  vars {
    name: "arg1_D_Ptr_X__D_Val_D_V0"
    type: INT64
    memory: {
      base: "arg1_D_Ptr_X_"
      offset: 16
    }
  }
)";

struct DwarfInfoTestParam {
  std::string_view input;
  std::string_view expected_output;
};

class DwarfInfoTest : public ::testing::TestWithParam<DwarfInfoTestParam> {
 protected:
  DwarfInfoTest() : kGoBinaryPath(pl::testing::TestFilePath(FLAGS_dummy_go_binary)) {}

  std::string kGoBinaryPath;
};

TEST_P(DwarfInfoTest, Transform) {
  DwarfInfoTestParam p = GetParam();

  std::string entry_probe_ir = absl::Substitute(p.input, kGoBinaryPath);
  ir::logical::Probe input_probe;
  ASSERT_TRUE(TextFormat::ParseFromString(std::string(entry_probe_ir), &input_probe));

  std::string entry_probe_phys_ir = absl::Substitute(p.expected_output, kGoBinaryPath);
  ir::physical::PhysicalProbe expected_output_probe;
  ASSERT_TRUE(
      TextFormat::ParseFromString(std::string(entry_probe_phys_ir), &expected_output_probe));

  ASSERT_OK_AND_ASSIGN(ir::physical::PhysicalProbe output_probe, AddDwarves(input_probe));

  MessageDifferencer message_differencer;
  std::string diff_out;
  message_differencer.ReportDifferencesToString(&diff_out);
  EXPECT_TRUE(message_differencer.Compare(output_probe, expected_output_probe)) << diff_out;
}

INSTANTIATE_TEST_SUITE_P(
    DwarfInfoTestSuite, DwarfInfoTest,
    ::testing::Values(DwarfInfoTestParam{kEntryProbeIRTmpl, kEntryProbePhysIRTmpl},
                      DwarfInfoTestParam{kReturnProbeIRTmpl, kReturnProbePhysIRTmpl},
                      DwarfInfoTestParam{kNestedArgEntryProbeIRTmpl,
                                         kNestedArgEntryProbePhysIRTmpl}));

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
