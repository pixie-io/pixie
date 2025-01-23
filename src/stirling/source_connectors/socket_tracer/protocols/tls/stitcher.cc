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

#include "src/stirling/source_connectors/socket_tracer/protocols/tls/stitcher.h"

#include <utility>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/tls/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/tls/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace tls {

/* The TLS protocol is an in-order protocol. The handshake protocol specificially must
 * be sent in the expected order or it results in a fatal protocol error.
 *
 * TLS v1.2 handeshake. Taken from RFC5246 page 35
 *
 *       Client                                               Server
 *
 *     ClientHello                  -------->
 *                                                     ServerHello
 *                                                    Certificate*
 *                                              ServerKeyExchange*
 *                                             CertificateRequest*
 *                                  <--------      ServerHelloDone
 *     Certificate*
 *     ClientKeyExchange
 *     CertificateVerify*
 *     [ChangeCipherSpec]
 *     Finished                     -------->
 *                                              [ChangeCipherSpec]
 *                                  <--------             Finished
 *     Application Data             <------->     Application Data
 *
 * `*` Indicates optional or situation-dependent messages that are not
 * always sent.
 *
 * TLS v1.3 handshake. Taken from RFC8446 page 10
 *
 *        Client                                           Server
 *
 * Key  ^ ClientHello
 * Exch | + key_share*
 *      | + signature_algorithms*
 *      | + psk_key_exchange_modes*
 *      v + pre_shared_key*       -------->
 *                                                   ServerHello  ^ Key
 *                                                  + key_share*  | Exch
 *                                             + pre_shared_key*  v
 *                                         {EncryptedExtensions}  ^  Server
 *                                         {CertificateRequest*}  v  Params
 *                                                {Certificate*}  ^
 *                                          {CertificateVerify*}  | Auth
 *                                                    {Finished}  v
 *                                <--------  [Application Data*]
 *      ^ {Certificate*}
 * Auth | {CertificateVerify*}
 *      v {Finished}              -------->
 *        [Application Data]      <------->  [Application Data]
 *
 *               +  Indicates noteworthy extensions sent in the
 *                  previously noted message.
 *
 *               *  Indicates optional or situation-dependent
 *                  messages/extensions that are not always sent.
 *
 *               {} Indicates messages protected using keys
 *                  derived from a [sender]_handshake_traffic_secret.
 *
 *               [] Indicates messages protected using keys
 *                  derived from [sender]_application_traffic_secret_N.
 */
RecordsWithErrorCount<tls::Record> StitchFrames(std::deque<tls::Frame>* reqs,
                                                std::deque<tls::Frame>* resps) {
  std::vector<tls::Record> records;
  int error_count = 0;
  uint64_t latest_resp_ts = 0;

  for (auto& resp : *resps) {
    if (resp.timestamp_ns > latest_resp_ts) {
      latest_resp_ts = resp.timestamp_ns;
    }

    for (auto req_it = reqs->begin(); req_it != reqs->end(); req_it++) {
      auto& req = *req_it;

      if (req.consumed) continue;

      // For now only kHandshakes matter since that transmits the interesting data.
      // We will probably wnat to support kAlert as well.
      if (req.content_type == ContentType::kApplicationData ||
          req.content_type == ContentType::kChangeCipherSpec ||
          req.content_type == ContentType::kAlert || req.content_type == ContentType::kHeartbeat) {
        req.consumed = true;
        continue;
      }

      if (req.content_type == ContentType::kHandshake &&
          req.handshake_type == HandshakeType::kClientHello &&
          resp.content_type == ContentType::kHandshake &&
          resp.handshake_type == HandshakeType::kServerHello) {
        req.consumed = true;
        resp.consumed = true;
        records.push_back({std::move(req), std::move(resp)});
        break;
      }
    }
  }

  auto erase_until_iter = reqs->begin();
  while (erase_until_iter != reqs->end() &&
         (erase_until_iter->consumed || erase_until_iter->timestamp_ns < latest_resp_ts)) {
    if (!erase_until_iter->consumed) {
      error_count++;
    }
    erase_until_iter++;
  }
  reqs->erase(reqs->begin(), erase_until_iter);
  resps->clear();
  return {records, error_count};
}

}  // namespace tls
}  // namespace protocols
}  // namespace stirling
}  // namespace px
