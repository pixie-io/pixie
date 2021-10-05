/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/physicalpb/physical.pb.h"

namespace px {
namespace stirling {
namespace dynamic_tracing {

/**
 * Uses ELF or DWARF information to detect the source language.
 * Populates the tracepoint program's language field in input_program.
 */
void DetectSourceLanguage(obj_tools::ElfReader* elf_reader, obj_tools::DwarfReader* dwarf_reader,
                          ir::logical::TracepointDeployment* input_program);

/**
 * Uses ELF information to check if the provided symbol exists.
 * If it does not exist, it checks whether it is a short-hand (suffix) of a full symbol.
 * If it is a short-hand reference to a symbol, the symbol is replaced with the full-form.
 * Potentially modifies each tracepoint's symbol field in input_program.
 */
Status ResolveProbeSymbol(obj_tools::ElfReader* elf_reader,
                          ir::logical::TracepointDeployment* input_program);

/**
 * If any tracepoint in input_program contains no fields to trace, this function uses DWARF info
 * to automatically add (1) all arguments, (2) all response values, and (3) function latency
 * to the tracepoint specifications in input_program.
 */
Status AutoTraceExpansion(obj_tools::DwarfReader* dwarf_reader,
                          ir::logical::TracepointDeployment* input_program);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
