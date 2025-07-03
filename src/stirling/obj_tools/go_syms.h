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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/stirling/obj_tools/elf_reader.h"

namespace px {
namespace stirling {
namespace obj_tools {

// Returns true if the executable is built by Golang.
bool IsGoExecutable(ElfReader* elf_reader);

struct Module {
  std::string path;
  std::string version;
  std::string sum;
  std::unique_ptr<Module> replace = nullptr;
};

struct BuildInfo {
  std::string path;
  Module main;
  std::vector<Module> deps;
  std::vector<std::pair<std::string, std::string>> settings;
};

StatusOr<BuildInfo> ReadModInfo(const std::string& mod);
// Returns the build version and buildinfo of a Golang executable. The executable is read through
// the input elf_reader.
StatusOr<std::pair<std::string, BuildInfo>> ReadGoBuildInfo(ElfReader* elf_reader);

// Returns the build version by reading the runtime.buildVersion symbol from a Golang executable.
// This is a fallback method for older Go binaries (Go 1.11 and earlier) that don't have the 
// .go.buildinfo section. The version string has the "go" prefix stripped (e.g., "1.11.13").
// Note: This function does not work correctly with 32-bit binaries due to gostring structure size differences.
StatusOr<std::pair<std::string, BuildInfo>> ReadBuildVersion(ElfReader* elf_reader);

// Describes a Golang type that implement an interface.
struct IntfImplTypeInfo {
  // The name of the type that implements a given interface.
  std::string type_name;

  // The address of the symbol that records this information.
  uint64_t address = 0;

  std::string ToString() const {
    return absl::Substitute("type_name=$0 address=$1", type_name, address);
  }
};

// Returns a map of all interfaces, and types that implement that interface in a go binary.
StatusOr<absl::flat_hash_map<std::string, std::vector<IntfImplTypeInfo>>> ExtractGolangInterfaces(
    ElfReader* elf_reader);

void PrintTo(const std::vector<IntfImplTypeInfo>& infos, std::ostream* os);

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
