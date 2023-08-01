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

#include <memory>
#include <string>
#include <vector>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/proc_parser.h"
#include "src/common/system/proc_pid_path.h"
#include "src/stirling/obj_tools/address_converter.h"

namespace px {
namespace stirling {
namespace obj_tools {

using ProcSMaps = system::ProcParser::ProcessSMaps;

uint64_t ElfAddressConverter::VirtualAddrToBinaryAddr(uint64_t virtual_addr) const {
  return virtual_addr + virtual_to_binary_addr_offset_;
}

uint64_t ElfAddressConverter::BinaryAddrToVirtualAddr(uint64_t binary_addr) const {
  return binary_addr - virtual_to_binary_addr_offset_;
}

StatusOr<uint32_t> GetProcMapsIndexForBinary(const std::string& binary_path,
                                             const std::vector<ProcSMaps>& map_entries) {
  for (const auto& [idx, entry] : Enumerate(map_entries)) {
    if (absl::EndsWith(binary_path, entry.pathname)) {
      return idx;
    }
  }
  return error::NotFound("");
}

/**
 * The calculated offset is used to convert between virtual addresses (eg. the address you
 * would get from a function pointer) and "binary" addresses (i.e. the address that `nm` would
 * display for a given function).
 *
 * This conversion is non-trivial and requires information from both the ELF file of the binary in
 * question, as well as the /proc/$PID/maps file for the PID of the process in question.
 *
 * For non-PIE executables, this conversion is trivial as the virtual addresses in the ELF file are
 * used directly when loading.
 *
 * However, for PIE, the loaded virtual address can be whatever. To calculate the offset we must
 * find the /proc/$PID/maps entry that corresponds to the given process's executable (entry that
 * matches /proc/$PID/exe) and use that entry's virtual memory offset to find the binary
 * address.
 *
 **/
StatusOr<std::unique_ptr<ElfAddressConverter>> ElfAddressConverter::Create(ElfReader* elf_reader,
                                                                           int64_t pid) {
  // If the binary is not a PIE binary, then we can skip calculating the offset.
  if (elf_reader->ELFType() != ELFIO::ET_DYN) {
    return std::unique_ptr<ElfAddressConverter>(new ElfAddressConverter(0));
  }
  if (pid <= 0) {
    return Status(statuspb::INVALID_ARGUMENT,
                  absl::Substitute("ElfAddressConverter::Create: Invalid pid=$0", pid));
  }
  system::ProcParser parser;
  std::vector<system::ProcParser::ProcessSMaps> map_entries;
  PX_RETURN_IF_ERROR(parser.ParseProcPIDMaps(pid, &map_entries));
  if (map_entries.size() < 1) {
    return Status(
        statuspb::INTERNAL,
        absl::Substitute("ElfAddressConverter::Create: Failed to parse /proc/$0/maps", pid));
  }

  PX_ASSIGN_OR_RETURN(const auto proc_exe, parser.GetExePath(pid));
  // Prior to pixie#1630, we believed that a process's VMA space would always have its executable
  // at the first (lowest) virtual memory address. This isn't always the case, however, if we fail
  // to match against a /proc/$PID/maps entry, default to the first one.
  auto map_entry = map_entries[0];
  auto idx_status = GetProcMapsIndexForBinary(proc_exe, map_entries);
  if (idx_status.ok()) {
    map_entry = map_entries[idx_status.ConsumeValueOrDie()];
  } else {
    LOG(WARNING) << absl::Substitute(
        "Failed to find match for $0 in /proc/$1/maps. Defaulting to the first entry",
        proc_exe.string(), pid);
  }
  const auto mapped_virt_addr = map_entry.vmem_start;
  uint64_t mapped_offset;
  if (!absl::SimpleHexAtoi(map_entry.offset, &mapped_offset)) {
    return Status(statuspb::INTERNAL,
                  absl::Substitute(
                      "ElfAddressConverter::Create: Failed to parse offset in /proc/$0/maps", pid));
  }

  const uint64_t mapped_segment_start = mapped_virt_addr - mapped_offset;

  PX_ASSIGN_OR_RETURN(auto elf_segment_start, elf_reader->GetVirtualAddrAtOffsetZero());

  const int64_t virtual_to_binary_addr_offset = elf_segment_start - mapped_segment_start;
  return std::unique_ptr<ElfAddressConverter>(
      new ElfAddressConverter(virtual_to_binary_addr_offset));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
