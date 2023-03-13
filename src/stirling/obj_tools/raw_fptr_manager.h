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

#include <memory>
#include <string>

#include "src/stirling/obj_tools/elf_reader.h"

namespace px {
namespace stirling {
namespace obj_tools {

// A class that opens a shared object, and gives access to function pointers by symbol.
// Unlike the typical dlopen()/dlsym() usage, this also enables access to static functions,
// provided the symbols are not stripped.
class RawFptrManager : NotCopyMoveable {
 public:
  explicit RawFptrManager(std::string lib_path);

  ~RawFptrManager();

  template <class T>
  StatusOr<T*> RawSymbolToFptr(const std::string& symbol_name) {
    PX_ASSIGN_OR_RETURN(void* fptr, RawSymbolToFptrImpl(symbol_name));
    return reinterpret_cast<T*>(fptr);
  }

 private:
  // Runs the the first time RawSymbolToFptr() is called.
  // Returns error if shared object cannot be loaded.
  Status LazyInit();

  StatusOr<void*> RawSymbolToFptrImpl(const std::string& symbol_name);

  std::string lib_path_;
  std::unique_ptr<obj_tools::ElfReader> elf_reader_;
  void* dlopen_handle_ = nullptr;
  uint64_t dl_vmem_start_ = 0;
};

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
