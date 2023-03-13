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

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/autogen.h"

#include <absl/strings/str_replace.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/obj_tools/go_syms.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/sharedpb/shared.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/types.h"

namespace px {
namespace stirling {
namespace dynamic_tracing {

namespace {

StatusOr<ir::shared::Language> TransformSourceLanguage(
    const llvm::dwarf::SourceLanguage& source_language) {
  switch (source_language) {
    case llvm::dwarf::DW_LANG_Go:
      return ir::shared::Language::GOLANG;
    case llvm::dwarf::DW_LANG_C:
    case llvm::dwarf::DW_LANG_C99:
    case llvm::dwarf::DW_LANG_C_plus_plus:
    case llvm::dwarf::DW_LANG_C_plus_plus_03:
    case llvm::dwarf::DW_LANG_C_plus_plus_11:
    case llvm::dwarf::DW_LANG_C_plus_plus_14:
      return ir::shared::Language::CPP;
    default:
      return error::Internal("Detected language $0 is not supported",
                             magic_enum::enum_name(source_language));
  }
}

}  // namespace

void DetectSourceLanguage(obj_tools::ElfReader* elf_reader, obj_tools::DwarfReader* dwarf_reader,
                          ir::logical::TracepointDeployment* input_program) {
  ir::shared::Language detected_language = ir::shared::Language::LANG_UNKNOWN;

  // Primary detection mechanism is DWARF info, when available.
  if (dwarf_reader != nullptr) {
    detected_language = TransformSourceLanguage(dwarf_reader->source_language())
                            .ConsumeValueOr(ir::shared::Language::LANG_UNKNOWN);
  } else {
    // Back-up detection policy looks for certain language-specific symbols
    if (IsGoExecutable(elf_reader)) {
      detected_language = ir::shared::Language::GOLANG;
    }

    // TODO(oazizi): Make this stronger by adding more elf-based tests.
  }

  if (detected_language != ir::shared::Language::LANG_UNKNOWN) {
    LOG(INFO) << absl::Substitute("Using language $0 for object $1 and others",
                                  magic_enum::enum_name(dwarf_reader->source_language()),
                                  input_program->deployment_spec().path_list().paths(0));

    // Since we only support tracing of a single object, all tracepoints have the same language.
    for (auto& tracepoint : *input_program->mutable_tracepoints()) {
      tracepoint.mutable_program()->set_language(detected_language);
    }
  } else {
    // For now, just print a warning, and let the probe proceed.
    // This is so we can use things like function argument tracing even when other features may not
    // work.
    LOG(WARNING) << absl::Substitute(
        "Language for object $0 and others is unknown or unsupported, so assuming C/C++ ABI. "
        "Some dynamic tracing features may not work, or may produce unexpected results.",
        input_program->deployment_spec().path_list().paths(0));
  }
}
namespace {

bool IsWholeWordSuffix(std::string_view name, std::string_view suffix) {
  if (!absl::EndsWith(name, suffix)) {
    return false;
  }

  name.remove_suffix(suffix.size());

  if (name.empty()) {
    return true;
  }

  char c = name.back();
  return (!std::isalnum(c) && c != '_');
}

}  // namespace

Status ResolveProbeSymbol(obj_tools::ElfReader* elf_reader,
                          ir::logical::TracepointDeployment* input_program) {
  // Expand symbol
  for (auto& t : *input_program->mutable_tracepoints()) {
    for (auto& probe : *t.mutable_program()->mutable_probes()) {
      PX_ASSIGN_OR_RETURN(
          std::vector<obj_tools::ElfReader::SymbolInfo> symbol_matches,
          elf_reader->SearchSymbols(probe.tracepoint().symbol(),
                                    obj_tools::SymbolMatchType::kSuffix, ELFIO::STT_FUNC));
      if (symbol_matches.empty()) {
        return error::Internal("Could not find symbol");
      }

      const std::string* symbol_name = nullptr;

      // First search for an exact match, since is the best we can do.
      for (const auto& candidate : symbol_matches) {
        if (probe.tracepoint().symbol() == candidate.name) {
          symbol_name = &candidate.name;
          break;
        }
      }

      // Next search for valid suffix matches.
      // A valid suffix match is one that has a special character preceding the suffix.
      // Example: Searching for Func1
      //   MyFunc1: Not a valid match
      //   (*Obj).Func1: Valid match.
      if (symbol_name == nullptr) {
        for (const auto& candidate : symbol_matches) {
          LOG(INFO) << candidate.name;
          if (IsWholeWordSuffix(candidate.name, probe.tracepoint().symbol())) {
            if (symbol_name != nullptr) {
              return error::Internal(
                  "Symbol is ambiguous. Found at least 2 possible matches: $0 -> $1", *symbol_name,
                  candidate.name);
            }
            symbol_name = &candidate.name;
          }
        }
      }

      if (symbol_name == nullptr) {
        return error::Internal("Could not find valid symbol match");
      }

      *probe.mutable_tracepoint()->mutable_symbol() = *symbol_name;
    }
  }

  return Status::OK();
}

Status AutoTraceExpansion(obj_tools::DwarfReader* dwarf_reader,
                          ir::logical::TracepointDeployment* input_program) {
  for (auto& t : *input_program->mutable_tracepoints()) {
    for (auto& probe : *t.mutable_program()->mutable_probes()) {
      if ((probe.args_size() != 0) || (probe.ret_vals_size() != 0) ||
          probe.has_function_latency()) {
        // A probe specification is explicitly provided, so use it.
        continue;
      }

      // For probes without anything to trace, we automatically trace everything:
      // args, return values and latency.
      PX_ASSIGN_OR_RETURN(auto args_map,
                          dwarf_reader->GetFunctionArgInfo(probe.tracepoint().symbol()));

      std::string table_name = probe.tracepoint().symbol() + "_table";
      table_name = absl::StrReplaceAll(
          table_name,
          {{".", "__d__"}, {"/", "__s__;"}, {"(", "__l__"}, {"(", "__r__"}, {"*", "__a__"}});

      auto* output = t.mutable_program()->add_outputs();
      output->set_name(table_name);

      auto* output_action = probe.add_output_actions();
      output_action->set_output_name(table_name);

      int i = 0;
      for (const auto& [arg_name, arg_info] : args_map) {
        if (!arg_info.retarg) {
          auto* arg = probe.add_args();
          arg->set_id("arg" + std::to_string(i));
          arg->set_expr(arg_name);

          output_action->add_variable_names(arg->id());
          output->add_fields(arg_name);
        } else {
          auto* arg = probe.add_ret_vals();
          arg->set_id("retval" + std::to_string(i));
          arg->set_expr(arg_name);

          output_action->add_variable_names(arg->id());
          output->add_fields(absl::StrReplaceAll(arg_name, {{"~", "__tilde__"}}));
        }
        ++i;
      }

      *probe.mutable_function_latency()->mutable_id() = "fn_latency";
      *output_action->add_variable_names() = probe.function_latency().id();
      *output->add_fields() = "latency";
    }
  }

  return Status::OK();
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
