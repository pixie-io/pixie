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

}  // namespace

std::unique_ptr<ABICallingConventionModel> ABICallingConventionModel::Create(ABI abi) {
  switch (abi) {
    case ABI::kSystemVAMD64:
      return std::unique_ptr<ABICallingConventionModel>(new SysVABIModel());
      break;
    case ABI::kGolangStack:
      return std::unique_ptr<ABICallingConventionModel>(new GolangStackABIModel());
      break;
    case ABI::kGolangRegister:
      return std::unique_ptr<ABICallingConventionModel>(new GolangRegABIModel());
      break;
    default:
      LOG(DFATAL) << absl::Substitute("Unsupported ABI: $0", magic_enum::enum_name(abi));
  }

  return nullptr;
}

//-----------------------------------------------------------------------------
// GolangStackABIModel
//-----------------------------------------------------------------------------

GolangStackABIModel::GolangStackABIModel() {}

StatusOr<VarLocation> GolangStackABIModel::PopLocation(TypeClass /* type_class */,
                                                       uint64_t type_size, uint64_t alignment_size,
                                                       int /* num_vars */, bool /* is_ret_arg */) {
  VarLocation location;

  // Align to the type's required alignment.
  current_stack_offset_ = SnapUpToMultiple<int64_t>(current_stack_offset_, alignment_size);
  location.loc_type = LocationType::kStack;
  location.offset = current_stack_offset_;

  current_stack_offset_ += type_size;

  return location;
}

//-----------------------------------------------------------------------------
// GolangRegABIModel
//-----------------------------------------------------------------------------

GolangRegABIModel::GolangRegABIModel() : reg_size_(RegisterSize()) {
  int_arg_registers_ = {RegisterName::kRAX, RegisterName::kRBX, RegisterName::kRCX,
                        RegisterName::kRDI, RegisterName::kRSI, RegisterName::kR8,
                        RegisterName::kR9,  RegisterName::kR10, RegisterName::kR11};

  fp_arg_registers_ = {RegisterName::kXMM0,  RegisterName::kXMM1,  RegisterName::kXMM2,
                       RegisterName::kXMM3,  RegisterName::kXMM4,  RegisterName::kXMM5,
                       RegisterName::kXMM6,  RegisterName::kXMM7,  RegisterName::kXMM8,
                       RegisterName::kXMM9,  RegisterName::kXMM10, RegisterName::kXMM11,
                       RegisterName::kXMM12, RegisterName::kXMM13, RegisterName::kXMM14};

  int_retval_registers_ = {RegisterName::kRAX, RegisterName::kRBX, RegisterName::kRCX,
                           RegisterName::kRDI, RegisterName::kRSI, RegisterName::kR8,
                           RegisterName::kR9,  RegisterName::kR10, RegisterName::kR11};

  fp_retval_registers_ = {RegisterName::kXMM0,  RegisterName::kXMM1,  RegisterName::kXMM2,
                          RegisterName::kXMM3,  RegisterName::kXMM4,  RegisterName::kXMM5,
                          RegisterName::kXMM6,  RegisterName::kXMM7,  RegisterName::kXMM8,
                          RegisterName::kXMM9,  RegisterName::kXMM10, RegisterName::kXMM11,
                          RegisterName::kXMM12, RegisterName::kXMM13, RegisterName::kXMM14};
}

StatusOr<VarLocation> GolangRegABIModel::PopLocation(TypeClass type_class, uint64_t type_size,
                                                     uint64_t alignment_size, int num_vars,
                                                     bool is_ret_arg) {
  std::deque<RegisterName>* registers = nullptr;
  int32_t* reg_offset = nullptr;

  if (type_class == TypeClass::kInteger) {
    if (is_ret_arg) {
      registers = &int_retval_registers_;
      reg_offset = &current_int_retval_reg_offset_;
    } else {
      registers = &int_arg_registers_;
      reg_offset = &current_int_arg_reg_offset_;
    }
  } else if (type_class == TypeClass::kFloat) {
    if (is_ret_arg) {
      registers = &fp_retval_registers_;
      reg_offset = &current_fp_retval_reg_offset_;
    } else {
      registers = &fp_arg_registers_;
      reg_offset = &current_fp_arg_reg_offset_;
    }
  } else {
    return error::Unimplemented("TypeClass not yet supported $0",
                                magic_enum::enum_name(type_class));
  }

  VarLocation location;

  if (num_vars <= static_cast<int>(registers->size())) {
    location.loc_type =
        (type_class == TypeClass::kInteger) ? LocationType::kRegister : LocationType::kRegisterFP;
    location.offset = *reg_offset;
    for (int i = 0; i < num_vars; ++i) {
      location.registers.push_back(registers->front());
      registers->pop_front();
    }
    *reg_offset += num_vars * reg_size_;
  } else {
    // Align to the type's required alignment.
    current_stack_offset_ = SnapUpToMultiple<int64_t>(current_stack_offset_, alignment_size);
    location.loc_type = LocationType::kStack;
    location.offset = current_stack_offset_;

    current_stack_offset_ += type_size;
  }

  return location;
}

//-----------------------------------------------------------------------------
// SysVABIModel
//-----------------------------------------------------------------------------

SysVABIModel::SysVABIModel() : reg_size_(RegisterSize()) {
  int_arg_registers_ = {RegisterName::kRDI, RegisterName::kRSI, RegisterName::kRDX,
                        RegisterName::kRCX, RegisterName::kR8,  RegisterName::kR9};

  fp_arg_registers_ = {RegisterName::kXMM0, RegisterName::kXMM1, RegisterName::kXMM2,
                       RegisterName::kXMM3, RegisterName::kXMM4, RegisterName::kXMM5,
                       RegisterName::kXMM6, RegisterName::kXMM7};

  int_retval_registers_ = {RegisterName::kRAX, RegisterName::kRDX};

  fp_retval_registers_ = {};
}

// TODO(oazizi): Function is getting long. Consider splitting it up.
StatusOr<VarLocation> SysVABIModel::PopLocation(TypeClass type_class, uint64_t type_size,
                                                uint64_t alignment_size, int /* num_vars */,
                                                bool is_ret_arg) {
  std::deque<RegisterName>* registers = nullptr;
  int32_t* reg_offset = nullptr;

  if (type_class == TypeClass::kInteger) {
    if (is_ret_arg) {
      registers = &int_retval_registers_;
      reg_offset = &current_int_retval_reg_offset_;
    } else {
      registers = &int_arg_registers_;
      reg_offset = &current_int_arg_reg_offset_;
    }
  } else if (type_class == TypeClass::kFloat) {
    if (is_ret_arg) {
      registers = &fp_retval_registers_;
      reg_offset = &current_fp_retval_reg_offset_;
    } else {
      registers = &fp_arg_registers_;
      reg_offset = &current_fp_arg_reg_offset_;
    }
  } else {
    return error::Unimplemented("TypeClass not yet supported $0",
                                magic_enum::enum_name(type_class));
  }

  VarLocation location;

  int num_regs_required = IntRoundUpDivide(type_size, reg_size_);

  if (is_ret_arg) {
    if (num_regs_required > static_cast<int>(registers->size())) {
      // There is an implicit argument which is the pointer to the return value. Pop it off.
      // Return value in this case is different, since we return the location of the hidden argument
      // which points to the return value.
      // TODO(oazizi): Fix the return value so it isn't different. Fortunately , there are no users
      //               of this case yet. DwarfReader currently throws the returned location away.
      location.loc_type = LocationType::kRegister;
      location.registers.push_back(int_arg_registers_.front());
      location.offset = current_int_arg_reg_offset_;

      int_arg_registers_.pop_front();
      current_int_arg_reg_offset_ += reg_size_;
    } else {
      location.loc_type =
          (type_class == TypeClass::kInteger) ? LocationType::kRegister : LocationType::kRegisterFP;
      location.offset = *reg_offset;
      for (int i = 0; i < num_regs_required; ++i) {
        location.registers.push_back(registers->front());
        registers->pop_front();
      }
      *reg_offset += num_regs_required * reg_size_;
    }
  } else {
    constexpr size_t kMaxTypeSizeForRegisterPassing = 16;
    if (type_size <= kMaxTypeSizeForRegisterPassing &&
        num_regs_required <= static_cast<int>(registers->size())) {
      location.loc_type =
          (type_class == TypeClass::kInteger) ? LocationType::kRegister : LocationType::kRegisterFP;
      location.offset = *reg_offset;
      for (int i = 0; i < num_regs_required; ++i) {
        location.registers.push_back(registers->front());
        registers->pop_front();
      }
      *reg_offset += num_regs_required * reg_size_;
    } else {
      // Align to the type's required alignment.
      current_stack_offset_ = SnapUpToMultiple<int64_t>(current_stack_offset_, alignment_size);
      location.loc_type = LocationType::kStack;
      location.offset = current_stack_offset_;

      current_stack_offset_ += type_size;
    }
  }

  return location;
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
