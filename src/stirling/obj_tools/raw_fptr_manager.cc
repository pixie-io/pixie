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

#include "src/stirling/obj_tools/raw_fptr_manager.h"

#include <dlfcn.h>
#include <link.h>

#include <string>
#include <utility>

#include "src/stirling/obj_tools/elf_reader.h"

using ::px::stirling::obj_tools::ElfReader;

namespace px {
namespace stirling {
namespace obj_tools {

RawFptrManager::RawFptrManager(std::string lib_path) : lib_path_(std::move(lib_path)) {}

Status RawFptrManager::LazyInit() {
  if (elf_reader_ == nullptr) {
    PX_ASSIGN_OR_RETURN(elf_reader_, ElfReader::Create(lib_path_));
  }

  if (dl_vmem_start_ == 0) {
    dlopen_handle_ = dlopen(lib_path_.c_str(), RTLD_LAZY);
    if (dlopen_handle_ == nullptr) {
      return error::Internal("Failed to dlopen so file: $0, $1", lib_path_, dlerror());
    }

    struct link_map* dl_link_map = nullptr;
    int retval = dlinfo(dlopen_handle_, RTLD_DI_LINKMAP, &dl_link_map);

    if (retval != 0) {
      return error::Internal("dlinfo() failed to return info [dlerror=$0].", dlerror());
    }

    if (dl_link_map == nullptr) {
      return error::Internal("dlinfo() returned nullptr.");
    }

    // The link_map is a linked list, but the last element, and the one that is returned by dinfo()
    // should be the library that we just loaded.
    // Because containerized environments can interfere with the paths, just check the filenames.
    DCHECK_EQ(std::filesystem::path(dl_link_map->l_name).filename(),
              std::filesystem::path(lib_path_).filename());

    dl_vmem_start_ = dl_link_map->l_addr;
  }

  return Status::OK();
}

StatusOr<void*> RawFptrManager::RawSymbolToFptrImpl(const std::string& symbol_name) {
  // Doesn't do anything if it has been called before.
  PX_RETURN_IF_ERROR(LazyInit());

  auto sym_addr = elf_reader_->SymbolAddress(symbol_name).value();
  if (!sym_addr) {
    return error::NotFound("Could not find symbol '$0'", symbol_name);
  }

  uint64_t fptr_addr = sym_addr + dl_vmem_start_;
  return reinterpret_cast<void*>(fptr_addr);
}

RawFptrManager::~RawFptrManager() {
  if (dlopen_handle_ != nullptr) {
    VLOG(1) << absl::Substitute("Closing dlopen handle for $0", lib_path_);
    dlclose(dlopen_handle_);
  }
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
