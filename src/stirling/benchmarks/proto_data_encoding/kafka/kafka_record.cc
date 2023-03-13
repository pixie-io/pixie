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

#include "src/stirling/benchmarks/proto_data_encoding/kafka/kafka_record.h"
#include <string>

namespace experimental {
using KafkaRecordJson = ::px::stirling::protocols::kafka::Record;
using KafkaRecordPb = ::experimental::KafkaProduceRecord;

inline static const std::string kProduceReqMsg =
    "{\"transactional_id\":\"\",\"acks\":1,\"timeout_ms\":30000,\"topics\":[{\"name\":\"order\","
    "\"partitions\":[{\"index\":2,\"message_set\":{\"size\":475}}]}]}";
inline static const std::string kProduceRespMsg =
    "{\"topics\":[{\"name\":\"order\",\"partitions\":[{\"index\":2,\"error_code\":0,\"base_"
    "offset\":50928,\"log_append_time_ms\":-1,\"log_start_offset\":0,\"record_errors\":[],\"error_"
    "message\":\"\"}]}],\"throttle_time_ms\":0}";

template <>
px::StatusOr<KafkaRecordPb*> InitRecordPb() {
  auto req = new KafkaProduceReq();
  req->set_transactional_id("");
  req->set_acks(1);
  req->set_timeout_ms(30000);
  auto req_topic_ptr = req->add_topics();
  req_topic_ptr->set_name("order");
  auto req_partition_ptr = req_topic_ptr->add_partitions();
  req_partition_ptr->set_index(2);
  auto message_set = new MessageSet();
  message_set->set_size(475);
  req_partition_ptr->set_allocated_message_set(message_set);

  auto resp = new KafkaProduceResp();
  auto resp_topic_ptr = resp->add_topics();
  resp_topic_ptr->set_name("order");
  auto resp_partition_ptr = resp_topic_ptr->add_partitions();
  resp_partition_ptr->set_index(4);
  resp_partition_ptr->set_error_code(0);
  resp_partition_ptr->set_base_offset(427942);
  resp_partition_ptr->set_log_append_time_ms(-1);
  resp_partition_ptr->set_log_start_offset(236896);
  resp_partition_ptr->set_error_message("");
  resp->set_throttle_time_ms(0);

  auto record = new KafkaProduceRecord();
  record->set_allocated_req(req);
  record->set_allocated_resp(resp);

  return record;
}

template <>
px::StatusOr<KafkaRecordJson*> InitRecordJson() {
  auto record = new KafkaRecordJson();

  record->req.api_key = ::px::stirling::protocols::kafka::APIKey::kProduce;
  record->req.api_version = 8;
  record->req.client_id = "producer-1";
  record->req.msg = kProduceReqMsg;
  record->req.timestamp_ns = 23240366589457368;

  record->resp.msg = kProduceRespMsg;
  record->resp.timestamp_ns = 23240366594637363;

  return record;
}

constexpr size_t kSizeAPIKey = 2;
constexpr std::string_view client_id = "producer-1";

template <>
px::StatusOr<size_t> GetRecordSizeJson(const KafkaRecordJson* record) {
  return kFixedColumnSize + kSizeAPIKey + client_id.size() + record->req.msg.size() +
         record->resp.msg.size();
}

template <>
px::StatusOr<size_t> GetRecordSizePb(const KafkaRecordPb* record) {
  return kFixedColumnSize + kSizeAPIKey + client_id.size() + record->ByteSizeLong();
}

}  // namespace experimental
