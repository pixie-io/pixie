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
#include <utility>
#include <vector>

#include <absl/synchronization/mutex.h>

#include "src/common/system/proc_parser.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/obj_tools/raw_fptr_manager.h"

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/grpc_c.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"

#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"
#include "src/stirling/utils/detect_application.h"
#include "src/stirling/utils/monitor.h"
#include "src/stirling/utils/proc_path_tools.h"
#include "src/stirling/utils/proc_tracker.h"

DECLARE_bool(stirling_rescan_for_dlopen);
DECLARE_bool(stirling_enable_grpc_c_tracing);
DECLARE_double(stirling_rescan_exp_backoff_factor);
DECLARE_bool(stirling_trace_static_tls_binaries);

namespace px {
namespace stirling {

/**
 * Describes a UProbe template.
 * In particular, allows for partial symbol matches using SymbolMatchType.
 */
struct UProbeTmpl {
  std::string_view symbol;
  obj_tools::SymbolMatchType match_type;
  std::string_view probe_fn;
  bpf_tools::BPFProbeAttachType attach_type = bpf_tools::BPFProbeAttachType::kEntry;
};

using px::stirling::bpf_tools::WrappedBCCMap;

/**
 * UProbeManager manages the deploying of all uprobes on behalf of the SocketTracer.
 * This includes: OpenSSL uprobes, GoTLS uprobes and Go HTTP2 uprobes.
 */
class UProbeManager {
  template <typename V>
  using MapT = WrappedBCCMap<uint32_t, V, /* user space managed */ true>;

 public:
  /**
   * Construct a UProbeManager.
   * @param bcc A pointer to a BCCWrapper instance that is used to deploy uprobes.
   */
  explicit UProbeManager(bpf_tools::BCCWrapper* bcc);

  /**
   * Mandatory initialization step before RunDeployUprobesThread can be called.
   * @param disable_go_tls_tracing Whether to disable Go TLS tracing. Implies enable_http2_tracing
   * is false.
   * @param enable_http2_tracing Whether to enable HTTP2 tracing.
   * @param disable_self_tracing Whether to enable uprobe deployment on Stirling itself.
   */
  void Init(bool disable_go_tls_tracing, bool enable_http2_tracing,
            bool disable_self_tracing = true);

  /**
   * Notify uprobe manager of an mmap event. An mmap may be indicative of a dlopen,
   * so this is used to determine when to rescan binaries for newly loaded shared libraries.
   * @param upid UPID of the process that performed the mmap.
   */
  void NotifyMMapEvent(upid_t upid);

  /**
   * Runs the uprobe deployment code on the provided set of pids, as a thread.
   * @param pids New PIDs to analyze deploy uprobes on. Old PIDs can also be provided,
   *             if they need to be rescanned.
   * @return thread that handles the uprobe deployment work.
   */
  std::thread RunDeployUProbesThread(const absl::flat_hash_set<md::UPID>& pids);

  /**
   * Returns true if a previously dispatched thread (via RunDeployUProbesThread is still running).
   */
  bool ThreadsRunning() { return num_deploy_uprobes_threads_ != 0; }

 private:
  inline static constexpr auto kHTTP2ProbeTmpls = MakeArray<UProbeTmpl>({
      // Probes on Golang net/http2 library.
      UProbeTmpl{
          .symbol = "google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_http2_client_operate_headers",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "google.golang.org/grpc/internal/transport.(*http2Server).operateHeaders",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_http2_server_operate_headers",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "google.golang.org/grpc/internal/transport.(*loopyWriter).writeHeader",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_loopy_writer_write_header",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "golang.org/x/net/http2.(*Framer).WriteDataPadded",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_http2_framer_write_data",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "golang.org/x/net/http2.(*Framer).checkFrameOrder",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_http2_framer_check_frame_order",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },

      // Probes on Golang net/http's implementation of http2.
      UProbeTmpl{
          .symbol = "net/http.(*http2Framer).WriteDataPadded",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_http_http2framer_write_data",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "net/http.(*http2Framer).checkFrameOrder",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_http_http2framer_check_frame_order",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "net/http.(*http2writeResHeaders).writeFrame",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_http_http2writeResHeaders_write_frame",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "golang.org/x/net/http2/hpack.(*Encoder).WriteField",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_hpack_header_encoder",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "net/http.(*http2serverConn).processHeaders",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_http_http2serverConn_processHeaders",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
  });

  // Probes on Golang crypto/tls library.
  inline static const auto kGoTLSUProbeTmpls = MakeArray<UProbeTmpl>({
      UProbeTmpl{
          .symbol = "crypto/tls.(*Conn).Write",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_entry_tls_conn_write",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "crypto/tls.(*Conn).Write",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_return_tls_conn_write",
          .attach_type = bpf_tools::BPFProbeAttachType::kReturnInsts,
      },
      UProbeTmpl{
          .symbol = "crypto/tls.(*Conn).Read",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_entry_tls_conn_read",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      },
      UProbeTmpl{
          .symbol = "crypto/tls.(*Conn).Read",
          .match_type = obj_tools::SymbolMatchType::kSuffix,
          .probe_fn = "probe_return_tls_conn_read",
          .attach_type = bpf_tools::BPFProbeAttachType::kReturnInsts,
      },
  });

  // TODO(yzhao): Regroups OpenSSL uprobes into 3 groups: 1) OpenSSL dynamic library; 2) OpenSSL
  // static library (no known cases other than nodejs today, but should support for future-proof);
  // 3) NodeJS specific uprobes.

  // Probes on node' C++ functions for obtaining the file descriptor from TLSWrap object.
  // The match type is kPrefix to (hopefully) tolerate potential changes in argument
  // order/type/count etc.
  inline static const std::array<UProbeTmpl, 6> kNodeOpenSSLUProbeTmplsV12_3_1 =
      MakeArray<UProbeTmpl>({
          UProbeTmpl{
              .symbol = "_ZN4node7TLSWrapC2E",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_entry_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          },
          UProbeTmpl{
              .symbol = "_ZN4node7TLSWrapC2E",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_ret_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          },
          UProbeTmpl{
              .symbol = "_ZN4node7TLSWrap7ClearInE",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_entry_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          },
          UProbeTmpl{
              .symbol = "_ZN4node7TLSWrap7ClearInE",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_ret_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          },
          UProbeTmpl{
              .symbol = "_ZN4node7TLSWrap8ClearOutE",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_entry_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          },
          UProbeTmpl{
              .symbol = "_ZN4node7TLSWrap8ClearOutE",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_ret_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          },
      });

  // In nodejs 15.0.0, openssl-related code is moved from ::node namespace to ::node::crypto
  // namespace. So the symbol for attaching has been changed from the above version.
  inline static const std::array<UProbeTmpl, 6> kNodeOpenSSLUProbeTmplsV15_0_0 =
      MakeArray<UProbeTmpl>({
          UProbeTmpl{
              .symbol = "_ZN4node6crypto7TLSWrapC2E",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_entry_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          },
          UProbeTmpl{
              .symbol = "_ZN4node6crypto7TLSWrapC2E",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_ret_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          },
          UProbeTmpl{
              .symbol = "_ZN4node6crypto7TLSWrap7ClearInE",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_entry_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          },
          UProbeTmpl{
              .symbol = "_ZN4node6crypto7TLSWrap7ClearInE",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_ret_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          },
          UProbeTmpl{
              .symbol = "_ZN4node6crypto7TLSWrap8ClearOutE",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_entry_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          },
          UProbeTmpl{
              .symbol = "_ZN4node6crypto7TLSWrap8ClearOutE",
              .match_type = obj_tools::SymbolMatchType::kPrefix,
              .probe_fn = "probe_ret_TLSWrap_memfn",
              .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          },
      });

  // When the gRPC-c probes run, they need to know the library's version.
  // To tell them which version is deployed, we find the version during probe attachment.
  // No neat mechanism was found to tell the version (we can't run the library with "--version"
  // and running "strings" on the library does not always return the correct version as well).
  // To tell the version, we hash the library's binary. A hash will match a single version.
  // When adding support for more versions, we should add their respective hashes here (for
  // example by building the relevant docker images with gRPC-c installed and hashing the library).
  // TODO(yzhao) - Add hashes of more docker images that use the supported versions.
  inline static const std::map<std::string, enum grpc_c_version_t> kGrpcCMD5HashToVersion = {
      {"64c205d1bc547cd53d6979fb76674f4b",  // python:3.7-slim grpcio-1.19.0
       grpc_c_version_t::GRPC_C_V1_19_0},
      {"43946bf95efc74729b96ea5630aa8067",  // python:3.7-slim grpcio-1.24.1
       grpc_c_version_t::GRPC_C_V1_24_1},
      {"3f9097d182b9a9392522e78945e776af",  // python:3.7-slim grpcio-1.33.2
       grpc_c_version_t::GRPC_C_V1_33_2},
      {"ddf1c743895aaf9fff5d2ca944e16052",  // python:3.5-alpine
       grpc_c_version_t::GRPC_C_V1_41_1}};

  // Probes for GRPC-C tracing.
  // The binary path field is going to be changed during attachment, so it's meaningless here.
  // TODO(yzhao) - consider using UProbeTmpls instead of UProbeSpecs. This way, we won't have to
  // provide the exact function name, and can instead use obj_tools::SymbolMatchType::kSubstr.
  // However, UProbeTmpls don't allow attaching by address (only by symbol), which we will
  // potentially need for these probes, because the gRPC-C library is stripped in newer version.
  inline static const auto kGrpcCUProbes = MakeArray<bpf_tools::UProbeSpec>({
      // grpc_chttp2_list_pop_writable_stream
      bpf_tools::UProbeSpec{
          .binary_path = "",
          .symbol = "_Z36grpc_chttp2_list_pop_writable_streamP21grpc_chttp2_transportPP18grpc_"
                    "chttp2_stream",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_entry_grpc_chttp2_list_pop_writable_stream",
      },
      bpf_tools::UProbeSpec{
          .binary_path = "",
          .symbol = "_Z36grpc_chttp2_list_pop_writable_streamP21grpc_chttp2_transportPP18grpc_"
                    "chttp2_stream",
          .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          .probe_fn = "probe_ret_grpc_chttp2_list_pop_writable_stream",
      },
      // grpc_chttp2_mark_stream_closed
      bpf_tools::UProbeSpec{
          .binary_path = "",
          .symbol = "_Z30grpc_chttp2_mark_stream_closedP21grpc_chttp2_transportP18grpc_chttp2_"
                    "streamiiP10grpc_error",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_grpc_chttp2_mark_stream_closed",
      },
      // grpc_chttp2_maybe_complete_recv_initial_metadata
      bpf_tools::UProbeSpec{
          .binary_path = "",
          .symbol = "_Z48grpc_chttp2_maybe_complete_recv_initial_metadataP21grpc_chttp2_"
                    "transportP18grpc_chttp2_stream",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_grpc_chttp2_maybe_complete_recv_initial_metadata",
      },
      // grpc_chttp2_maybe_complete_recv_trailing_metadata
      bpf_tools::UProbeSpec{
          .binary_path = "",
          .symbol = "_Z49grpc_chttp2_maybe_complete_recv_trailing_metadataP21grpc_chttp2_"
                    "transportP18grpc_chttp2_stream",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_grpc_chttp2_maybe_complete_recv_trailing_metadata",
      },
  });

  // grpc_chttp2_data_parser_parse
  // This function's symbol changes slightly between library version. Since to attach to it with a
  // UProbeSpec we need the exact symbol, we need to store the different options.
  // The symbol could be one of two: once where the slice is const (e.g.
  // version 1.19.1) and once where it ain't.
  // To overcome that, we try to attach the symbols in this array, until we find one that succeeds.
  // TODO(yzhao) - We should probably change UProbeSpec to UProbeTmpl (currently not feasible
  // because UProbeTmpl does not support attachment by address) and then we can add this probe to
  // the same array with the other grpc-c probes (because we would only need a part of the symbol).
  inline static const auto kGrpcCDataParserParseUProbes = MakeArray<bpf_tools::UProbeSpec>({
      bpf_tools::UProbeSpec{
          .binary_path = "",
          .symbol = "_Z29grpc_chttp2_data_parser_parsePvP21grpc_chttp2_transportP18grpc_chttp2_"
                    "streamRK10grpc_slicei",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_grpc_chttp2_data_parser_parse",
      },
      bpf_tools::UProbeSpec{
          .binary_path = "",
          .symbol = "_Z29grpc_chttp2_data_parser_parsePvP21grpc_chttp2_transportP18grpc_chttp2_"
                    "stream10grpc_slicei",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_grpc_chttp2_data_parser_parse",
      },
  });

  int DeployGrpcCUProbes(const absl::flat_hash_set<md::UPID>& pids);
  // We hash grpc-c libraries to know its version.
  // For further explanation see the definition of kGrpcCMD5HashToVersion.
  StatusOr<std::string> MD5onFile(const std::string& file);
  StatusOr<int> AttachGrpcCUProbesOnDynamicPythonLib(uint32_t pid);

  static StatusOr<std::array<UProbeTmpl, 6>> GetNodeOpensslUProbeTmpls(const SemVer& ver);

  // Probes for OpenSSL tracing.
  inline static const auto kOpenSSLUProbes = MakeArray<bpf_tools::UProbeSpec>({
      bpf_tools::UProbeSpec{
          .binary_path = "/usr/lib/x86_64-linux-gnu/libssl.so.1.1",
          .symbol = "SSL_write",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_entry_SSL_write",
      },
      bpf_tools::UProbeSpec{
          .binary_path = "/usr/lib/x86_64-linux-gnu/libssl.so.1.1",
          .symbol = "SSL_write",
          .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          .probe_fn = "probe_ret_SSL_write",
      },
      bpf_tools::UProbeSpec{
          .binary_path = "/usr/lib/x86_64-linux-gnu/libssl.so.1.1",
          .symbol = "SSL_read",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_entry_SSL_read",
      },
      bpf_tools::UProbeSpec{
          .binary_path = "/usr/lib/x86_64-linux-gnu/libssl.so.1.1",
          .symbol = "SSL_read",
          .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          .probe_fn = "probe_ret_SSL_read",
      },
      // The _ex variants are declared optional since they were not available
      // prior to OpenSSL 1.1.1 and may not be available for statically linked
      // applications (NodeJS).
      bpf_tools::UProbeSpec{
          .binary_path = "/usr/lib/x86_64-linux-gnu/libssl.so.1.1",
          .symbol = "SSL_write_ex",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_entry_SSL_write_ex",
          .is_optional = true,
      },
      bpf_tools::UProbeSpec{
          .binary_path = "/usr/lib/x86_64-linux-gnu/libssl.so.1.1",
          .symbol = "SSL_write_ex",
          .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          .probe_fn = "probe_ret_SSL_write_ex",
          .is_optional = true,
      },
      bpf_tools::UProbeSpec{
          .binary_path = "/usr/lib/x86_64-linux-gnu/libssl.so.1.1",
          .symbol = "SSL_read_ex",
          .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
          .probe_fn = "probe_entry_SSL_read_ex",
          .is_optional = true,
      },
      bpf_tools::UProbeSpec{
          .binary_path = "/usr/lib/x86_64-linux-gnu/libssl.so.1.1",
          .symbol = "SSL_read_ex",
          .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          .probe_fn = "probe_ret_SSL_read_ex",
          .is_optional = true,
      },
      // Used by node tracing to record the mapping from SSL object to TLSWrap object.
      // TODO(yzhao): Move this to a separate list for node application only.
      bpf_tools::UProbeSpec{
          .binary_path = "/usr/lib/x86_64-linux-gnu/libssl.so.1.1",
          .symbol = "SSL_new",
          .attach_type = bpf_tools::BPFProbeAttachType::kReturn,
          .probe_fn = "probe_ret_SSL_new",
      },
  });

  /**
   * Deploys all available uprobe types (HTTP2, OpenSSL, etc.) on new processes.
   * @param pids The list of pids to analyze and instrument with uprobes, if appropriate.
   */
  void DeployUProbes(const absl::flat_hash_set<md::UPID>& pids);

  /**
   * Deploys all OpenSSL uprobes on new processes.
   * @param pids The list of pids to analyze and instrument with OpenSSL uprobes, if appropriate.
   * @return Number of uprobes deployed.
   */
  int DeployOpenSSLUProbes(const absl::flat_hash_set<md::UPID>& pids);

  /**
   * Deploys all Go uprobes on new processes.
   * @param pids The list of pids to analyze and instrument with Go uprobes, if appropriate.
   * @return Number of uprobes deployed.
   */
  int DeployGoUProbes(const absl::flat_hash_set<md::UPID>& pids);

  /**
   * Sets up the BPF maps used for GOID tracking. Required for general Go tracing.
   *
   * @param binary The path to the binary on which to deploy Go probes.
   * @param pids The list of PIDs that are new instances of the binary.
   */
  void SetupGOIDMaps(const std::string& binary, const std::vector<int32_t>& pids);

  /**
   * Attaches the required uprobes for Go HTTP2 tracing to the specified binary, if it is a
   * compatible Go binary.
   *
   * @param binary The path to the binary on which to deploy Go HTTP2 probes.
   * @param elf_reader ELF reader for the binary.
   * @param dwarf_reader DWARF reader for the binary.
   * @param pids The list of PIDs that are new instances of the binary. Used to populate symbol
   *             addresses.
   * @return The number of uprobes deployed, or error. It is not considered an error if the binary
   *         is not a Go binary or doesn't use a Go HTTP2 library; instead the return value will be
   *         zero.
   */
  StatusOr<int> AttachGoHTTP2UProbes(const std::string& binary, obj_tools::ElfReader* elf_reader,
                                     obj_tools::DwarfReader* dwarf_reader,
                                     const std::vector<int32_t>& pids);

  /**
   * Attaches the required probes for GoTLS tracing to the specified binary, if it is a compatible
   * Go binary.
   *
   * @param binary The path to the binary on which to deploy Go HTTP2 probes.
   * @param elf_reader ELF reader for the binary.
   * @param dwarf_reader DWARF reader for the binary.
   * @param pids The list of PIDs that are new instances of the binary. Used to populate symbol
   *             addresses.
   * @return The number of uprobes deployed, or error. It is not an error if the binary
   *         is not a Go binary or doesn't use Go TLS; instead the return value will be zero.
   */
  StatusOr<int> AttachGoTLSUProbes(const std::string& binary, obj_tools::ElfReader* elf_reader,
                                   obj_tools::DwarfReader* dwarf_reader,
                                   const std::vector<int32_t>& new_pids);

  /**
   * Attaches the required probes for OpenSSL tracing to the specified PID, if it uses OpenSSL.
   *
   * @param pid The PID of the process whose mount namespace is examined for OpenSSL dynamic library
   * files.
   * @return The number of uprobes deployed. It is not an error if the binary
   *         does not use OpenSSL; instead the return value will be zero.
   */
  StatusOr<int> AttachOpenSSLUProbesOnDynamicLib(uint32_t pid);

  /**
   * Attaches the required probes for OpenSSL tracing the executable of the specified PID.
   * The OpenSSL library is assumed to be statically linked into the executable.
   *
   * @param pid The PID of the process whose executable is attached with the probes.
   * @return The number of uprobes deployed. It is not an error if the binary
   * does not use OpenSSL; instead the return value will be zero.
   */
  StatusOr<int> AttachNodeJsOpenSSLUprobes(uint32_t pid);

  /**
   * Attaches the required probes for TLS tracing to the specified PID if the binary is
   * statically linked with the necessary OpenSSL compatible symbols. This will only capture
   * data if the binary uses the OpenSSL API in a BIO native way -- where OpenSSL IO primitives
   * are used rather than just its encryption functionality.
   *
   * @param pid The PID of the process whose binary is examined for OpenSSL symbols statically
   * linked.
   * @return The number of uprobes deployed. It is not an error if the binary
   *         does not contain the necessary symbols to probe; instead the return value will be zero.
   */
  StatusOr<int> AttachOpenSSLUProbesOnStaticBinary(uint32_t pid);

  /**
   * Calls BCCWrapper.AttachUProbe() with a probe template and log any errors to the probe status
   * table.
   */
  Status LogAndAttachUProbe(const bpf_tools::UProbeSpec& spec);

  /**
   * Helper function that calls BCCWrapper.AttachUprobe() from a probe template.
   * Among other things, it finds all symbol matches as specified in the template,
   * and attaches a probe per matching symbol.
   *
   * @param probe_tmpls Array of probe templates to process.
   * @param binary The binary to uprobe.
   * @param elf_reader Pointer to an elf reader for the binary. Used to find symbol matches.
   * @return Number of uprobes deployed, or error if uprobes failed to deploy. Zero uprobes
   *         deploying because there are no symbol matches is not considered an error.
   */
  StatusOr<int> AttachUProbeTmpl(const ArrayView<UProbeTmpl>& probe_tmpls,
                                 const std::string& binary, obj_tools::ElfReader* elf_reader);

  // Returns set of PIDs that have had mmap called on them since the last call.
  absl::flat_hash_set<md::UPID> PIDsToRescanForUProbes();

  Status UpdateOpenSSLSymAddrs(px::stirling::obj_tools::RawFptrManager* fptrManager,
                               std::filesystem::path container_lib, uint32_t pid);
  Status UpdateGoCommonSymAddrs(obj_tools::ElfReader* elf_reader,
                                obj_tools::DwarfReader* dwarf_reader,
                                const std::vector<int32_t>& pids);
  Status UpdateGoHTTP2SymAddrs(obj_tools::ElfReader* elf_reader,
                               obj_tools::DwarfReader* dwarf_reader,
                               const std::vector<int32_t>& pids);
  Status UpdateGoTLSSymAddrs(obj_tools::ElfReader* elf_reader, obj_tools::DwarfReader* dwarf_reader,
                             const std::vector<int32_t>& pids);
  Status UpdateNodeTLSWrapSymAddrs(int32_t pid, const std::filesystem::path& node_exe,
                                   const SemVer& ver);

  // Clean-up various BPF maps used to communicate symbol addresses per PID.
  // Once the PID has terminated, the information is not required anymore.
  // Note that BPF maps can fill up if this is not done.
  void CleanupPIDMaps(const absl::flat_hash_set<md::UPID>& deleted_upids);

  bpf_tools::BCCWrapper* bcc_;

  // Whether to try to uprobe ourself (e.g. for OpenSSL). Typically, we don't want to do that.
  bool cfg_disable_self_probing_;

  // Whether we want to enable Go TLS tracing. When true, it implies cfg_enable_http2_tracing_ is
  // false.
  bool cfg_disable_go_tls_tracing_;

  // Whether we want to enable HTTP2 tracing. When false, we don't deploy HTTP2 uprobes.
  bool cfg_enable_http2_tracing_;

  // Ensures DeployUProbes threads run sequentially.
  std::mutex deploy_uprobes_mutex_;
  std::atomic<int> num_deploy_uprobes_threads_ = 0;

  std::unique_ptr<system::ProcParser> proc_parser_;
  ProcTracker proc_tracker_;

  absl::flat_hash_set<upid_t> upids_with_mmap_;

  // Count the number of times PIDsToRescanForUProbes() has been called.
  int rescan_counter_ = 0;

  // Map of UPIDs to the periodicity at which they are allowed to be rescanned.
  // The backoff value starts at 1 (meaning they can be scanned every iteration),
  // and exponentially grows every time nothing new is found.
  absl::flat_hash_map<md::UPID, int> backoff_map_;

  // Records the binaries that have uprobes attached, so we don't try to probe them again.
  // TODO(oazizi): How should these sets be cleaned up of old binaries, once they are deleted?
  //               Without clean-up, these could consume more-and-more memory.
  absl::flat_hash_set<std::string> openssl_probed_binaries_;
  absl::flat_hash_set<std::string> scanned_binaries_;
  absl::flat_hash_set<std::string> go_probed_binaries_;
  absl::flat_hash_set<std::string> go_http2_probed_binaries_;
  absl::flat_hash_set<std::string> go_tls_probed_binaries_;
  absl::flat_hash_set<std::string> nodejs_binaries_;
  absl::flat_hash_set<std::string> grpc_c_probed_binaries_;

  // BPF maps through which the addresses of symbols for a given pid are communicated to uprobes.
  std::unique_ptr<MapT<ssl_source_t>> openssl_source_map_;
  std::unique_ptr<MapT<struct openssl_symaddrs_t>> openssl_symaddrs_map_;
  std::unique_ptr<MapT<struct go_common_symaddrs_t>> go_common_symaddrs_map_;
  std::unique_ptr<MapT<struct go_http2_symaddrs_t>> go_http2_symaddrs_map_;
  std::unique_ptr<MapT<struct go_tls_symaddrs_t>> go_tls_symaddrs_map_;
  std::unique_ptr<MapT<struct node_tlswrap_symaddrs_t>> node_tlswrap_symaddrs_map_;
  // Key is python gRPC module's md5 hash, value is the corresponding version enum's numeric value.
  std::unique_ptr<MapT<uint64_t>> grpc_c_versions_map_;

  const system::Config& syscfg_ = system::Config::GetInstance();
  StirlingMonitor& monitor_ = *StirlingMonitor::GetInstance();
};

}  // namespace stirling
}  // namespace px
