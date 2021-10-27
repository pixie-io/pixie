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

#include <deque>
#include <limits>
#include <string>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace obj_tools {

enum class ABI {
  kUnknown = 0,
  kSystemVAMD64,
  kGolangStack,
  kGolangRegister,
};

// Identifies where an variable is located.
enum class LocationType {
  kUnknown,
  // Stack address relative to the frame stack pointer (SP)
  kStack,
  // Stack address relative to the frame base pointer (BP)
  kStackBP,
  // Integer register.
  kRegister,
  // Floating-point register.
  kRegisterFP
};

enum class RegisterName : int {
  kRAX = 0,
  kRBX = 1,
  kRCX = 2,
  kRDX = 3,
  kRDI = 4,
  kRSI = 5,
  kR8 = 6,
  kR9 = 7,
  kR10 = 8,
  kR11 = 9,

  kXMM0 = 100,
  kXMM1 = 101,
  kXMM2 = 102,
  kXMM3 = 103,
  kXMM4 = 104,
  kXMM5 = 105,
  kXMM6 = 106,
  kXMM7 = 107,
  kXMM8 = 108,
  kXMM9 = 109,
  kXMM10 = 110,
  kXMM11 = 111,
  kXMM12 = 112,
  kXMM13 = 113,
  kXMM14 = 114,
};

// Location of a variable or argument.
// The location may be on the stack or in registers.
struct VarLocation {
  LocationType loc_type = LocationType::kUnknown;

  // For stack locations, the offset represents the offset off the SP.
  // For register locations, the offset represents the offset in the register space,
  // assuming all the available registers were concatenated into a single scratch space of memory.
  int64_t offset = std::numeric_limits<uint64_t>::max();

  // For register locations, the list of registers where the variable is located.
  std::vector<RegisterName> registers = {};

  std::string ToString() const {
    std::string registers_str = "[";
    registers_str +=
        absl::StrJoin(registers, ",", [](std::string* out, const RegisterName& reg_name) {
          absl::StrAppend(out, magic_enum::enum_name(reg_name));
        });
    registers_str += "]";
    return absl::Substitute("type=$0 offset=$1 registers=$2", magic_enum::enum_name(loc_type),
                            offset, registers_str);
  }
};

// FunctionArgTracker's responsibility is to determine where
// arguments are located: in memory (i.e. on the stack) or in registers.
// FunctionArgTracker keeps track of stack/register positions that have been
// used for each successive argument, so that it can report the next location.
// For Golang, everything is always on the stack, so the algorithm is easy.
// For C/C++, which uses the System V ABI, the rules are more complex:
//   https://uclibc.org/docs/psABI-x86_64.pdf
// TODO(oazizi): Finish implementing the rules.
class FunctionArgTracker {
 public:
  explicit FunctionArgTracker(ABI abi);

  StatusOr<VarLocation> PopLocation(uint64_t type_size, uint64_t alignment_size);

  void AdjustForReturnValue(uint64_t ret_val_size);

 private:
  const uint64_t reg_size_;

  uint64_t current_stack_offset_ = 0;
  uint64_t current_shadow_reg_offset_ = 0;

  std::deque<RegisterName> int_arg_registers_;
  std::deque<RegisterName> fp_arg_registers_;
  std::deque<RegisterName> int_retval_registers_;
  std::deque<RegisterName> fp_retval_registers_;
};

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
