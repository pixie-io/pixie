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
