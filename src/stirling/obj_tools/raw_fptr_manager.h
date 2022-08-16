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

#include <string>

#include "src/common/system/proc_parser.h"
#include "src/stirling/obj_tools/elf_reader.h"

namespace px {
namespace stirling {
namespace obj_tools {

// A class that opens a shared object, and gives access to function pointers by symbol.
// Unlike the typical dlopen()/dlsym() usage, this also enables access to static functions,
// provided the symbols are not stripped.
class RawFptrManager : NotCopyMoveable {
 public:
  RawFptrManager(obj_tools::ElfReader* elf_reader, ::px::system::ProcParser* proc_parser,
                 std::string lib_path);

  ~RawFptrManager();

  template <class T>
  StatusOr<T*> RawSymbolToFptr(const std::string& symbol_name) {
    PL_ASSIGN_OR_RETURN(void* fptr, RawSymbolToFptrImpl(symbol_name));
    return reinterpret_cast<T*>(fptr);
  }

  // Required before calling RawSymbolToFptr. Returns error if shared object cannot be loaded.
  Status Init();

 private:
  StatusOr<void*> RawSymbolToFptrImpl(const std::string& symbol_name);

  obj_tools::ElfReader* elf_reader_;
  ::px::system::ProcParser* proc_parser_;
  uint64_t text_segment_offset_;
  void* dlopen_handle_;
  std::string lib_path_;
  ::px::system::ProcParser::ProcessSMaps map_entry_;
};

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
