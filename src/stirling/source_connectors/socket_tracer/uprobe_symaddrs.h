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

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/obj_tools/raw_fptr_manager.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"
#include "src/stirling/utils/detect_application.h"

DECLARE_bool(openssl_force_raw_fptrs);
DECLARE_bool(openssl_raw_fptrs_enabled);

namespace px {
namespace stirling {

constexpr std::string_view kLibNettyTcnativePrefix = "libnetty_tcnative_linux_x86";

/**
 * Uses ELF and DWARF information to return the locations of all relevant symbols for general Go
 * uprobe deployment.
 */
StatusOr<struct go_common_symaddrs_t> GoCommonSymAddrs(obj_tools::ElfReader* elf_reader,
                                                       obj_tools::DwarfReader* dwarf_reader);

/**
 * Uses ELF and DWARF information to return the locations of all relevant symbols for Go HTTP2
 * uprobe deployment.
 */
StatusOr<struct go_http2_symaddrs_t> GoHTTP2SymAddrs(obj_tools::ElfReader* elf_reader,
                                                     obj_tools::DwarfReader* dwarf_reader);

/**
 * Uses ELF and DWARF information to return the locations of all relevant symbols for Go TLS
 * uprobe deployment.
 */
StatusOr<struct go_tls_symaddrs_t> GoTLSSymAddrs(obj_tools::ElfReader* elf_reader,
                                                 obj_tools::DwarfReader* dwarf_reader);

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

}  // namespace stirling
}  // namespace px
