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

#include <dlfcn.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"

namespace px {
namespace stirling {

using obj_tools::DwarfReader;
using obj_tools::ElfReader;

class UprobeSymaddrsTest : public ::testing::Test {
 protected:
  void SetUp() {
    std::filesystem::path p = px::testing::BazelRunfilePath(kGoGRPCServer);
    ASSERT_OK_AND_ASSIGN(dwarf_reader_, DwarfReader::CreateIndexingAll(p));
    ASSERT_OK_AND_ASSIGN(elf_reader_, ElfReader::Create(p));
  }

  static inline constexpr std::string_view kGoGRPCServer =
      "src/stirling/testing/demo_apps/go_grpc_tls_pl/server/golang_1_19_grpc_tls_server_binary";

  std::unique_ptr<DwarfReader> dwarf_reader_;
  std::unique_ptr<ElfReader> elf_reader_;
};

TEST_F(UprobeSymaddrsTest, GoCommonSymAddrs) {
  ASSERT_OK_AND_ASSIGN(struct go_common_symaddrs_t symaddrs,
                       GoCommonSymAddrs(elf_reader_.get(), dwarf_reader_.get()));

  // Check a few interface types.
  EXPECT_NE(symaddrs.tls_Conn, -1);
  EXPECT_NE(symaddrs.tls_Conn, 0);

  // Check some member offsets.
  // The values may change when golang version is updated.
  // If the test breaks because of that, just update the numbers here.
  EXPECT_EQ(symaddrs.FD_Sysfd_offset, 16);
  EXPECT_EQ(symaddrs.tlsConn_conn_offset, 0);
  EXPECT_EQ(symaddrs.g_goid_offset, 152);
}

TEST_F(UprobeSymaddrsTest, GoHTTP2SymAddrs) {
  ASSERT_OK_AND_ASSIGN(struct go_http2_symaddrs_t symaddrs,
                       GoHTTP2SymAddrs(elf_reader_.get(), dwarf_reader_.get()));

  // Check a few interface types.
  EXPECT_NE(symaddrs.transport_bufWriter, -1);
  EXPECT_NE(symaddrs.transport_bufWriter, 0);

  // Check some member offsets.
  // The values may change when golang version is updated.
  // If the test breaks because of that, just update the numbers here.
  EXPECT_EQ(symaddrs.http2Framer_WriteDataPadded_f_loc,
            (location_t{.type = kLocationTypeRegisters, .offset = 0}));
  EXPECT_EQ(symaddrs.writeHeader_hf_ptr_loc,
            (location_t{.type = kLocationTypeRegisters, .offset = 24}));
}

TEST_F(UprobeSymaddrsTest, GoTLSSymAddrs) {
  ASSERT_OK_AND_ASSIGN(struct go_tls_symaddrs_t symaddrs,
                       GoTLSSymAddrs(elf_reader_.get(), dwarf_reader_.get()));

  // Check some member offsets.
  // The values may change when golang version is updated.
  // If the test breaks because of that, just update the numbers here.
  EXPECT_EQ(symaddrs.Write_c_loc, (location_t{.type = kLocationTypeRegisters, .offset = 0}));
  EXPECT_EQ(symaddrs.Write_b_loc, (location_t{.type = kLocationTypeRegisters, .offset = 8}));
  EXPECT_EQ(symaddrs.Read_c_loc, (location_t{.type = kLocationTypeRegisters, .offset = 0}));
  EXPECT_EQ(symaddrs.Read_b_loc, (location_t{.type = kLocationTypeRegisters, .offset = 8}));
}

// Note that DwarfReader cannot be created if there is no dwarf info.
TEST(UprobeSymaddrsNodeTest, TLSWrapSymAddrsFromDwarfInfo) {
  std::filesystem::path p =
      px::testing::BazelRunfilePath("src/stirling/testing/demo_apps/node/node_debug/node_debug");
  ASSERT_OK_AND_ASSIGN(struct node_tlswrap_symaddrs_t symaddrs, NodeTLSWrapSymAddrs(p, {0, 0, 0}));
  EXPECT_EQ(symaddrs.TLSWrap_StreamListener_offset, 0x08);
  EXPECT_EQ(symaddrs.StreamListener_stream_offset, 0x08);
  EXPECT_EQ(symaddrs.StreamBase_StreamResource_offset, 0x00);
  EXPECT_EQ(symaddrs.LibuvStreamWrap_StreamBase_offset, 0x00);
  EXPECT_EQ(symaddrs.LibuvStreamWrap_stream_offset, 0x08);
  EXPECT_EQ(symaddrs.uv_stream_s_io_watcher_offset, 0x00);
  EXPECT_EQ(symaddrs.uv__io_s_fd_offset, 0x04);
}

}  // namespace stirling
}  // namespace px
