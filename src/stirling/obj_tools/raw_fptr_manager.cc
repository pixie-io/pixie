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

#include <string>

#include "src/stirling/obj_tools/elf_reader.h"

using ::px::stirling::obj_tools::ElfReader;

namespace px {
namespace stirling {
namespace obj_tools {

RawFptrManager::RawFptrManager(ElfReader* elf_reader, system::ProcParser* proc_parser,
                               std::string lib_path)
    : elf_reader_(elf_reader),
      proc_parser_(proc_parser),
      dlopen_handle_(nullptr),
      lib_path_(lib_path) {}

Status RawFptrManager::Init() {
  dlopen_handle_ = dlopen(lib_path_.c_str(), RTLD_LAZY);
  if (dlopen_handle_ == nullptr) {
    return error::Internal("Failed to dlopen OpenSSL so file: $0, $1", lib_path_, dlerror());
  }

  PL_ASSIGN_OR_RETURN(text_segment_offset_, elf_reader_->FindSegmentOffsetOfSection(".text"));
  auto pid = getpid();

  // The dlopen pointer will store the address of the shared library's virtual memory location.
  // This is dependent on the implementation details of the dl library and was discovered
  // from the following forum post
  // https://www.linuxquestions.org/questions/programming-9/getting-base-address-of-dynamic-library-256670/#post4189790
  // It's crucial to find the correct /prod/<pid>/maps entry otherwise we cannot guarantee that
  // it will still be mapped when the function is later invoked. In practice, this appears to happen
  // because the openssl_trace_bpf_tests segfault without this additional verification.
  auto vmem_start = text_segment_offset_ + (uint64_t) * (size_t const*)dlopen_handle_;
  auto entry = proc_parser_->GetExecutableMapEntry(pid, lib_path_, vmem_start);
  if (!entry.ok()) {
    return error::NotFound(
        "Failed to find map entry for pid: $0 and path: $1 vmem start: $2 and segment offset: $3",
        pid, lib_path_, absl::Hex(vmem_start), absl::Hex(text_segment_offset_));
  }

  map_entry_ = entry.ValueOrDie();
  return Status::OK();
}

StatusOr<void*> RawFptrManager::RawSymbolToFptrImpl(const std::string& symbol_name) {
  auto sym_addr = elf_reader_->SymbolAddress(symbol_name).value();
  if (!sym_addr) {
    return error::NotFound("Could not find symbol '$0'", symbol_name);
  }

  uint64_t fptr_addr = sym_addr - text_segment_offset_ + map_entry_.vmem_start;
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
