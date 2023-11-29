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

#include <unordered_set>
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"

DECLARE_bool(stirling_enable_lazy_parsing);

namespace px {

namespace stirling {

// Define the set of protocols which currently support lazy parsing
const std::unordered_set<traffic_protocol_t> kLazyParsingProtocols = {
    kProtocolHTTP,
    // kProtocolHTTP2,
    // kProtocolCQL,
    // kProtocolMySQL,
    // kProtocolPGSQL,
    // kProtocolDNS,
    // kProtocolRedis,
    // kProtocolNATS,
    // kProtocolKafka,
    // kProtocolMux,
    // kProtocolMongo,
    // kProtocolAMQP,
};

/**
 * Check if lazy parsing is enabled and supported for the given protocol.
 * @param protocol The protocol to check
 *
 * @return true if lazy parsing is enabled and supported for the given protocol.
 */
inline bool LazyParsingEnabled(traffic_protocol_t protocol) {
  return FLAGS_stirling_enable_lazy_parsing &&
         (kLazyParsingProtocols.find(protocol) != kLazyParsingProtocols.end());
}

}  // namespace stirling
}  // namespace px
