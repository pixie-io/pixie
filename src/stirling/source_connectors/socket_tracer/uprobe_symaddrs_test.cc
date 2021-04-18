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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/obj_tools/dwarf_tools.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"

namespace px {
namespace stirling {

using obj_tools::DwarfReader;
using obj_tools::ElfReader;

class UprobeSymaddrsTest : public ::testing::Test {
 protected:
  void SetUp() {
    std::filesystem::path p = px::testing::BazelBinTestFilePath(kGoGRPCServer);
    ASSERT_OK_AND_ASSIGN(dwarf_reader_, DwarfReader::Create(p.string()));
    ASSERT_OK_AND_ASSIGN(elf_reader_, ElfReader::Create(p));
  }

  static inline constexpr std::string_view kGoGRPCServer =
      "src/stirling/testing/demo_apps/go_grpc_tls_pl/server/server_/server";

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
  EXPECT_EQ(symaddrs.http2Framer_WriteDataPadded_f_offset, 8);
  EXPECT_EQ(symaddrs.writeHeader_hf_offset, 24);
}

TEST_F(UprobeSymaddrsTest, GoTLSSymAddrs) {
  ASSERT_OK_AND_ASSIGN(struct go_tls_symaddrs_t symaddrs,
                       GoTLSSymAddrs(elf_reader_.get(), dwarf_reader_.get()));

  // Check some member offsets.
  // The values may change when golang version is updated.
  // If the test breaks because of that, just update the numbers here.
  EXPECT_EQ(symaddrs.Write_c_offset, 8);
  EXPECT_EQ(symaddrs.Write_b_offset, 16);
  EXPECT_EQ(symaddrs.Read_c_offset, 8);
  EXPECT_EQ(symaddrs.Read_b_offset, 16);
}

}  // namespace stirling
}  // namespace px
