#pragma once

#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/ir/logicalpb/logical.pb.h"
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
StatusOr<std::vector<std::string>> GenScalarVariable(const ir::physical::ScalarVariable& var);

// Returns the definition of the input StructVariable, with assignments of all fields.
StatusOr<std::vector<std::string>> GenStructVariable(const ir::physical::StructVariable& var);

// Returns the code (in multiple lines) that perform the action to stash a key and variable pair
// into a BPF map.
StatusOr<std::vector<std::string>> GenMapStashAction(const ir::physical::MapStashAction& action);

// Returns the BCC code for the input physical IR.
StatusOr<std::string> GenBCCProgram(const ir::physical::Program& program);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
