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

#include <benchmark/benchmark.h>

#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"
#include "src/stirling/obj_tools/dwarf_reader.h"

using px::stirling::obj_tools::DwarfReader;
using px::testing::BazelRunfilePath;

constexpr std::string_view kBinary =
    "src/stirling/testing/demo_apps/go_grpc_tls_pl/server/golang_1_19_grpc_tls_server_binary_/"
    "golang_1_19_grpc_tls_server_binary";

struct SymAddrs {
  // Members of net/http.http2serverConn.
  int32_t http2serverConn_conn_offset;
  int32_t http2serverConn_hpackEncoder_offset;

  // Members of net/http.http2HeadersFrame
  int32_t http2HeadersFrame_http2FrameHeader_offset;

  // Members of net/http.http2FrameHeader.
  int32_t http2FrameHeader_Flags_offset;
  int32_t http2FrameHeader_StreamID_offset;

  // Members of net/http.http2writeResHeaders.
  int32_t http2writeResHeaders_streamID_offset;
  int32_t http2writeResHeaders_endStream_offset;

  // Members of net/http.http2MetaHeadersFrame.
  int32_t http2MetaHeadersFrame_http2HeadersFrame_offset;
  int32_t http2MetaHeadersFrame_Fields_offset;
};

void GetSymAddrs(DwarfReader* dwarf_reader, SymAddrs* symaddrs) {
#define GET_SYMADDR(symaddr, type, member) \
  symaddr = dwarf_reader->GetStructMemberOffset(type, member).ValueOr(-1);

  GET_SYMADDR(symaddrs->http2serverConn_conn_offset, "net/http.http2serverConn", "conn");
  GET_SYMADDR(symaddrs->http2serverConn_hpackEncoder_offset, "net/http.http2serverConn",
              "hpackEncoder");
  GET_SYMADDR(symaddrs->http2HeadersFrame_http2FrameHeader_offset, "net/http.http2HeadersFrame",
              "http2FrameHeader");
  GET_SYMADDR(symaddrs->http2FrameHeader_Flags_offset, "net/http.http2FrameHeader", "Flags");
  GET_SYMADDR(symaddrs->http2FrameHeader_StreamID_offset, "net/http.http2FrameHeader", "StreamID");
  GET_SYMADDR(symaddrs->http2writeResHeaders_streamID_offset, "net/http.http2writeResHeaders",
              "streamID");
  GET_SYMADDR(symaddrs->http2writeResHeaders_endStream_offset, "net/http.http2writeResHeaders",
              "endStream");
  GET_SYMADDR(symaddrs->http2MetaHeadersFrame_http2HeadersFrame_offset,
              "net/http.http2MetaHeadersFrame", "http2HeadersFrame");
  GET_SYMADDR(symaddrs->http2MetaHeadersFrame_Fields_offset, "net/http.http2MetaHeadersFrame",
              "Fields");
}

// NOLINTNEXTLINE : runtime/references.
static void BM_noindex(benchmark::State& state) {
  size_t num_lookup_iterations = state.range(0);

  for (auto _ : state) {
    SymAddrs symaddrs;

    PX_ASSIGN_OR_EXIT(std::unique_ptr<DwarfReader> dwarf_reader,
                      DwarfReader::CreateWithoutIndexing(kBinary));

    for (size_t i = 0; i < num_lookup_iterations; ++i) {
      GetSymAddrs(dwarf_reader.get(), &symaddrs);
      benchmark::DoNotOptimize(symaddrs);
    }
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_indexed(benchmark::State& state) {
  size_t num_lookup_iterations = state.range(0);

  for (auto _ : state) {
    SymAddrs symaddrs;

    PX_ASSIGN_OR_EXIT(std::unique_ptr<DwarfReader> dwarf_reader,
                      DwarfReader::CreateIndexingAll(kBinary));

    for (size_t i = 0; i < num_lookup_iterations; ++i) {
      GetSymAddrs(dwarf_reader.get(), &symaddrs);
      benchmark::DoNotOptimize(symaddrs);
    }
  }
}

BENCHMARK(BM_noindex)->RangeMultiplier(2)->Range(1, 16);
BENCHMARK(BM_indexed)->RangeMultiplier(2)->Range(1, 16);
