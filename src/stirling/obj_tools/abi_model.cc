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

#include "src/stirling/obj_tools/abi_model.h"

namespace px {
namespace stirling {
namespace obj_tools {

namespace {

// TODO(oazizi): This is a placeholder. This information can come from DWARF.
uint32_t RegisterSize() { return 8; }

// This function takes a value, and if it is not a multiple of the `size` parameter,
// it rounds it up to so that it is aligned to the given `size`.
// Examples:
//   SnapUpToMultiple(64, 8) = 64
//   SnapUpToMultiple(66, 8) = 72
uint64_t SnapUpToMultiple(uint64_t val, uint64_t size) {
  // Alternate implementation: std::ceil(val / size) * size.
  // But the one below avoids floating point math.
  return ((val + (size - 1)) / size) * size;
}

}  // namespace

// TODO(oazizi): Consider a better way to inject the ABI parameters.
FunctionArgTracker::FunctionArgTracker(ABI abi) : reg_size_(RegisterSize()) {
  // Initialize parameters for the ABI model.
  switch (abi) {
    case ABI::kSystemVAMD64:
      int_arg_registers_ = {RegisterName::kRDI, RegisterName::kRSI, RegisterName::kRDX,
                            RegisterName::kRCX, RegisterName::kR8,  RegisterName::kR9};
      int_retval_registers_ = {RegisterName::kRAX, RegisterName::kRDX};
      fp_arg_registers_ = {RegisterName::kXMM0, RegisterName::kXMM1, RegisterName::kXMM2,
                           RegisterName::kXMM3, RegisterName::kXMM4, RegisterName::kXMM5,
                           RegisterName::kXMM6, RegisterName::kXMM7};
      fp_retval_registers_ = {};
      break;
    case ABI::kGolangStack:
      int_arg_registers_ = {};
      fp_arg_registers_ = {};
      int_retval_registers_ = {};
      fp_arg_registers_ = {};
      break;
    case ABI::kGolangRegister:
      int_arg_registers_ = {RegisterName::kRAX, RegisterName::kRBX, RegisterName::kRCX,
                            RegisterName::kRDI, RegisterName::kRSI, RegisterName::kR8,
                            RegisterName::kR9,  RegisterName::kR10, RegisterName::kR11};
      int_retval_registers_ = {};
      fp_arg_registers_ = {RegisterName::kXMM0,  RegisterName::kXMM1,  RegisterName::kXMM2,
                           RegisterName::kXMM3,  RegisterName::kXMM4,  RegisterName::kXMM5,
                           RegisterName::kXMM6,  RegisterName::kXMM7,  RegisterName::kXMM8,
                           RegisterName::kXMM9,  RegisterName::kXMM10, RegisterName::kXMM11,
                           RegisterName::kXMM12, RegisterName::kXMM13, RegisterName::kXMM14};
      fp_retval_registers_ = {};
      break;
    default:
      LOG(DFATAL) << absl::Substitute("Unsupported ABI: $0", magic_enum::enum_name(abi));
  }
}

StatusOr<VarLocation> FunctionArgTracker::PopLocation(uint64_t type_size, uint64_t alignment_size) {
  VarLocation location;

  int num_regs_required = IntRoundUpDivide(type_size, reg_size_);
  if (type_size <= 16 &&
      num_regs_required <= static_cast<int>(int_arg_registers_.size() * reg_size_)) {
    location.loc_type = LocationType::kRegister;
    location.offset = current_shadow_reg_offset_;
    for (int i = 0; i < num_regs_required; ++i) {
      location.registers.push_back(int_arg_registers_.front());
      int_arg_registers_.pop_front();
    }
    current_shadow_reg_offset_ += SnapUpToMultiple(type_size, reg_size_);
  } else {
    // Align to the type's required alignment.
    current_stack_offset_ = SnapUpToMultiple(current_stack_offset_, alignment_size);
    location.loc_type = LocationType::kStack;
    location.offset = current_stack_offset_;

    current_stack_offset_ += type_size;
  }

  return location;
}

void FunctionArgTracker::AdjustForReturnValue(uint64_t ret_val_size) {
  int num_regs_required = IntRoundUpDivide(ret_val_size, reg_size_);
  if (num_regs_required > static_cast<int>(int_retval_registers_.size())) {
    // There is an implicit argument which is the pointer to the return value. Pop it off.
    int_arg_registers_.pop_front();
    current_shadow_reg_offset_ += reg_size_;
  }
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
