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
#include <memory>
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

// Identifies where a variable is located.
// TODO(oazizi): Consider consolidating with location_t in symaddrs.h.
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

enum class TypeClass { kNone, kInteger, kFloat, kMixed };

inline TypeClass Combine(TypeClass a, TypeClass b) {
  if (a == TypeClass::kMixed || b == TypeClass::kMixed) {
    return TypeClass::kMixed;
  }

  if (a == TypeClass::kNone) {
    return b;
  }

  if (b == TypeClass::kNone) {
    return a;
  }

  DCHECK(a == TypeClass::kInteger || a == TypeClass::kFloat);
  DCHECK(b == TypeClass::kInteger || b == TypeClass::kFloat);

  if (a != b) {
    return TypeClass::kMixed;
  }

  return a;
}

// Location of a variable or argument.
// The location may be on the stack or in registers.
struct VarLocation {
  LocationType loc_type = LocationType::kUnknown;

  // For stack locations, the offset represents the offset off the SP.
  // For register locations, the offset represents the offset in the register space,
  // assuming all the available registers were concatenated into a single scratch space of memory.
  int64_t offset = 0;

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

// ABICallingConventionModel's responsibility is to determine where arguments are located on a
// function call: in memory (i.e. on the stack) or in registers. ABICallingConventionModel keeps
// track of stack/register positions that have been used for each successive argument, so that it
// can report the next location.
class ABICallingConventionModel {
 public:
  static std::unique_ptr<ABICallingConventionModel> Create(ABI abi);

  virtual ~ABICallingConventionModel() = default;

  /**
   * Get the location of the next argument.
   *
   * @param type_class Class of the argument, which may be a struct (e.g. integer, float, mixed).
   * @param type_size The size of the argument when laid out in memory.
   * @param alignment_size The size of the largest primitive member inside the argument,
   *                       used for determining the alignment of the argument in memory.
   * @param num_vars The number of primitive variables in the argument.
   *                 This is 1 for a primitive type.
   *                 For structs, it counts the number of primitives after a full traversal.
   * @return The location where the argument would be placed according to the calling convention.
   */
  virtual StatusOr<VarLocation> PopLocation(TypeClass type_class, uint64_t type_size,
                                            uint64_t alignment_size, int num_vars,
                                            bool is_ret_arg) = 0;
};

/**
 * A model for the original Golang ABI, where arguments are always passed through the stack.
 */
class GolangStackABIModel : public ABICallingConventionModel {
 public:
  GolangStackABIModel();
  ~GolangStackABIModel() = default;

  StatusOr<VarLocation> PopLocation(TypeClass type_class, uint64_t type_size,
                                    uint64_t alignment_size, int num_vars,
                                    bool is_ret_arg) override;

 private:
  int32_t current_stack_offset_ = 0;
};

/**
 * A model for the original Golang register ABI, where arguments are passed through registers,
 * and/or the stack, according to the ABI's rules.
 */
class GolangRegABIModel : public ABICallingConventionModel {
 public:
  GolangRegABIModel();
  ~GolangRegABIModel() = default;

  StatusOr<VarLocation> PopLocation(TypeClass type_class, uint64_t type_size,
                                    uint64_t alignment_size, int num_vars,
                                    bool is_ret_arg) override;

 private:
  const uint64_t reg_size_;

  int32_t current_stack_offset_ = 0;
  int32_t current_int_arg_reg_offset_ = 0;
  int32_t current_fp_arg_reg_offset_ = 0;
  int32_t current_int_retval_reg_offset_ = 0;
  int32_t current_fp_retval_reg_offset_ = 0;

  // There are separate registers for integer-values (int) and floating point values (fp).
  // Registers are also reused for argument passing (arg) and return values (retval).
  std::deque<RegisterName> int_arg_registers_;
  std::deque<RegisterName> fp_arg_registers_;
  std::deque<RegisterName> int_retval_registers_;
  std::deque<RegisterName> fp_retval_registers_;
};

/**
 * The System V ABI model, used by C/C++ programs.
 * The calling convention uses registers and the stack.
 * Reference: https://uclibc.org/docs/psABI-x86_64.pdf
 */
// TODO(oazizi): Finish implementing the rules.
class SysVABIModel : public ABICallingConventionModel {
 public:
  SysVABIModel();
  ~SysVABIModel() = default;

  StatusOr<VarLocation> PopLocation(TypeClass type_class, uint64_t type_size,
                                    uint64_t alignment_size, int num_vars,
                                    bool is_ret_arg) override;

 private:
  const uint64_t reg_size_;

  int32_t current_stack_offset_ = 0;
  int32_t current_fp_arg_reg_offset_ = 0;
  int32_t current_int_arg_reg_offset_ = 0;
  int32_t current_fp_retval_reg_offset_ = 0;
  int32_t current_int_retval_reg_offset_ = 0;

  std::deque<RegisterName> int_arg_registers_;
  std::deque<RegisterName> fp_arg_registers_;
  std::deque<RegisterName> int_retval_registers_;
  std::deque<RegisterName> fp_retval_registers_;
};

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
