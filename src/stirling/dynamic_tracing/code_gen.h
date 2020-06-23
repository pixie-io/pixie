#pragma once

#include <string>

#include "src/common/base/base.h"
#include "src/stirling/proto/physical_ir.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

// TODO(yzhao): Add systematic formatting option to organize nested code. One way is to have
// different nesting level, which translates into different indentation depth.

// Returns the definition of the input struct.
StatusOr<std::string> GenStruct(const ::pl::stirling::dynamictracingpb::Struct& st,
                                int member_indent_size = 2);

// Returns the definition of the input Variable.
// TODO(yzhao): This probably need to handle indentation.
StatusOr<std::string> GenVariable(const ::pl::stirling::dynamictracingpb::Variable& var);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
