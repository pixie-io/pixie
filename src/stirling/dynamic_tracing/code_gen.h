#pragma once

#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/ir/physicalpb/physical.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

// TODO(yzhao): Add systematic formatting option to organize nested code. One way is to have
// different nesting level, which translates into different indentation depth.

// Returns the definition of the input struct.
StatusOr<std::vector<std::string>> GenStruct(const ir::physical::Struct& st,
                                             int member_indent_size = 2);

// Returns the definition of the input ScalarVariable.
// TODO(yzhao): This probably need to handle indentation.
StatusOr<std::vector<std::string>> GenScalarVariable(
    const ir::physical::ScalarVariable& var, const ir::shared::BinarySpec::Language& language);

// Returns the definition of the input StructVariable, with assignments of all fields.
StatusOr<std::vector<std::string>> GenStructVariable(const ir::physical::Struct& st,
                                                     const ir::physical::StructVariable& var);

// Returns the code (in multiple lines) that perform the action to stash a key and variable pair
// into a BPF map.
StatusOr<std::vector<std::string>> GenMapStashAction(const ir::physical::MapStashAction& action);

// Returns the code that submits variables to a perf buffer.
std::string GenOutputAction(const ir::physical::OutputAction& action);

// TODO(yzhao): Considers move this out of this header and into
// src/stirling/dynamic_trace_connector.h, because most of this part is not generating BCC code.
StatusOr<std::string> GenProgram(const ir::physical::Program& program);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
