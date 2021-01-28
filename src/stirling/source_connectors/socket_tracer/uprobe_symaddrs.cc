#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"
#include <dlfcn.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/obj_tools/dwarf_tools.h"
#include "src/stirling/obj_tools/elf_tools.h"

using ::pl::stirling::obj_tools::DwarfReader;
using ::pl::stirling::obj_tools::ElfReader;

namespace pl {
namespace stirling {

//-----------------------------------------------------------------------------
// Symbol Population Functions
//-----------------------------------------------------------------------------

// The functions in this section populate structs that contain locations of necessary symbols,
// which are then passed through a BPF map to the uprobe.
// For example, locations of required struct members are communicated through this fasion.

namespace {

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

#define GET_SYMADDR(symaddr, name)                        \
  symaddr = elf_reader->SymbolAddress(name).value_or(-1); \
  VLOG(1) << absl::Substitute(#symaddr " = $0", symaddr);

  GET_SYMADDR(symaddrs->internal_syscallConn,
              absl::StrCat("go.itab.*", vendor_prefix,
                           "google.golang.org/grpc/credentials/internal.syscallConn,net.Conn"));
  GET_SYMADDR(symaddrs->tls_Conn, "go.itab.*crypto/tls.Conn,net.Conn");
  GET_SYMADDR(symaddrs->net_TCPConn, "go.itab.*net.TCPConn,net.Conn");

#undef GET_SYMADDR

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

#define GET_MEMBER_OFFSET(symaddr, type, member)                           \
  symaddr = dwarf_reader->GetStructMemberOffset(type, member).ValueOr(-1); \
  VLOG(1) << absl::Substitute(#symaddr " = $0", symaddr);

  // clang-format off
  GET_MEMBER_OFFSET(symaddrs->FD_Sysfd_offset,
                    "internal/poll.FD",
                    "Sysfd");
  GET_MEMBER_OFFSET(symaddrs->tlsConn_conn_offset,
                    "crypto/tls.Conn",
                    "conn");
  GET_MEMBER_OFFSET(symaddrs->syscallConn_conn_offset,
                    VENDOR_SYMBOL("google.golang.org/grpc/credentials/internal.syscallConn"),
                    "conn");
  // clang-format on

#undef GET_MEMBER_OFFSET
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

#define GET_SYMADDR(symaddr, name)                        \
  symaddr = elf_reader->SymbolAddress(name).value_or(-1); \
  VLOG(1) << absl::Substitute(#symaddr " = $0", symaddr);

  GET_SYMADDR(symaddrs->http_http2bufferedWriter,
              "go.itab.*net/http.http2bufferedWriter,io.Writer");
  GET_SYMADDR(symaddrs->transport_bufWriter,
              absl::StrCat("go.itab.*", vendor_prefix,
                           "google.golang.org/grpc/internal/transport.bufWriter,io.Writer"));

#undef GET_SYMADDR

  return Status::OK();
}

Status PopulateHTTP2DebugSymbols(DwarfReader* dwarf_reader, std::string_view vendor_prefix,
                                 struct go_http2_symaddrs_t* symaddrs) {
  // Note: we only return error if a *mandatory* symbol is missing. Currently none are mandatory,
  // because these multiple probes for multiple HTTP2/GRPC libraries. Even if a symbol for one
  // is missing it doesn't mean the other library's probes should not be deployed.

#define VENDOR_SYMBOL(symbol) absl::StrCat(vendor_prefix, symbol)

#define GET_MEMBER_OFFSET(symaddr, type, member)                           \
  symaddr = dwarf_reader->GetStructMemberOffset(type, member).ValueOr(-1); \
  VLOG(1) << absl::Substitute(#symaddr " = $0", symaddr);

  // clang-format off
  GET_MEMBER_OFFSET(symaddrs->HeaderField_Name_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2/hpack.HeaderField"),
                    "Name");
  GET_MEMBER_OFFSET(symaddrs->HeaderField_Value_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2/hpack.HeaderField"),
                    "Value");
  GET_MEMBER_OFFSET(symaddrs->http2Server_conn_offset,
                    VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.http2Server"),
                    "conn");
  GET_MEMBER_OFFSET(symaddrs->http2Client_conn_offset,
                    VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.http2Client"),
                    "conn");
  GET_MEMBER_OFFSET(symaddrs->loopyWriter_framer_offset,
                    VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.loopyWriter"),
                    "framer");
  GET_MEMBER_OFFSET(symaddrs->Framer_w_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2.Framer"),
                    "w");
  GET_MEMBER_OFFSET(symaddrs->MetaHeadersFrame_HeadersFrame_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2.MetaHeadersFrame"),
                    "HeadersFrame");
  GET_MEMBER_OFFSET(symaddrs->MetaHeadersFrame_Fields_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2.MetaHeadersFrame"),
                    "Fields");
  GET_MEMBER_OFFSET(symaddrs->HeadersFrame_FrameHeader_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2.HeadersFrame"),
                    "FrameHeader");
  GET_MEMBER_OFFSET(symaddrs->FrameHeader_Type_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2.FrameHeader"),
                    "Type");
  GET_MEMBER_OFFSET(symaddrs->FrameHeader_Flags_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2.FrameHeader"),
                    "Flags");
  GET_MEMBER_OFFSET(symaddrs->FrameHeader_StreamID_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2.FrameHeader"),
                    "StreamID");
  GET_MEMBER_OFFSET(symaddrs->DataFrame_data_offset,
                    VENDOR_SYMBOL("golang.org/x/net/http2.DataFrame"),
                    "data");
  GET_MEMBER_OFFSET(symaddrs->bufWriter_conn_offset,
                    VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.bufWriter"),
                    "conn");
  GET_MEMBER_OFFSET(symaddrs->http2serverConn_conn_offset,
                    "net/http.http2serverConn",
                    "conn");
  GET_MEMBER_OFFSET(symaddrs->http2serverConn_hpackEncoder_offset,
                    "net/http.http2serverConn",
                    "hpackEncoder");
  GET_MEMBER_OFFSET(symaddrs->http2HeadersFrame_http2FrameHeader_offset,
                    "net/http.http2HeadersFrame",
                    "http2FrameHeader");
  GET_MEMBER_OFFSET(symaddrs->http2FrameHeader_Type_offset,
                    "net/http.http2FrameHeader",
                    "Type");
  GET_MEMBER_OFFSET(symaddrs->http2FrameHeader_Flags_offset,
                    "net/http.http2FrameHeader",
                    "Flags");
  GET_MEMBER_OFFSET(symaddrs->http2FrameHeader_StreamID_offset,
                    "net/http.http2FrameHeader",
                    "StreamID");
  GET_MEMBER_OFFSET(symaddrs->http2DataFrame_data_offset,
                    "net/http.http2DataFrame",
                    "data");
  GET_MEMBER_OFFSET(symaddrs->http2writeResHeaders_streamID_offset,
                    "net/http.http2writeResHeaders",
                    "streamID");
  GET_MEMBER_OFFSET(symaddrs->http2writeResHeaders_endStream_offset,
                    "net/http.http2writeResHeaders",
                    "endStream");
  GET_MEMBER_OFFSET(symaddrs->http2MetaHeadersFrame_http2HeadersFrame_offset,
                    "net/http.http2MetaHeadersFrame",
                    "http2HeadersFrame");
  GET_MEMBER_OFFSET(symaddrs->http2MetaHeadersFrame_Fields_offset,
                    "net/http.http2MetaHeadersFrame",
                    "Fields");
  GET_MEMBER_OFFSET(symaddrs->http2Framer_w_offset,
                    "net/http.http2Framer",
                    "w");
  GET_MEMBER_OFFSET(symaddrs->http2bufferedWriter_w_offset,
                    "net/http.http2bufferedWriter",
                    "w");
  // clang-format on

#undef GET_MEMBER_OFFSET

  // The information from DWARF assumes SP is 8 bytes larger than the SP
  // we get from BPF code, so add the correction factor here.
  constexpr int32_t kSPOffset = 8;
  const std::map<std::string, obj_tools::ArgInfo> kEmptyMap;

#define GET_ARG_OFFSET(symaddr, fn_args_map, arg)                                        \
  {                                                                                      \
    auto it = fn_args_map.find(arg);                                                     \
    symaddr = (it != fn_args_map.end()) ? (it->second.location.offset + kSPOffset) : -1; \
    VLOG(1) << absl::Substitute(#symaddr " = $0", symaddr);                              \
  }

  // Arguments of net/http.(*http2Framer).WriteDataPadded.
  {
    std::string_view fn = "net/http.(*http2Framer).WriteDataPadded";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->http2Framer_WriteDataPadded_f_offset, args_map, "f");
    GET_ARG_OFFSET(symaddrs->http2Framer_WriteDataPadded_streamID_offset, args_map, "streamID");
    GET_ARG_OFFSET(symaddrs->http2Framer_WriteDataPadded_endStream_offset, args_map, "endStream");
    GET_ARG_OFFSET(symaddrs->http2Framer_WriteDataPadded_data_offset, args_map, "data");
  }

  // Arguments of golang.org/x/net/http2.(*Framer).WriteDataPadded.
  {
    std::string fn = VENDOR_SYMBOL("golang.org/x/net/http2.(*Framer).WriteDataPadded");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->http2_WriteDataPadded_f_offset, args_map, "f");
    GET_ARG_OFFSET(symaddrs->http2_WriteDataPadded_streamID_offset, args_map, "streamID");
    GET_ARG_OFFSET(symaddrs->http2_WriteDataPadded_endStream_offset, args_map, "endStream");
    GET_ARG_OFFSET(symaddrs->http2_WriteDataPadded_data_offset, args_map, "data");
  }

  // Arguments of net/http.(*http2Framer).checkFrameOrder.
  {
    std::string_view fn = "net/http.(*http2Framer).checkFrameOrder";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->http2Framer_checkFrameOrder_fr_offset, args_map, "fr");
    GET_ARG_OFFSET(symaddrs->http2Framer_checkFrameOrder_f_offset, args_map, "f");
  }

  // Arguments of golang.org/x/net/http2.(*Framer).checkFrameOrder.
  {
    std::string fn = VENDOR_SYMBOL("golang.org/x/net/http2.(*Framer).checkFrameOrder");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->http2_checkFrameOrder_fr_offset, args_map, "fr");
    GET_ARG_OFFSET(symaddrs->http2_checkFrameOrder_f_offset, args_map, "f");
  }

  // Arguments of net/http.(*http2writeResHeaders).writeFrame.
  {
    std::string_view fn = "net/http.(*http2writeResHeaders).writeFrame";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->writeFrame_w_offset, args_map, "w");
    GET_ARG_OFFSET(symaddrs->writeFrame_ctx_offset, args_map, "ctx");
  }

  // Arguments of golang.org/x/net/http2/hpack.(*Encoder).WriteField.
  {
    std::string fn = VENDOR_SYMBOL("golang.org/x/net/http2/hpack.(*Encoder).WriteField");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->WriteField_e_offset, args_map, "e");
    GET_ARG_OFFSET(symaddrs->WriteField_f_offset, args_map, "f");
  }

  // Arguments of net/http.(*http2serverConn).processHeaders.
  {
    std::string_view fn = "net/http.(*http2serverConn).processHeaders";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->processHeaders_sc_offset, args_map, "sc");
    GET_ARG_OFFSET(symaddrs->processHeaders_f_offset, args_map, "f");
  }

  // Arguments of google.golang.org/grpc/internal/transport.(*http2Server).operateHeaders.
  {
    std::string fn =
        VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.(*http2Server).operateHeaders");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->http2Server_operateHeaders_t_offset, args_map, "t");
    GET_ARG_OFFSET(symaddrs->http2Server_operateHeaders_frame_offset, args_map, "frame");
  }

  // Arguments of google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders.
  {
    std::string fn =
        VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->http2Client_operateHeaders_t_offset, args_map, "t");
    GET_ARG_OFFSET(symaddrs->http2Client_operateHeaders_frame_offset, args_map, "frame");
  }

  // Arguments of google.golang.org/grpc/internal/transport.(*loopyWriter).writeHeader.
  {
    std::string fn =
        VENDOR_SYMBOL("google.golang.org/grpc/internal/transport.(*loopyWriter).writeHeader");
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->writeHeader_l_offset, args_map, "l");
    GET_ARG_OFFSET(symaddrs->writeHeader_streamID_offset, args_map, "streamID");
    GET_ARG_OFFSET(symaddrs->writeHeader_endStream_offset, args_map, "endStream");
    GET_ARG_OFFSET(symaddrs->writeHeader_hf_offset, args_map, "hf");
  }

#undef GET_ARG_OFFSET

#undef VENDOR_SYMBOL

  return Status::OK();
}

Status PopulateGoTLSDebugSymbols(DwarfReader* dwarf_reader, struct go_tls_symaddrs_t* symaddrs) {
  // The information from DWARF assumes SP is 8 bytes larger than the SP
  // we get from BPF code, so add the correction factor here.
  // This is due to the return address being on the stack.
  constexpr int32_t kSPOffset = 8;
  const std::map<std::string, obj_tools::ArgInfo> kEmptyMap;

#define GET_ARG_OFFSET(symaddr, fn_args_map, arg)                                        \
  {                                                                                      \
    auto it = fn_args_map.find(arg);                                                     \
    symaddr = (it != fn_args_map.end()) ? (it->second.location.offset + kSPOffset) : -1; \
    VLOG(1) << absl::Substitute(#symaddr " = $0", symaddr);                              \
  }

  // Arguments of crypto/tls.(*Conn).Write.
  {
    std::string_view fn = "crypto/tls.(*Conn).Write";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->Write_c_offset, args_map, "c");
    GET_ARG_OFFSET(symaddrs->Write_b_offset, args_map, "b");
  }

  // Arguments of crypto/tls.(*Conn).Read.
  {
    std::string fn = "crypto/tls.(*Conn).Read";
    auto args_map = dwarf_reader->GetFunctionArgInfo(fn).ValueOr(kEmptyMap);
    GET_ARG_OFFSET(symaddrs->Read_c_offset, args_map, "c");
    GET_ARG_OFFSET(symaddrs->Read_b_offset, args_map, "b");
  }

#undef GET_ARG_OFFSET

  // List mandatory symaddrs here (symaddrs without which all probes become useless).
  // Returning an error will prevent the probes from deploying.
  if (symaddrs->Write_b_offset == -1 || symaddrs->Read_b_offset == -1) {
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
  if (!fs::Exists(lib_openssl_path).ok()) {
    return error::Internal("Path to OpenSSL so is not valid: $0", lib_openssl_path.string());
  }

  void* h = dlopen(lib_openssl_path.c_str(), RTLD_LAZY);

  if (h == nullptr) {
    return error::Internal("Failed to dlopen OpenSSL so file: $0, $1", lib_openssl_path.string(),
                           dlerror());
  }

  const std::string version_num_symbol = "OpenSSL_version_num";

  // NOLINTNEXTLINE(runtime/int): 'unsigned long' is from upstream, match that here (vs. uint64_t)
  PL_ASSIGN_OR_RETURN(auto version_num_f, DLSymbolToFptr<unsigned long()>(h, version_num_symbol));

  const uint64_t version_num = version_num_f();
  dlclose(h);
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

  return symaddrs;
}

// Appendix: Using GDB to confirm member offsets on OpenSSL 1.1.1:
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

}  // namespace stirling
}  // namespace pl
