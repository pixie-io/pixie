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

#include <string>
#include <string_view>

#include "src/stirling/obj_tools/elf_tools.h"

namespace px {
namespace stirling {
namespace obj_tools {

// Returns true if the executable is built by Golang.
bool IsGoExecutable(ElfReader* elf_reader);

// Returns the build version of a Golang executable. The executable is read through the input
// elf_reader.
// TODO(yzhao): We'll use this to determine the corresponding Golang executable's TLS data
// structures and their offsets.
StatusOr<std::string> ReadBuildVersion(ElfReader* elf_reader);

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
