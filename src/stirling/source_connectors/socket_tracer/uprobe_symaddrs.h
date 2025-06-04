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

#include <map>
#include <memory>
#include <string>

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/obj_tools/go_syms.h"
#include "src/stirling/obj_tools/raw_fptr_manager.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"
#include "src/stirling/utils/detect_application.h"

DECLARE_bool(openssl_force_raw_fptrs);
DECLARE_bool(openssl_raw_fptrs_enabled);

namespace px {
namespace stirling {

constexpr std::string_view kLibNettyTcnativePrefix = "libnetty_tcnative_linux_x86";

using StructOffsetMap =
    std::map<std::string, std::map<std::string, std::map<std::string, int32_t>>>;
using FunctionArgMap =
    std::map<std::string,
             std::map<std::string, std::map<std::string, std::unique_ptr<obj_tools::VarLocation>>>>;

class GoOffsetLocator {
 public:
  // TODO(ddelnano): Remove this constructor once the scaffolding to populate StructOffsetMap
  // and FunctionArgMap is available.
  GoOffsetLocator(obj_tools::DwarfReader* dwarf_reader, const obj_tools::BuildInfo& build_info,
                  const std::string& go_version)
      : GoOffsetLocator(dwarf_reader, StructOffsetMap{}, FunctionArgMap{}, build_info, go_version) {
  }

  GoOffsetLocator(obj_tools::DwarfReader* dwarf_reader, const StructOffsetMap& struct_offsets,
                  const FunctionArgMap& function_args, const obj_tools::BuildInfo& build_info,
                  const std::string& go_version)
      : dwarf_reader_(dwarf_reader),
        struct_offsets_(struct_offsets),
        function_args_(function_args),
        go_version_(go_version) {
    PopulateModuleVersions(build_info);
  }

  StatusOr<std::map<std::string, obj_tools::ArgInfo>> GetFunctionArgInfo(
      std::string_view function_symbol_name) {
    if (dwarf_reader_ != nullptr) {
      return dwarf_reader_->GetFunctionArgInfo(function_symbol_name);
    }
    return GetFunctionArgInfoFromOffsets(function_symbol_name);
  }

  StatusOr<obj_tools::VarLocation> GetArgumentLocation(std::string_view /*function_symbol_name*/,
                                                       std::string_view /*arg_name*/) {
    return error::Internal(
        "GetArgumentLocation is not implemented for GoOffsetLocator. Use GetFunctionArgInfo "
        "instead.");
  }

  StatusOr<uint64_t> GetStructMemberOffset(std::string_view struct_name,
                                           std::string_view member_name) {
    if (dwarf_reader_ != nullptr) {
      return dwarf_reader_->GetStructMemberOffset(struct_name, member_name);
    }
    return GetStructMemberOffsetFromOffsets(struct_name, member_name);
  }

 private:
  StatusOr<std::map<std::string, obj_tools::ArgInfo>> GetFunctionArgInfoFromOffsets(
      std::string_view function_symbol_name) {
    auto fn_map = function_args_.find(std::string(function_symbol_name));
    if (fn_map == function_args_.end()) {
      return error::Internal("Unable to find function location for $0", function_symbol_name);
    }
    std::map<std::string, obj_tools::ArgInfo> result;
    for (const auto& [arg_name, version_info_map] : fn_map->second) {
      std::string version_key = go_version_;
      auto version_map = version_info_map.find(version_key);
      if (version_map == version_info_map.end()) {
        return error::Internal("Unable to find function location for arg=$0 version=$1", arg_name,
                               version_key);
      }
      auto var_loc_ptr = version_map->second.get();
      if (var_loc_ptr == nullptr) {
        return error::Internal("Function location for arg=$0 version=$1 is missing", arg_name,
                               version_key);
      }
      result.insert({arg_name, obj_tools::ArgInfo{obj_tools::TypeInfo{}, *var_loc_ptr}});
    }
    return result;
  }

  StatusOr<uint64_t> GetStructMemberOffsetFromOffsets(std::string_view struct_name,
                                                      std::string_view member_name) {
    auto struct_map = struct_offsets_.find(std::string(struct_name));
    if (struct_map == struct_offsets_.end()) {
      return error::Internal("Unable to find offsets for struct=$0", struct_name);
    }
    auto member_map = struct_map->second.find(std::string(member_name));
    if (member_map == struct_map->second.end()) {
      return error::Internal("Unable to find offsets for struct member=$0.$1", struct_name,
                             member_name);
    }

    std::string version_key = go_version_;
    auto version_map = member_map->second.find(version_key);
    if (version_map == member_map->second.end()) {
      return error::Internal("Unable to find offsets for struct member=$0.$1 for version $2",
                             struct_name, member_name, version_key);
    }
    return version_map->second;
  }

  void PopulateModuleVersions(const obj_tools::BuildInfo& build_info) {
    for (const auto& dep : build_info.deps) {
      // Find the related dependencies and strip the "v" prefix
      if (dep.path == "golang.org/x/net") {
        golang_x_net_version_ = dep.version.substr(1);
      } else if (dep.path == "google.golang.org/grpc") {
        google_golang_grpc_version_ = dep.version.substr(1);
      }
    }
    VLOG(1) << "golang.org/x/net module version: " << golang_x_net_version_;
    VLOG(1) << "google.golang.org/grpc module version: " << google_golang_grpc_version_;
  }

  obj_tools::DwarfReader* dwarf_reader_;

  const StructOffsetMap& struct_offsets_;
  const FunctionArgMap& function_args_;

  const std::string& go_version_;

  std::string golang_x_net_version_;
  std::string google_golang_grpc_version_;
};

/**
 * Uses ELF and DWARF information to return the locations of all relevant symbols for general Go
 * uprobe deployment.
 */
StatusOr<struct go_common_symaddrs_t> GoCommonSymAddrs(obj_tools::ElfReader* elf_reader,
                                                       GoOffsetLocator* offset_locator);

/**
 * Uses ELF and DWARF information to return the locations of all relevant symbols for Go HTTP2
 * uprobe deployment.
 */
StatusOr<struct go_http2_symaddrs_t> GoHTTP2SymAddrs(obj_tools::ElfReader* elf_reader,
                                                     GoOffsetLocator* offset_locator);

/**
 * Uses ELF and DWARF information to return the locations of all relevant symbols for Go TLS
 * uprobe deployment.
 */
StatusOr<struct go_tls_symaddrs_t> GoTLSSymAddrs(obj_tools::ElfReader* elf_reader,
                                                 GoOffsetLocator* offset_locator);

/**
 * Detects the version of OpenSSL to return the locations of all relevant symbols for OpenSSL uprobe
 * deployment.
 */
StatusOr<struct openssl_symaddrs_t> OpenSSLSymAddrs(obj_tools::RawFptrManager* fptrManager,
                                                    const std::filesystem::path& openssl_lib,
                                                    uint32_t pid);

/**
 * Returns the corresponding symbol offsets of the input Nodejs executable.
 * If the executable includes the dwarf info, the offsets are read directly from that.
 * Otherwise, consult the provided version (if available) to lookup for hard-coded symbol offsets.
 */
StatusOr<struct node_tlswrap_symaddrs_t> NodeTLSWrapSymAddrs(const std::filesystem::path& node_exe,
                                                             const SemVer& ver);

px::Status PopulateGoTLSDebugSymbols(obj_tools::ElfReader* elf_reader,
                                     GoOffsetLocator* offset_locator,
                                     struct go_tls_symaddrs_t* symaddrs);

}  // namespace stirling
}  // namespace px
