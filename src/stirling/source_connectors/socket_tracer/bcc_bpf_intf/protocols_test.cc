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

#include <absl/strings/str_replace.h>
#include <gtest/gtest.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"

#include "src/shared/protocols/protocols.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"

// We have two definitions of protocols that need to be kept in sync.
// Ideally there would be a single source, but one of them is used inside BPF code,
// which makes it a bit annoying.
// For the moment, this test ensures that the two defintions are consistent.
// TODO(oazizi): Find a clean way of importing the //src/shared defintion into the BPF code.
TEST(ProtocolDefinitionConsistency, Verify) {
  auto px_protocols = magic_enum::enum_values<px::protocols::Protocol>();
  auto bpf_protocols = magic_enum::enum_values<traffic_protocol_t>();

  ASSERT_EQ(px_protocols.size(), bpf_protocols.size());

  for (size_t i = 0; i < px_protocols.size(); ++i) {
    std::string px_protocol_name(magic_enum::enum_name(px_protocols[i]));
    std::string bpf_protocol_name(magic_enum::enum_name(bpf_protocols[i]));

    // Names should match with the exception that the prefixes differ (kHTTP vs kProtocolHTTP).
    EXPECT_EQ(px_protocol_name, absl::StrReplaceAll(bpf_protocol_name, {{"kProtocol", "k"}}));
  }
}
