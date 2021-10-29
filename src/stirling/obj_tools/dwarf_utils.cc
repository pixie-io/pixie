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

#include "src/stirling/obj_tools/dwarf_utils.h"

#include <llvm/DebugInfo/DIContext.h>
#include <llvm/Object/ObjectFile.h>

namespace px {
namespace stirling {
namespace obj_tools {

using llvm::DWARFContext;
using llvm::DWARFDie;
using llvm::DWARFFormValue;

std::string_view GetShortName(const DWARFDie& die) {
  const char* name = die.getShortName();
  return (name == nullptr) ? "" : name;
}

std::string_view GetLinkageName(const DWARFDie& die) {
  const char* name = die.getLinkageName();
  return (name == nullptr) ? "" : name;
}

StatusOr<DWARFFormValue> GetAttribute(const DWARFDie& die, llvm::dwarf::Attribute attribute) {
  return AdaptLLVMOptional(die.find(attribute),
                           absl::Substitute("Could not find attribute $0 in DIE $1",
                                            magic_enum::enum_name(attribute), GetShortName(die)));
}

std::string Dump(const llvm::DWARFDie& die) {
  std::string buf;
  llvm::raw_string_ostream rso(buf);
  die.dump(rso);
  rso.flush();
  return buf;
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
