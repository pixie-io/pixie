#pragma once

#include <string>

#include "src/common/base/base.h"
#include "src/stirling/proto/ir.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

// Returns the definition of the input struct in BCC code.
StatusOr<std::string> GenStruct(const ::pl::stirling::dynamictracingpb::Struct& st,
                                int indent_size = 2);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
