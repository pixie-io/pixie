#pragma once

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/code_gen.h"
#include "src/stirling/dynamic_tracing/dwarf_info.h"
#include "src/stirling/dynamic_tracing/probe_transformer.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

/**
 * Transforms any logical probes inside a program into entry and return probes.
 * Also automatically adds any required supporting maps and implicit outputs.
 */
StatusOr<BCCProgram> CompileProgram(const ir::logical::Program& input_program);

/**
 * @brief Initialize BCC code and attach all uprobes defined in the input BCC program.
 */
Status DeployBCCProgram(const BCCProgram& bcc_program, bpf_tools::BCCWrapper* bcc_wrapper);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
