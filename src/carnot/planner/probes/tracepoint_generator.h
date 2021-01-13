#pragma once

#include "src/carnot/planner/compiler/ast_visitor.h"

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * Take a tracepoint specification in PXL format, and compiles it to a logical tracepoint Program.
 */
StatusOr<stirling::dynamic_tracing::ir::logical::TracepointDeployment> CompileTracepoint(
    std::string_view query);

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
