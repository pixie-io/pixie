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

#include "src/stirling/source_connectors/socket_tracer/protocols/dns/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace dns {

TEST(DNSTypesTest, ByteSize) {
  Frame f = {};

  DNSRecord r1{"px.dev", "px_cname", {}};
  DNSRecord r2{"aloooooooooooooooooooooooongname", "", {}};
  f.AddRecords({r1, r2});

  auto s = std::string_view("px.dev").size() + std::string_view("px_cname").size() +
           sizeof(DNSRecord::addr) + std::string_view("aloooooooooooooooooooooooongname").size() +
           std::string_view("").size() + sizeof(DNSRecord::addr);

  EXPECT_EQ(f.ByteSize(), s + sizeof(f));
}

}  // namespace dns
}  // namespace protocols
}  // namespace stirling
}  // namespace px
