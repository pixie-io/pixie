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

#pragma once

#include <string>

namespace px {
namespace stirling {
namespace dynamic_tracing {

namespace test_data {

constexpr std::string_view kProgramSpec = R"(
binary_spec {
  path: "$0"
  language: GOLANG
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
}
probes: {
  name: "probe0"
  tracepoint: {
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
    index: 6
  }
  ret_vals {
    id: "retval1"
    index: 7
  }
}
)";

}  // namespace test_data

namespace test_utils {

template <typename TMessageType>
StatusOr<TMessageType> Prepare(std::string_view program_template, const std::string& binary) {
  TMessageType trace_program;
  std::string program_str = absl::Substitute(program_template, binary);
  bool success = google::protobuf::TextFormat::ParseFromString(program_str, &trace_program);
  if (!success) {
    return error::Internal("Failed to parse protobuf");
  }
  return trace_program;
}

}  // namespace test_utils
}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
