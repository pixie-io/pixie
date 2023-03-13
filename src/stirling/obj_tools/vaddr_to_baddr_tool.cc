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

#include <absl/strings/numbers.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/obj_tools/address_converter.h"

namespace px {
namespace stirling {
namespace obj_tools {

Status convert(const std::string binary_path, const int pid, const uint64_t v_addr) {
  PX_ASSIGN_OR_RETURN(const auto elf_reader, ElfReader::Create(binary_path));
  PX_ASSIGN_OR_RETURN(const auto converter, ElfAddressConverter::Create(elf_reader.get(), pid));
  const uint64_t b_addr = converter->VirtualAddrToBinaryAddr(v_addr);
  std::cout << absl::StrFormat("binary_path: %s.", binary_path) << std::endl;
  std::cout << absl::StrFormat("pid: %d.", pid) << std::endl;
  std::cout << absl::StrFormat("virtual addr: 0x%016lx.", v_addr) << std::endl;
  std::cout << absl::StrFormat("binary addr:  0x%016lx.", b_addr) << std::endl;
  return Status::OK();
}

Status read_args_and_convert(int argc, char** argv) {
  if (argc != 4) {
    return error::InvalidArgument("Usage:\n$0 <binary path> <pid> <virtual addr>.", argv[0]);
  }

  const std::string binary_path(argv[1]);
  if (!px::fs::Exists(binary_path)) {
    return error::ResourceUnavailable("Could not find file: $0.", binary_path);
  }

  int pid = 0;
  if (!absl::SimpleAtoi(argv[2], &pid)) {
    return error::Internal("Could not parse pid argument: $0.", argv[2]);
  }

  uint64_t vaddr = 0;
  if (!absl::SimpleHexAtoi(argv[3], &vaddr)) {
    return error::Internal("Could not parse virtual addr argument: $0.", argv[3]);
  }

  return convert(binary_path, pid, vaddr);
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);
  const auto s = px::stirling::obj_tools::read_args_and_convert(argc, argv);

  if (!s.ok()) {
    LOG(WARNING) << s.msg();
    return -1;
  }

  return 0;
}
