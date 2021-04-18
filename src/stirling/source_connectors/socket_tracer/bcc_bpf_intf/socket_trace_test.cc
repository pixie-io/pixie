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

#include <gtest/gtest.h>

// Needed for integer types used in socket_trace.h.
// We cannot include stdint.h inside socket_trace.h, as that conflicts with BCC's headers.
#include <cstdint>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"

// Inside BPF, socket_data_event_t object is submitted as a bytes array, with a size calculated as
// sizeof(socket_data_event_t::attr_t) + <submitted msg size>
// We have to make sure that the follow condition holds, otherwise, the userspace code cannot
// accurately read data at the right boundary.
TEST(SocketDataEventTTest, VerifyAlignment) {
  socket_data_event_t event;
  EXPECT_EQ(0, offsetof(socket_data_event_t, attr));
  EXPECT_EQ(sizeof(event.attr), offsetof(socket_data_event_t, msg));
}
