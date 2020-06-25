#pragma once

#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/proto/physical_ir.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

// TODO(yzhao): Add systematic formatting option to organize nested code. One way is to have
// different nesting level, which translates into different indentation depth.

// Returns the definition of the input struct.
StatusOr<std::vector<std::string>> GenStruct(const ::pl::stirling::dynamictracingpb::Struct& st,
                                             int member_indent_size = 2);

// Returns the definition of the input ScalarVariable.
// TODO(yzhao): This probably need to handle indentation.
StatusOr<std::vector<std::string>> GenScalarVariable(
    const ::pl::stirling::dynamictracingpb::ScalarVariable& var);

// Returns the definition of the input StructVariable, with assignments of all fields.
StatusOr<std::vector<std::string>> GenStructVariable(
    const ::pl::stirling::dynamictracingpb::Struct& st,
    const ::pl::stirling::dynamictracingpb::StructVariable& var);

// Returns the code (in multiple lines) that perform the action to stash a key and variable pair
// into a BPF map.
std::vector<std::string> GenMapStashAction(
    const ::pl::stirling::dynamictracingpb::MapStashAction& action);

// Returns the code that submits variables to a perf buffer.
std::vector<std::string> GenOutputAction(
    const ::pl::stirling::dynamictracingpb::OutputAction& action);

// Returns the BCC probe function code.
StatusOr<std::vector<std::string>> GenPhysicalProbe(
    const ::pl::stirling::dynamictracingpb::PhysicalProbe& probe);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
