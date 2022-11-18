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

#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/probe_transformer.h"

constexpr std::string_view kBinaryPath = "src/stirling/obj_tools/testdata/go/test_go_1_16_binary";

namespace px {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::px::testing::proto::EqualsProto;

constexpr std::string_view kLogicalProgram = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: GOLANG
    outputs {
      name: "probe0_table"
      fields: "arg0"
      fields: "arg1"
      fields: "arg2"
      fields: "arg3"
      fields: "arg4"
      fields: "arg5"
      fields: "retval0"
      fields: "retval1"
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
      ret_vals {
        id: "retval0"
        expr: "$$0"
      }
      ret_vals {
        id: "retval1"
        expr: "$$1"
      }
      function_latency { id: "fn_latency" }
      output_actions {
        output_name: "probe0_table"
        variable_names: "arg0"
        variable_names: "arg1"
        variable_names: "arg2"
        variable_names: "arg3"
        variable_names: "arg4"
        variable_names: "arg5"
        variable_names: "retval0"
        variable_names: "retval1"
        variable_names: "fn_latency"
      }
    }
  }
}
)";

constexpr std::string_view kTransformedProgram = R"proto(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: GOLANG
    maps {
      name: "pid_goid_map"
    }
    maps {
      name: "probe0_argstash"
    }
    outputs {
      name: "probe0_table"
      fields: "arg0"
      fields: "arg1"
      fields: "arg2"
      fields: "arg3"
      fields: "arg4"
      fields: "arg5"
      fields: "retval0"
      fields: "retval1"
      fields: "latency"
    }
    probes {
      name: "probe_entry_runtime_casgstatus"
      tracepoint {
        symbol: "runtime.casgstatus"
        type: ENTRY
      }
      consts {
        name: "kGRunningState"
        type: INT64
        constant: "2"
      }
      args {
        id: "goid"
        expr: "gp.goid"
      }
      args {
        id: "newval"
        expr: "newval"
      }
      map_stash_actions {
        map_name: "pid_goid_map"
        key: TGID_PID
        value_variable_names: "goid"
        cond {
          op: EQUAL
          vars: "newval"
          vars: "kGRunningState"
        }
      }
    }
    probes {
      name: "probe0_entry"
      tracepoint {
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
      map_stash_actions {
        map_name: "probe0_argstash"
        key: GOID
        value_variable_names: "arg0"
        value_variable_names: "arg1"
        value_variable_names: "arg2"
        value_variable_names: "arg3"
        value_variable_names: "arg4"
        value_variable_names: "arg5"
        value_variable_names: "time_"
      }
    }
    probes {
      name: "probe0_return"
      tracepoint {
        symbol: "main.MixedArgTypes"
        type: RETURN
      }
      function_latency { id: "fn_latency" }
      map_vals {
        map_name: "probe0_argstash"
        key: GOID
        value_ids: "arg0"
        value_ids: "arg1"
        value_ids: "arg2"
        value_ids: "arg3"
        value_ids: "arg4"
        value_ids: "arg5"
        value_ids: "start_ktime_ns"
      }
      ret_vals {
        id: "retval0"
        expr: "$$0"
      }
      ret_vals {
        id: "retval1"
        expr: "$$1"
      }
      output_actions {
        output_name: "probe0_table"
        variable_names: "arg0"
        variable_names: "arg1"
        variable_names: "arg2"
        variable_names: "arg3"
        variable_names: "arg4"
        variable_names: "arg5"
        variable_names: "retval0"
        variable_names: "retval1"
        variable_names: "fn_latency"
      }
      map_delete_actions {
        map_name: "probe0_argstash"
        key: GOID
      }
    }
  }
}
)proto";

struct ProbeGenTestParam {
  std::string_view input;
  std::string_view expected_output;
};

class ProbeGenTest : public ::testing::TestWithParam<ProbeGenTestParam> {
 protected:
  ProbeGenTest() : binary_path_(px::testing::BazelRunfilePath(kBinaryPath)) {}

  std::string binary_path_;
};

TEST_P(ProbeGenTest, Transform) {
  ProbeGenTestParam p = GetParam();

  std::string input_program_str = absl::Substitute(p.input, binary_path_);
  ir::logical::TracepointDeployment input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(input_program_str, &input_program));

  std::string expected_output = absl::Substitute(p.expected_output, binary_path_);

  ASSERT_OK_AND_THAT(TransformLogicalProgram(input_program), EqualsProto(expected_output));
}

INSTANTIATE_TEST_SUITE_P(ProbeGenTestSuite, ProbeGenTest,
                         ::testing::Values(ProbeGenTestParam{kLogicalProgram,
                                                             kTransformedProgram}));

TEST_F(ProbeGenTest, ErrorCases) {
  std::string input_program_str = absl::Substitute(kLogicalProgram, binary_path_);
  ir::logical::TracepointDeployment input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(input_program_str, &input_program));

  {
    // Output must be specified if an OutputAction exists.
    ir::logical::TracepointDeployment p = input_program;
    p.mutable_tracepoints(0)->mutable_program()->mutable_outputs()->Clear();
    ASSERT_NOT_OK(TransformLogicalProgram(p));
  }

  // TODO(oazizi): Add more.
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
