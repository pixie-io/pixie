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

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/elf_reader.h"

namespace px {
namespace stirling {
namespace obj_tools {

class ElfAddressConverter {
 public:
  static StatusOr<std::unique_ptr<ElfAddressConverter>> Create(ElfReader* elf_reader, int64_t pid);
  uint64_t VirtualAddrToBinaryAddr(uint64_t virtual_addr) const;
  uint64_t BinaryAddrToVirtualAddr(uint64_t binary_addr) const;

 private:
  explicit ElfAddressConverter(int64_t offset) : virtual_to_binary_addr_offset_(offset) {}
  int64_t virtual_to_binary_addr_offset_;
};

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
