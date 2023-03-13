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

#include "src/stirling/benchmarks/proto_data_encoding/dns/dns_record.h"
#include <string>

namespace experimental {

using DNSRecordPb = ::experimental::DNSRecord;
using DNSHeaderPb = ::experimental::DNSHeader;
using DNSRequestPb = ::experimental::DNSRequest;
using DNSResponsePb = ::experimental::DNSResponse;

using DNSRecordJson = ::px::stirling::protocols::dns::Record;

inline static const std::string kHeader =
    "{\"txid\":4433,\"qr\":0,\"opcode\":0,\"aa\":0,\"tc\":0,\"rd\":1,\"ra\":0,\"ad\":0,\"cd\":0,"
    "\"rcode\":0,\"num_queries\":1,\"num_answers\":0,\"num_auth\":0,\"num_addl\":0}";
inline static const std::string kReqQuery =
    "{\"queries\":[{\"name\":\"vizier-cloud-connector-svc.pl.svc\",\"type\":\"AAAA\"}]}";
inline static const std::string kRespMsg =
    "{\"answers\":[{\"name\":\"vizier-metadata-svc.pl.svc.cluster.local\",\"type\":\"A\",\"addr\":"
    "\"10.162.154.179\"}]}";

// inline static const std::string kRespMsg = "{\"answers\":[]}";

template <>
px::StatusOr<DNSRecordPb*> InitRecordPb() {
  auto header = new DNSHeaderPb();
  header->set_txid(29452);
  header->set_flags(0);
  header->set_num_queries(1);
  header->set_num_answers(0);
  header->set_num_auth(0);
  header->set_num_addl(0);

  auto req = new DNSRequestPb();
  req->set_allocated_header(header);
  auto query_ptr = req->add_queries();
  query_ptr->set_name("vizier-cloud-connector-svc.pl.svc");
  query_ptr->set_type("AAAA");
  req->set_time_stamp_ns(22985671373804481);

  auto resp = new DNSResponsePb();
  resp->set_allocated_header(header);
  auto answer_ptr = resp->add_answers();
  answer_ptr->set_name("vizier-metadata-svc.pl.svc.cluster.local");
  answer_ptr->set_type("AAAA");
  answer_ptr->set_addr("10.162.154.179");
  resp->set_time_stamp_ns(2298567137414149);

  auto record = new DNSRecordPb();
  record->set_allocated_req(req);
  record->set_allocated_resp(resp);

  return record;
}

template <>
px::StatusOr<DNSRecordJson*> InitRecordJson() {
  auto record = new DNSRecordJson();

  record->req.header = kHeader;
  record->req.query = kReqQuery;
  record->req.timestamp_ns = 22985671373804481;

  record->resp.header = kHeader;
  record->resp.msg = kRespMsg;

  return record;
}

template <>
px::StatusOr<size_t> GetRecordSizeJson(const DNSRecordJson* record) {
  return kFixedColumnSize + record->req.header.size() + record->req.query.size() +
         record->resp.header.size() + record->resp.msg.size();
}

}  // namespace experimental
