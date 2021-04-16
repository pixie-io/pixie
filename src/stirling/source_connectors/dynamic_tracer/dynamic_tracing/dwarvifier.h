#pragma once

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/dwarf_tools.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/physicalpb/physical.pb.h"

DECLARE_bool(enable_tracing_golang_interface);

namespace px {
namespace stirling {
namespace dynamic_tracing {

StatusOr<ir::physical::Program> GeneratePhysicalProgram(
    const ir::logical::TracepointDeployment& input_program,
    ::px::stirling::obj_tools::DwarfReader* dwarf_reader,
    ::px::stirling::obj_tools::ElfReader* elf_reader);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
