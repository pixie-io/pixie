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

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Support/TargetSelect.h>

#include <string>
#include <string_view>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace obj_tools {

/**
 * Converts llvm::Optional<T> to StatusOr<T>.
 * When the optional value is not present, an error with the provided message is returned.
 */
template <typename TValueType>
StatusOr<TValueType> AdaptLLVMOptional(llvm::Optional<TValueType>&& llvm_opt,
                                       std::string_view msg) {
  if (!llvm_opt.hasValue()) {
    return error::Internal(msg);
  }
  return llvm_opt.getValue();
}

/**
 * Returns the DW_AT_name attribute of the input DIE.
 * Returns an empty string if attribute does not exist, or for any errors.
 */
std::string_view GetShortName(const llvm::DWARFDie& die);

/**
 * Returns the DW_AT_linkage_name attribute of the input DIE.
 * Returns an empty string if attribute does not exist, or for any errors.
 */
std::string_view GetLinkageName(const llvm::DWARFDie& die);

/**
 * Returns the specified attribute (DW_AT_*) for the die.
 */
StatusOr<llvm::DWARFFormValue> GetAttribute(const llvm::DWARFDie& die,
                                            llvm::dwarf::Attribute attribute);

/**
 * Returns the text representation of the input DIE.
 */
std::string Dump(const llvm::DWARFDie& die);

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
