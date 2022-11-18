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

#include <memory>
#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/autogen.h"

constexpr std::string_view kBinaryPath = "src/stirling/obj_tools/testdata/go/test_go_1_16_binary";

namespace px {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::px::testing::proto::EqualsProto;
using ::testing::HasSubstr;

using ::px::stirling::obj_tools::DwarfReader;
using ::px::stirling::obj_tools::ElfReader;

constexpr std::string_view kInputProgram = R"(
deployment_spec {
  path_list: {
    paths: "$0"
  }
}
tracepoints {
  program {
    probes {
      name: "probe0"
      tracepoint {
        symbol: "MixedArgTypes"
        type: LOGICAL
      }
    }
  }
}
)";

constexpr std::string_view kProgramWithLanguage = R"(
deployment_spec {
  path_list: {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: GOLANG
    probes {
      name: "probe0"
      tracepoint {
        symbol: "MixedArgTypes"
        type: LOGICAL
      }
    }
  }
}
)";

constexpr std::string_view kProgramWithSymbol = R"(
deployment_spec {
  path_list: {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: GOLANG
    probes {
      name: "probe0"
      tracepoint {
        symbol: "main.MixedArgTypes"
        type: LOGICAL
      }
    }
  }
}
)";

constexpr std::string_view kAutoTraceExpansionOutput = R"(
deployment_spec {
  path_list: {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: GOLANG
    outputs {
      name: "main__d__MixedArgTypes_table"
      fields: "b1"
      fields: "b2"
      fields: "b3"
      fields: "i1"
      fields: "i2"
      fields: "i3"
      fields: "__tilde__r6"
      fields: "__tilde__r7"
      fields: "latency"
    }
    probes {
      name: "probe0"
      tracepoint {
        symbol: "main.MixedArgTypes"
        type: LOGICAL
      }
      args {
        id: "arg0"
        expr: "b1"
      }
      args {
        id: "arg1"
        expr: "b2"
      }
      args {
        id: "arg2"
        expr: "b3"
      }
      args {
        id: "arg3"
        expr: "i1"
      }
      args {
        id: "arg4"
        expr: "i2"
      }
      args {
        id: "arg5"
        expr: "i3"
      }
      ret_vals {
        id: "retval6"
        expr: "~r6"
      }
      ret_vals {
        id: "retval7"
        expr: "~r7"
      }
      function_latency { id: "fn_latency" }
      output_actions {
        output_name: "main__d__MixedArgTypes_table"
        variable_names: "arg0"
        variable_names: "arg1"
        variable_names: "arg2"
        variable_names: "arg3"
        variable_names: "arg4"
        variable_names: "arg5"
        variable_names: "retval6"
        variable_names: "retval7"
        variable_names: "fn_latency"
      }
    }
  }
}
)";

struct ProbeGenTestParam {
  std::string_view input;
  std::string_view expected_output;
};

class ProbeGenTest : public ::testing::TestWithParam<ProbeGenTestParam> {
 protected:
  ProbeGenTest() : binary_path_(px::testing::BazelRunfilePath(kBinaryPath)) {}

  void SetUp() {
    ASSERT_OK_AND_ASSIGN(dwarf_reader_, DwarfReader::CreateIndexingAll(binary_path_));
    ASSERT_OK_AND_ASSIGN(elf_reader_, ElfReader::Create(binary_path_));
  }

  void PrepareInput(std::string_view input_str,
                    ir::logical::TracepointDeployment* prepared_program) {
    std::string input_program_str = absl::Substitute(input_str, binary_path_);
    ASSERT_TRUE(TextFormat::ParseFromString(input_program_str, prepared_program));
  }

  std::string binary_path_;
  std::unique_ptr<DwarfReader> dwarf_reader_;
  std::unique_ptr<ElfReader> elf_reader_;
};

//-------------------------------------
// DetectSourceLanguage Tests
//-------------------------------------

class DetectSourceLanguageTest : public ProbeGenTest {};

TEST_P(DetectSourceLanguageTest, Transform) {
  ProbeGenTestParam p = GetParam();

  ir::logical::TracepointDeployment program;
  ASSERT_NO_FATAL_FAILURE(PrepareInput(p.input, &program));

  std::string expected_output = absl::Substitute(p.expected_output, binary_path_);

  DetectSourceLanguage(elf_reader_.get(), dwarf_reader_.get(), &program);
  ASSERT_THAT(program, EqualsProto(expected_output));
}

INSTANTIATE_TEST_SUITE_P(DetectSourceLanguageTestSuite, DetectSourceLanguageTest,
                         ::testing::Values(ProbeGenTestParam{kInputProgram, kProgramWithLanguage}));

//-------------------------------------
// ResolveProbeSymbol Tests
//-------------------------------------

class ResolveProbeSymbolTest : public ProbeGenTest {};

TEST_P(ResolveProbeSymbolTest, Transform) {
  ProbeGenTestParam p = GetParam();

  ir::logical::TracepointDeployment program;
  ASSERT_NO_FATAL_FAILURE(PrepareInput(p.input, &program));

  std::string expected_output = absl::Substitute(p.expected_output, binary_path_);

  ASSERT_OK(ResolveProbeSymbol(elf_reader_.get(), &program));

  ASSERT_THAT(program, EqualsProto(expected_output));
}

INSTANTIATE_TEST_SUITE_P(ResolveProbeSymbolTestSuite, ResolveProbeSymbolTest,
                         ::testing::Values(ProbeGenTestParam{kProgramWithLanguage,
                                                             kProgramWithSymbol}));

TEST_F(ResolveProbeSymbolTest, IncompleteSymbol) {
  constexpr std::string_view kInputProgramWithIncompleteSymbol = R"(
deployment_spec {
  path_list: {
    paths: "$0"
  }
}
tracepoints {
  program {
    probes {
      name: "probe0"
      tracepoint {
        symbol: "ixedArgTypes"
        type: LOGICAL
      }
    }
  }
}
)";

  ir::logical::TracepointDeployment program;
  ASSERT_NO_FATAL_FAILURE(PrepareInput(kInputProgramWithIncompleteSymbol, &program));

  ASSERT_NOT_OK(ResolveProbeSymbol(elf_reader_.get(), &program));
}

TEST_F(ResolveProbeSymbolTest, AmbiguousSymbol) {
  constexpr std::string_view kInputProgramWithAmbiguousSymbol = R"(
deployment_spec {
  path_list: {
    paths: "$0"
  }
}
tracepoints {
  program {
    probes {
      name: "probe0"
      tracepoint {
        symbol: "Scale"
        type: LOGICAL
      }
    }
  }
}
)";

  ir::logical::TracepointDeployment program;
  ASSERT_NO_FATAL_FAILURE(PrepareInput(kInputProgramWithAmbiguousSymbol, &program));

  Status result = ResolveProbeSymbol(elf_reader_.get(), &program);
  ASSERT_NOT_OK(result);
  ASSERT_THAT(result.ToString(),
              HasSubstr("Symbol is ambiguous. Found at least 2 possible matches"));
}

//-------------------------------------
// AutoTraceExpansion Tests
//-------------------------------------

class AutoTraceExpansionTest : public ProbeGenTest {};

TEST_P(AutoTraceExpansionTest, Transform) {
  ProbeGenTestParam p = GetParam();

  ir::logical::TracepointDeployment program;
  ASSERT_NO_FATAL_FAILURE(PrepareInput(p.input, &program));

  std::string expected_output = absl::Substitute(p.expected_output, binary_path_);

  ASSERT_OK(AutoTraceExpansion(dwarf_reader_.get(), &program));

  ASSERT_THAT(program, EqualsProto(expected_output));
}

INSTANTIATE_TEST_SUITE_P(AutoTraceExpansionTestSuite, AutoTraceExpansionTest,
                         ::testing::Values(ProbeGenTestParam{kProgramWithSymbol,
                                                             kAutoTraceExpansionOutput}));

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
