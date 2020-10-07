#pragma once

#include "src/common/base/base.h"
#include "src/stirling/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/dynamic_tracing/ir/physicalpb/physical.pb.h"
#include "src/stirling/obj_tools/dwarf_tools.h"
#include "src/stirling/obj_tools/elf_tools.h"

DECLARE_bool(enable_tracing_golang_interface);

namespace pl {
namespace stirling {
namespace dynamic_tracing {

StatusOr<ir::physical::Program> GeneratePhysicalProgram(
    const ir::logical::TracepointDeployment& input_program,
    ::pl::stirling::dwarf_tools::DwarfReader* dwarf_reader,
    ::pl::stirling::elf_tools::ElfReader* elf_reader);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
