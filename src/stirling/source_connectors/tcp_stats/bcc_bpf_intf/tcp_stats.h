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

#include "src/stirling/upid/upid.h"

#define MAX_CMD_SIZE 32

union sockaddress_t {
  struct sockaddr sa;
  struct sockaddr_in in4;
  struct sockaddr_in6 in6;
};

enum tcp_event_type_t {
  // Unknown Event.
  kUnknownEvent,

  // TCP egress data event.
  kTCPTx,

  // TCP ingress data event.
  kTCPRx,

  // TCP retransmissions.
  kTCPRetransmissions,
};

struct tcp_event_t {
  // The time when this was captured in the BPF time.
  uint64_t timestamp_ns;

  // The unique identifier of the process.
  struct upid_t upid;

  // IP address of the local endpoint.
  union sockaddress_t local_addr;

  // IP address of the remote endpoint.
  union sockaddress_t remote_addr;

  // Number of bytes.
  uint64_t size;

  // Source of event.
  enum tcp_event_type_t type;
};
