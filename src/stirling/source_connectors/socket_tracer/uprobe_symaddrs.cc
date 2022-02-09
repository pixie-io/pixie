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

#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"
#include <dlfcn.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/utils/detect_application.h"

using ::px::stirling::obj_tools::DwarfReader;
using ::px::stirling::obj_tools::ElfReader;

namespace px {
namespace stirling {

namespace {
// A golang array consists of a pointer, a length and a capacity.
// These could come from DWARF information, but are hard-coded,
// since an array is a pretty stable type.
constexpr int kGoArrayPtrOffset = 0;
constexpr int kGoArrayLenOffset = 8;
}  // namespace

//-----------------------------------------------------------------------------
// Symbol Population Functions
//-----------------------------------------------------------------------------

// The functions in this section populate structs that contain locations of necessary symbols,
// which are then passed through a BPF map to the uprobe.
// For example, locations of required struct members are communicated through this fasion.

// The following is a helper macro that is useful during debug.
// By changing VLOG to LOG, all assignments that use this macro are logged.
// Primarily used to record the symbol address and offset assignments.
#define LOG_ASSIGN(var, val)         \
  {                                  \
    var = val;                       \
    VLOG(1) << #var << " = " << var; \
  }

#define LOG_ASSIGN_STATUSOR(var, val) LOG_ASSIGN(var, val.ValueOr(-1))
#define LOG_ASSIGN_OPTIONAL(var, val) LOG_ASSIGN(var, val.value_or(-1))

namespace {

location_t GetArgOffset(const std::map<std::string, obj_tools::ArgInfo>& fn_args_map,
                        const std::string& arg) {
  // The information from DWARF assumes SP is 8 bytes larger than the SP
  // we get from BPF code, so add the correction factor here.
  constexpr int32_t kSPOffset = 8;

  location_t location;

  auto it = fn_args_map.find(arg);
  if (it == fn_args_map.end()) {
    location.type = kLocationTypeInvalid;
    location.offset = -1;
    return location;
  }

  switch (it->second.location.loc_type) {
    case obj_tools::LocationType::kStack:
      location.type = kLocationTypeStack;
      location.offset = it->second.location.offset + kSPOffset;
      return location;
    case obj_tools::LocationType::kRegister:
      location.type = kLocationTypeRegisters;
      location.offset = it->second.location.offset;
      return location;
    default:
      location.type = kLocationTypeInvalid;
      location.offset = -1;
  }

  return location;
}

StatusOr<std::string> InferHTTP2SymAddrVendorPrefix(ElfReader* elf_reader) {
  // We now want to infer the vendor prefix directory. Use the list of symbols below as samples to
  // help infer. The iteration will stop after the first inference.
  const std::vector<std::string_view> kSampleSymbols = {
      "google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders",
      "golang.org/x/net/http2.(*Framer).WriteHeaders"};

  std::string vendor_prefix;
  for (std::string_view s : kSampleSymbols) {
    PL_ASSIGN_OR(std::vector<ElfReader::SymbolInfo> symbol_matches,
                 elf_reader->ListFuncSymbols(s, obj_tools::SymbolMatchType::kSuffix), continue);
    if (symbol_matches.size() > 1) {
      VLOG(1) << absl::Substitute(
          "Found multiple symbol matches for $0. Cannot infer vendor prefix.", s);
      continue;
    }
    if (!symbol_matches.empty()) {
      const auto& name = symbol_matches.front().name;
      DCHECK_GE(name.size(), s.size());
      vendor_prefix = name.substr(0, name.size() - s.size());
      break;
    }
  }

  VLOG_IF(1, !vendor_prefix.empty())
      << absl::Substitute("Inferred vendor prefix: $0", vendor_prefix);
  return vendor_prefix;
}

Status PopulateCommonTypeAddrs(ElfReader* elf_reader, std::string_view vendor_prefix,
                               struct go_common_symaddrs_t* symaddrs) {
  // Note: we only return error if a *mandatory* symbol is missing. Only TCPConn is mandatory.
  // Without TCPConn, the uprobe cannot resolve the FD, and becomes pointless.

  LOG_ASSIGN_OPTIONAL(symaddrs->internal_syscallConn,
                      elf_reader->SymbolAddress(absl::StrCat(
                          "go.itab.*", vendor_prefix,
                          "google.golang.org/grpc/credentials/internal.syscallConn,net.Conn")));
  LOG_ASSIGN_OPTIONAL(symaddrs->tls_Conn,
                      elf_reader->SymbolAddress("go.itab.*crypto/tls.Conn,net.Conn"));
  LOG_ASSIGN_OPTIONAL(symaddrs->net_TCPConn,
                      elf_reader->SymbolAddress("go.itab.*net.TCPConn,net.Conn"));

  // TCPConn is mandatory by the HTTP2 uprobes probe, so bail if it is not found (-1).
  // It should be the last layer of nested interface, and contains the FD.
  // The other conns can be invalid, and will simply be ignored.
  if (symaddrs->net_TCPConn == -1) {
    return error::Internal("TCPConn not found");
  }

  return Status::OK();
}

Status PopulateCommonDebugSymbols(DwarfReader* dwarf_reader, std::string_view vendor_prefix,
                                  struct go_common_symaddrs_t* symaddrs) {
#define VENDOR_SYMBOL(symbol) absl::StrCat(vendor_prefix, symbol)

  LOG_ASSIGN_STATUSOR(symaddrs->FD_Sysfd_offset,
                      dwarf_reader->GetStructMemberOffset("internal/poll.FD", "Sysfd"));
  LOG_ASSIGN_STATUSOR(symaddrs->tlsConn_conn_offset,
                      dwarf_reader->GetStructMemberOffset("crypto/tls.Conn", "conn"));
  LOG_ASSIGN_STATUSOR(
      symaddrs->syscallConn_conn_offset,
      dwarf_reader->GetStructMemberOffset(
          VENDOR_SYMBOL("google.golang.org/grpc/credentials/internal.syscallConn"), "conn"));

#undef VENDOR_SYMBOL

  // List mandatory symaddrs here (symaddrs without which all probes become useless).
  // Returning an error will prevent the probes from deploying.
  if (symaddrs->FD_Sysfd_offset == -1) {
    return error::Internal("FD_Sysfd_offset not found");
  }

  return Status::OK();
}

Status PopulateHTTP2TypeAddrs(ElfReader* elf_reader, std::string_view vendor_prefix,
                              struct go_http2_symaddrs_t* symaddrs) {
  // Note: we only return error if a *mandatory* symbol is missing. Only TCPConn is mandatory.
  // Without TCPConn, the uprobe cannot resolve the FD, and becomes pointless.

  LOG_ASSIGN_OPTIONAL(symaddrs->http_http2bufferedWriter,
                      elf_reader->SymbolAddress("go.itab.*net/http.http2bufferedWriter,io.Writer"));
  LOG_ASSIGN_OPTIONAL(symaddrs->transport_bufWriter,
                      elf_reader->SymbolAddress(absl::StrCat(
                          "go.itab.*", vendor_prefix,
                          "google.golang.org/grpc/internal/transport.bufWriter,io.Writer")));

  return Status::OK();
}

Status PopulateHTTP2DebugSymbols(DwarfReader* dwarf_reader, std::string_view vendor_prefix,
                                 struct go_http2_symaddrs_t* symaddrs) {
  // Note: we only return error if a *mandatory* symbol is missing. Currently none are mandatory,
  // because these multiple probes for multiple HTTP2/GRPC libraries. Even if a symbol for one
  // is missing it doesn't mean the other library's probes should not be deployed.

#define VENDOR_SYMBOL(symbol) absl::StrCat(vendor_prefix, symbol)

  // clang-format off
  LOG_ASSIGN_STATUSOR(symaddrs->HeaderField_Name_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2/hpack.HeaderField"),
                            "Name"));
  LOG_ASSIGN_STATUSOR(symaddrs->HeaderField_Value_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2/hpack.HeaderField"),
                            "Value"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2Server_conn_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.http2Server"),
                            "conn"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2Client_conn_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.http2Client"),
                            "conn"));
  LOG_ASSIGN_STATUSOR(symaddrs->loopyWriter_framer_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.loopyWriter"),
                            "framer"));
  LOG_ASSIGN_STATUSOR(symaddrs->Framer_w_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2.Framer"),
                            "w"));
  LOG_ASSIGN_STATUSOR(symaddrs->MetaHeadersFrame_HeadersFrame_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2.MetaHeadersFrame"),
                            "HeadersFrame"));
  LOG_ASSIGN_STATUSOR(symaddrs->MetaHeadersFrame_Fields_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2.MetaHeadersFrame"),
                            "Fields"));
  LOG_ASSIGN_STATUSOR(symaddrs->HeadersFrame_FrameHeader_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2.HeadersFrame"),
                            "FrameHeader"));
  LOG_ASSIGN_STATUSOR(symaddrs->FrameHeader_Type_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2.FrameHeader"),
                            "Type"));
  LOG_ASSIGN_STATUSOR(symaddrs->FrameHeader_Flags_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2.FrameHeader"),
                            "Flags"));
  LOG_ASSIGN_STATUSOR(symaddrs->FrameHeader_StreamID_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2.FrameHeader"),
                            "StreamID"));
  LOG_ASSIGN_STATUSOR(symaddrs->DataFrame_data_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("golang.org/x/net/http2.DataFrame"),
                            "data"));
  LOG_ASSIGN_STATUSOR(symaddrs->bufWriter_conn_offset,
                    dwarf_reader->GetStructMemberOffset(
                            VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.bufWriter"),
                            "conn"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2serverConn_conn_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2serverConn",
                            "conn"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2serverConn_hpackEncoder_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2serverConn",
                            "hpackEncoder"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2HeadersFrame_http2FrameHeader_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2HeadersFrame",
                            "http2FrameHeader"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2FrameHeader_Type_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2FrameHeader",
                            "Type"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2FrameHeader_Flags_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2FrameHeader",
                            "Flags"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2FrameHeader_StreamID_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2FrameHeader",
                            "StreamID"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2DataFrame_data_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2DataFrame",
                            "data"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2writeResHeaders_streamID_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2writeResHeaders",
                            "streamID"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2writeResHeaders_endStream_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2writeResHeaders",
                            "endStream"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2MetaHeadersFrame_http2HeadersFrame_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2MetaHeadersFrame",
                            "http2HeadersFrame"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2MetaHeadersFrame_Fields_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2MetaHeadersFrame",
                            "Fields"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2Framer_w_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2Framer",
                            "w"));
  LOG_ASSIGN_STATUSOR(symaddrs->http2bufferedWriter_w_offset,
                    dwarf_reader->GetStructMemberOffset(
                            "net/http.http2bufferedWriter",
                            "w"));
  // clang-format on

  const std::map<std::string, obj_tools::ArgInfo> kEmptyMap;

  // Arguments of net/http.(*http2Framer).WriteDataPadded.
  {
    std::string_view fn = "net/http.(*http2Framer).WriteDataPadded";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->http2Framer_WriteDataPadded_f_loc, GetArgOffset(args_map, "f"));
    LOG_ASSIGN(symaddrs->http2Framer_WriteDataPadded_streamID_loc,
               GetArgOffset(args_map, "streamID"));
    LOG_ASSIGN(symaddrs->http2Framer_WriteDataPadded_endStream_loc,
               GetArgOffset(args_map, "endStream"));

    LOG_ASSIGN(symaddrs->http2Framer_WriteDataPadded_data_ptr_loc, GetArgOffset(args_map, "data"));
    symaddrs->http2Framer_WriteDataPadded_data_ptr_loc.offset += kGoArrayPtrOffset;

    LOG_ASSIGN(symaddrs->http2Framer_WriteDataPadded_data_len_loc, GetArgOffset(args_map, "data"));
    symaddrs->http2Framer_WriteDataPadded_data_len_loc.offset += kGoArrayLenOffset;
  }

  // Arguments of golang.org/x/net/http2.(*Framer).WriteDataPadded.
  {
    std::string fn = VENDOR_SYMBOL("golang.org/x/net/http2.(*Framer).WriteDataPadded");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->http2_WriteDataPadded_f_loc, GetArgOffset(args_map, "f"));
    LOG_ASSIGN(symaddrs->http2_WriteDataPadded_streamID_loc, GetArgOffset(args_map, "streamID"));
    LOG_ASSIGN(symaddrs->http2_WriteDataPadded_endStream_loc, GetArgOffset(args_map, "endStream"));

    LOG_ASSIGN(symaddrs->http2_WriteDataPadded_data_ptr_loc, GetArgOffset(args_map, "data"));
    symaddrs->http2_WriteDataPadded_data_ptr_loc.offset += kGoArrayPtrOffset;

    LOG_ASSIGN(symaddrs->http2_WriteDataPadded_data_len_loc, GetArgOffset(args_map, "data"));
    symaddrs->http2_WriteDataPadded_data_len_loc.offset += kGoArrayLenOffset;
  }

  // Arguments of net/http.(*http2Framer).checkFrameOrder.
  {
    std::string_view fn = "net/http.(*http2Framer).checkFrameOrder";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->http2Framer_checkFrameOrder_fr_loc, GetArgOffset(args_map, "fr"));
    LOG_ASSIGN(symaddrs->http2Framer_checkFrameOrder_f_loc, GetArgOffset(args_map, "f"));
  }

  // Arguments of golang.org/x/net/http2.(*Framer).checkFrameOrder.
  {
    std::string fn = VENDOR_SYMBOL("golang.org/x/net/http2.(*Framer).checkFrameOrder");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->http2_checkFrameOrder_fr_loc, GetArgOffset(args_map, "fr"));
    LOG_ASSIGN(symaddrs->http2_checkFrameOrder_f_loc, GetArgOffset(args_map, "f"));
  }

  // Arguments of net/http.(*http2writeResHeaders).writeFrame.
  {
    std::string_view fn = "net/http.(*http2writeResHeaders).writeFrame";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->writeFrame_w_loc, GetArgOffset(args_map, "w"));
    LOG_ASSIGN(symaddrs->writeFrame_ctx_loc, GetArgOffset(args_map, "ctx"));
  }

  // Arguments of golang.org/x/net/http2/hpack.(*Encoder).WriteField.
  {
    std::string fn = VENDOR_SYMBOL("golang.org/x/net/http2/hpack.(*Encoder).WriteField");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->WriteField_e_loc, GetArgOffset(args_map, "e"));
    LOG_ASSIGN(symaddrs->WriteField_f_name_loc, GetArgOffset(args_map, "f"));
    symaddrs->WriteField_f_name_loc.offset += 0;

    LOG_ASSIGN(symaddrs->WriteField_f_value_loc, GetArgOffset(args_map, "f"));
    symaddrs->WriteField_f_value_loc.offset += 16;
  }

  // Arguments of net/http.(*http2serverConn).processHeaders.
  {
    std::string_view fn = "net/http.(*http2serverConn).processHeaders";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->processHeaders_sc_loc, GetArgOffset(args_map, "sc"));
    LOG_ASSIGN(symaddrs->processHeaders_f_loc, GetArgOffset(args_map, "f"));
  }

  // Arguments of google.golang.org/grpc/internal/transport.(*http2Server).operateHeaders.
  {
    std::string fn =
        VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.(*http2Server).operateHeaders");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->http2Server_operateHeaders_t_loc, GetArgOffset(args_map, "t"));
    LOG_ASSIGN(symaddrs->http2Server_operateHeaders_frame_loc, GetArgOffset(args_map, "frame"));
  }

  // Arguments of google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders.
  {
    std::string fn =
        VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->http2Client_operateHeaders_t_loc, GetArgOffset(args_map, "t"));
    LOG_ASSIGN(symaddrs->http2Client_operateHeaders_frame_loc, GetArgOffset(args_map, "frame"));
  }

  // Arguments of google.golang.org/grpc/internal/transport.(*loopyWriter).writeHeader.
  {
    std::string fn =
        VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.(*loopyWriter).writeHeader");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->writeHeader_l_loc, GetArgOffset(args_map, "l"));
    LOG_ASSIGN(symaddrs->writeHeader_streamID_loc, GetArgOffset(args_map, "streamID"));
    LOG_ASSIGN(symaddrs->writeHeader_endStream_loc, GetArgOffset(args_map, "endStream"));

    LOG_ASSIGN(symaddrs->writeHeader_hf_ptr_loc, GetArgOffset(args_map, "hf"));
    symaddrs->writeHeader_hf_ptr_loc.offset += kGoArrayPtrOffset;

    LOG_ASSIGN(symaddrs->writeHeader_hf_len_loc, GetArgOffset(args_map, "hf"));
    symaddrs->writeHeader_hf_len_loc.offset += kGoArrayLenOffset;
  }

#undef VENDOR_SYMBOL

  return Status::OK();
}

Status PopulateGoTLSDebugSymbols(DwarfReader* dwarf_reader, struct go_tls_symaddrs_t* symaddrs) {
  const std::map<std::string, obj_tools::ArgInfo> kEmptyMap;

  // Arguments of crypto/tls.(*Conn).Write.
  {
    std::string_view fn = "crypto/tls.(*Conn).Write";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->Write_c_loc, GetArgOffset(args_map, "c"));
    LOG_ASSIGN(symaddrs->Write_b_loc, GetArgOffset(args_map, "b"));
    LOG_ASSIGN(symaddrs->Write_retval0_loc, GetArgOffset(args_map, "~r1"));
    LOG_ASSIGN(symaddrs->Write_retval1_loc, GetArgOffset(args_map, "~r2"));
  }

  // Arguments of crypto/tls.(*Conn).Read.
  {
    std::string fn = "crypto/tls.(*Conn).Read";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    LOG_ASSIGN(symaddrs->Read_c_loc, GetArgOffset(args_map, "c"));
    LOG_ASSIGN(symaddrs->Read_b_loc, GetArgOffset(args_map, "b"));
    LOG_ASSIGN(symaddrs->Read_retval0_loc, GetArgOffset(args_map, "~r1"));
    LOG_ASSIGN(symaddrs->Read_retval1_loc, GetArgOffset(args_map, "~r2"));
  }

  // List mandatory symaddrs here (symaddrs without which all probes become useless).
  // Returning an error will prevent the probes from deploying.
  if (symaddrs->Write_b_loc.type == kLocationTypeInvalid ||
      symaddrs->Read_b_loc.type == kLocationTypeInvalid) {
    return error::Internal("Go TLS Read/Write arguments not found.");
  }

  return Status::OK();
}

}  // namespace

StatusOr<struct go_common_symaddrs_t> GoCommonSymAddrs(ElfReader* elf_reader,
                                                       DwarfReader* dwarf_reader) {
  struct go_common_symaddrs_t symaddrs;

  PL_ASSIGN_OR_RETURN(std::string vendor_prefix, InferHTTP2SymAddrVendorPrefix(elf_reader));

  PL_RETURN_IF_ERROR(PopulateCommonTypeAddrs(elf_reader, vendor_prefix, &symaddrs));
  PL_RETURN_IF_ERROR(PopulateCommonDebugSymbols(dwarf_reader, vendor_prefix, &symaddrs));

  return symaddrs;
}

StatusOr<struct go_http2_symaddrs_t> GoHTTP2SymAddrs(ElfReader* elf_reader,
                                                     DwarfReader* dwarf_reader) {
  struct go_http2_symaddrs_t symaddrs;

  PL_ASSIGN_OR_RETURN(std::string vendor_prefix, InferHTTP2SymAddrVendorPrefix(elf_reader));

  PL_RETURN_IF_ERROR(PopulateHTTP2TypeAddrs(elf_reader, vendor_prefix, &symaddrs));
  PL_RETURN_IF_ERROR(PopulateHTTP2DebugSymbols(dwarf_reader, vendor_prefix, &symaddrs));

  return symaddrs;
}

StatusOr<struct go_tls_symaddrs_t> GoTLSSymAddrs(ElfReader* elf_reader, DwarfReader* dwarf_reader) {
  struct go_tls_symaddrs_t symaddrs;

  PL_UNUSED(elf_reader);

  PL_RETURN_IF_ERROR(PopulateGoTLSDebugSymbols(dwarf_reader, &symaddrs));

  return symaddrs;
}

namespace {

// Returns a function pointer from a dlopen handle.
template <class T>
StatusOr<T*> DLSymbolToFptr(void* handle, const std::string& symbol_name) {
  // The templated form compares nicely to c-style function pointer typedefs.
  // Example usage:
  // auto myFunction = DLSymbolToFptr<int (FooQux &, const Baz *)>( h, "somesymbol");
  T* fptr = reinterpret_cast<T*>(dlsym(handle, symbol_name.c_str()));

  const char* dlsym_error = dlerror();
  if (dlsym_error) {
    return error::Internal("Failed to find symbol: $0, $1", symbol_name, dlsym_error);
  }

  return fptr;
}

StatusOr<uint64_t> GetOpenSSLVersionNumUsingDLOpen(const std::filesystem::path& lib_openssl_path) {
  if (!fs::Exists(lib_openssl_path)) {
    return error::Internal("Path to OpenSSL so is not valid: $0", lib_openssl_path.string());
  }

  void* h = dlopen(lib_openssl_path.c_str(), RTLD_LAZY);

  if (h == nullptr) {
    return error::Internal("Failed to dlopen OpenSSL so file: $0, $1", lib_openssl_path.string(),
                           dlerror());
  }
  DEFER(dlclose(h));

  const std::string version_num_symbol = "OpenSSL_version_num";

  // NOLINTNEXTLINE(runtime/int): 'unsigned long' is from upstream, match that here (vs. uint64_t)
  PL_ASSIGN_OR_RETURN(auto version_num_f, DLSymbolToFptr<unsigned long()>(h, version_num_symbol));

  const uint64_t version_num = version_num_f();
  return version_num;
}

// Returns the "fix" version number for OpenSSL.
StatusOr<uint32_t> OpenSSLFixSubversionNum(const std::filesystem::path& lib_openssl_path) {
  // Current use case:
  // switch for the correct number of bytes offset for the socket fd.
  //
  // Basic version number format: "major.minor.fix".
  // In more detail:
  // MNNFFPPS: major minor fix patch status
  // From https://www.openssl.org/docs/man1.1.1/man3/OPENSSL_VERSION_NUMBER.html.
  union open_ssl_version_num_t {
    struct __attribute__((packed)) {
      uint32_t status : 4;
      uint32_t patch : 8;
      uint32_t fix : 8;
      uint32_t minor : 8;
      uint32_t major : 8;
      uint32_t unused : 64 - (4 + 4 * 8);
    };  // NOLINT(readability/braces) False claim that ';' is unnecessary.
    uint64_t packed;
  };
  open_ssl_version_num_t version_num;

  PL_ASSIGN_OR_RETURN(version_num.packed, GetOpenSSLVersionNumUsingDLOpen(lib_openssl_path));

  const uint32_t major = version_num.major;
  const uint32_t minor = version_num.minor;
  const uint32_t fix = version_num.fix;

  VLOG(1) << absl::StrFormat("Found OpenSSL version: 0x%016lx (%d.%d.%d:%x.%x), %s",
                             version_num.packed, major, minor, fix, version_num.patch,
                             version_num.status, lib_openssl_path.string());

  constexpr uint32_t min_fix_version = 0;
  constexpr uint32_t max_fix_version = 1;

  if (major != 1) {
    return error::Internal("Unsupported OpenSSL major version: $0.$1.$2", major, minor, fix);
  }
  if (minor != 1) {
    return error::Internal("Unsupported OpenSSL minor version: $0.$1.$2", major, minor, fix);
  }
  if (fix != std::clamp(fix, min_fix_version, max_fix_version)) {
    return error::Internal("Unsupported OpenSSL fix version: $0.$1.$2", major, minor, fix);
  }
  return fix;
}

}  // namespace

StatusOr<struct openssl_symaddrs_t> OpenSSLSymAddrs(const std::filesystem::path& openssl_lib) {
  // Some useful links, for different OpenSSL versions:
  // 1.1.0a:
  // https://github.com/openssl/openssl/blob/ac2c44c6289f9716de4c4beeb284a818eacde517/<filename>
  // 1.1.0l:
  // https://github.com/openssl/openssl/blob/7ea5bd2b52d0e81eaef3d109b3b12545306f201c/<filename>
  // 1.1.1a:
  // https://github.com/openssl/openssl/blob/d1c28d791a7391a8dc101713cd8646df96491d03/<filename>
  // 1.1.1e:
  // https://github.com/openssl/openssl/blob/a61eba4814fb748ad67e90e81c005ffb09b67d3d/<filename>

  // Offset of rbio in struct ssl_st.
  // Struct is defined in ssl/ssl_local.h, ssl/ssl_locl.h, ssl/ssl_lcl.h, depending on the version.
  // Verified to be valid for following versions:
  //  - 1.1.0a to 1.1.0k
  //  - 1.1.1a to 1.1.1e
  constexpr int32_t kSSL_RBIO_offset = 0x10;

  // Offset of num in struct bio_st.
  // Struct is defined in crypto/bio/bio_lcl.h, crypto/bio/bio_local.h depending on the version.
  //  - In 1.1.1a to 1.1.1e, the offset appears to be 0x30
  //  - In 1.1.0, the value appears to be 0x28.
  constexpr int32_t kOpenSSL_1_1_0_RBIO_num_offset = 0x28;
  constexpr int32_t kOpenSSL_1_1_1_RBIO_num_offset = 0x30;

  struct openssl_symaddrs_t symaddrs;
  symaddrs.SSL_rbio_offset = kSSL_RBIO_offset;

  PL_ASSIGN_OR_RETURN(uint32_t openssl_fix_sub_version, OpenSSLFixSubversionNum(openssl_lib));
  switch (openssl_fix_sub_version) {
    case 0:
      symaddrs.RBIO_num_offset = kOpenSSL_1_1_0_RBIO_num_offset;
      break;
    case 1:
      symaddrs.RBIO_num_offset = kOpenSSL_1_1_1_RBIO_num_offset;
      break;
    default:
      // Supported versions are checked in function OpenSSLFixSubversionNum(),
      // should not fall through to here, ever.
      DCHECK(false);
      return error::Internal("Unsupported openssl_fix_sub_version: $0", openssl_fix_sub_version);
  }

  // Using GDB to confirm member offsets on OpenSSL 1.1.1:
  // (gdb) p s
  // $18 = (SSL *) 0x55ea646953c0
  // (gdb) p &s.rbio
  // $22 = (BIO **) 0x55ea646953d0
  // (gdb) p s.rbio
  // $23 = (BIO *) 0x55ea64698b10
  // (gdb) p &s.rbio.num
  // $24 = (int *) 0x55ea64698b40
  // (gdb) p s.rbio.num
  // $25 = 3
  // (gdb) p &s.wbio
  // $26 = (BIO **) 0x55ea646953d8
  // (gdb) p s.wbio
  // $27 = (BIO *) 0x55ea64698b10
  // (gdb) p &s.wbio.num
  // $28 = (int *) 0x55ea64698b40
  // (gdb) p s.wbio.num
  return symaddrs;
}

// Instructions of get symbol offsets for nodejs.
//   git clone nodejs repo.
//   git checkout v<version>  # Checkout the tagged release
//   ./configure --debug && make -j8  # build the debug version
//   sudo out/Debug/node src/stirling/.../containers/ssl/https_server.js
//   Launch stirling_wrapper, log the output of NodeTLSWrapSymAddrsFromDwarf() from inside
//   UProbeManager::UpdateNodeTLSWrapSymAddrs().
constexpr struct node_tlswrap_symaddrs_t kNodeSymaddrsV12_3_1 = {
    .TLSWrap_StreamListener_offset = 0x0130,
    .StreamListener_stream_offset = 0x08,
    .StreamBase_StreamResource_offset = 0x00,
    .LibuvStreamWrap_StreamBase_offset = 0x50,
    .LibuvStreamWrap_stream_offset = 0x90,
    .uv_stream_s_io_watcher_offset = 0x88,
    .uv__io_s_fd_offset = 0x30,
};

constexpr struct node_tlswrap_symaddrs_t kNodeSymaddrsV12_16_2 = {
    .TLSWrap_StreamListener_offset = 0x138,
    .StreamListener_stream_offset = 0x08,
    .StreamBase_StreamResource_offset = 0x00,
    .LibuvStreamWrap_StreamBase_offset = 0x58,
    .LibuvStreamWrap_stream_offset = 0x98,
    .uv_stream_s_io_watcher_offset = 0x88,
    .uv__io_s_fd_offset = 0x30,
};

constexpr struct node_tlswrap_symaddrs_t kNodeSymaddrsV13_0_0 = {
    .TLSWrap_StreamListener_offset = 0x130,
    .StreamListener_stream_offset = 0x8,
    .StreamBase_StreamResource_offset = 0x00,
    .LibuvStreamWrap_StreamBase_offset = 0x50,
    .LibuvStreamWrap_stream_offset = 0x90,
    .uv_stream_s_io_watcher_offset = 0x88,
    .uv__io_s_fd_offset = 0x30,
};

constexpr struct node_tlswrap_symaddrs_t kNodeSymaddrsV13_2_0 = {
    .TLSWrap_StreamListener_offset = 0x138,
    .StreamListener_stream_offset = 0x08,
    .StreamBase_StreamResource_offset = 0x00,
    .LibuvStreamWrap_StreamBase_offset = 0x58,
    .LibuvStreamWrap_stream_offset = 0x98,
    .uv_stream_s_io_watcher_offset = 0x88,
    .uv__io_s_fd_offset = 0x30,
};

constexpr struct node_tlswrap_symaddrs_t kNodeSymaddrsV13_10_1 = {
    .TLSWrap_StreamListener_offset = 0x140,
    .StreamListener_stream_offset = 0x8,
    .StreamBase_StreamResource_offset = 0x00,
    .LibuvStreamWrap_StreamBase_offset = 0x60,
    .LibuvStreamWrap_stream_offset = 0xa0,
    .uv_stream_s_io_watcher_offset = 0x88,
    .uv__io_s_fd_offset = 0x30,
};

constexpr struct node_tlswrap_symaddrs_t kNodeSymaddrsV14_5_0 = {
    .TLSWrap_StreamListener_offset = 0x138,
    .StreamListener_stream_offset = 0x08,
    .StreamBase_StreamResource_offset = 0x00,
    .LibuvStreamWrap_StreamBase_offset = 0x58,
    .LibuvStreamWrap_stream_offset = 0x98,
    .uv_stream_s_io_watcher_offset = 0x88,
    .uv__io_s_fd_offset = 0x30,
};

// This works for version from 15.0 to 16.9 as tested. Versions newer than 16.9 should still be
// compatible, but requires testing.
constexpr struct node_tlswrap_symaddrs_t kNodeSymaddrsV15_0_0 = {
    .TLSWrap_StreamListener_offset = 0x78,
    .StreamListener_stream_offset = 0x08,
    .StreamBase_StreamResource_offset = 0x00,
    .LibuvStreamWrap_StreamBase_offset = 0x58,
    .LibuvStreamWrap_stream_offset = 0x98,
    .uv_stream_s_io_watcher_offset = 0x88,
    .uv__io_s_fd_offset = 0x30,
};

StatusOr<struct node_tlswrap_symaddrs_t> NodeTLSWrapSymAddrsFromVersion(const SemVer& ver) {
  LOG(INFO) << "Getting symbol offsets for version: " << ver.ToString();
  static const std::map<SemVer, struct node_tlswrap_symaddrs_t> kNodeVersionSymaddrs = {
      {SemVer{12, 3, 1}, kNodeSymaddrsV12_3_1},   {SemVer{12, 16, 2}, kNodeSymaddrsV12_16_2},
      {SemVer{13, 0, 0}, kNodeSymaddrsV13_0_0},   {SemVer{13, 2, 0}, kNodeSymaddrsV13_2_0},
      {SemVer{13, 10, 1}, kNodeSymaddrsV13_10_1}, {SemVer{14, 5, 0}, kNodeSymaddrsV14_5_0},
      {SemVer{15, 0, 0}, kNodeSymaddrsV15_0_0},
  };
  auto iter = Floor(kNodeVersionSymaddrs, ver);
  if (iter == kNodeVersionSymaddrs.end()) {
    return error::NotFound("Found no symbol offsets for version '$0'", ver.ToString());
  }
  return iter->second;
}

StatusOr<struct node_tlswrap_symaddrs_t> NodeTLSWrapSymAddrsFromDwarf(DwarfReader* dwarf_reader) {
  struct node_tlswrap_symaddrs_t symaddrs = {};

  PL_ASSIGN_OR_RETURN(symaddrs.TLSWrap_StreamListener_offset,
                      dwarf_reader->GetClassParentOffset("TLSWrap", "StreamListener"));

  PL_ASSIGN_OR_RETURN(symaddrs.StreamListener_stream_offset,
                      dwarf_reader->GetClassMemberOffset("StreamListener", "stream_"));

  PL_ASSIGN_OR_RETURN(symaddrs.StreamBase_StreamResource_offset,
                      dwarf_reader->GetClassParentOffset("StreamBase", "StreamResource"));

  PL_ASSIGN_OR_RETURN(symaddrs.LibuvStreamWrap_StreamBase_offset,
                      dwarf_reader->GetClassParentOffset("LibuvStreamWrap", "StreamBase"));

  PL_ASSIGN_OR_RETURN(symaddrs.LibuvStreamWrap_stream_offset,
                      dwarf_reader->GetClassMemberOffset("LibuvStreamWrap", "stream_"));

  PL_ASSIGN_OR_RETURN(symaddrs.uv_stream_s_io_watcher_offset,
                      dwarf_reader->GetStructMemberOffset("uv_stream_s", "io_watcher"));

  PL_ASSIGN_OR_RETURN(symaddrs.uv__io_s_fd_offset,
                      dwarf_reader->GetStructMemberOffset("uv__io_s", "fd"));

  return symaddrs;
}

StatusOr<struct node_tlswrap_symaddrs_t> NodeTLSWrapSymAddrs(const std::filesystem::path& node_exe,
                                                             const SemVer& ver) {
  // Indexing is disabled, because nodejs has 700+MB debug info file, and it takes >100 seconds to
  // index them.
  //
  // TODO(yzhao): We can implement "selective caching". The input needs to be a collection of symbol
  // patterns, which means only indexing the matched symbols.
  auto dwarf_reader_or = DwarfReader::CreateWithoutIndexing(node_exe);

  // Creation might fail if source language cannot be detected, which means that there is no dwarf
  // info.
  if (dwarf_reader_or.ok()) {
    auto symaddrs_or = NodeTLSWrapSymAddrsFromDwarf(dwarf_reader_or.ValueOrDie().get());
    if (symaddrs_or.ok()) {
      return symaddrs_or.ConsumeValueOrDie();
    }
  }

  // Try to lookup hard-coded symbol offsets with version.
  auto symaddrs_or = NodeTLSWrapSymAddrsFromVersion(ver);
  if (symaddrs_or.ok()) {
    return symaddrs_or.ConsumeValueOrDie();
  }

  return error::NotFound("Nodejs version cannot be older than 12.3.1, got '$0'", ver.ToString());
}

}  // namespace stirling
}  // namespace px
