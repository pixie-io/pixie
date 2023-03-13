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

#include <benchmark/benchmark.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <google/protobuf/util/json_util.h>

#include "src/stirling/benchmarks/proto_data_encoding/common/data_gen.h"
#include "src/stirling/benchmarks/proto_data_encoding/dns/dns_record.h"
#include "src/stirling/benchmarks/proto_data_encoding/kafka/kafka_record.h"

namespace px {
using DNSRecordPb = ::experimental::DNSRecord;
using DNSRecordJson = ::px::stirling::protocols::dns::Record;
using KafkaRecordPb = ::experimental::KafkaProduceRecord;
using KafkaRecordJson = ::px::stirling::protocols::kafka::Record;

using ::px::Status;

template <typename RecordType>
// NOLINTNEXTLINE : runtime/references.
static void BM_serialize_pb(benchmark::State& state) {
  RecordType* record_pb;

  double total_bytes = 0;
  for (auto _ : state) {
    PX_ASSIGN_OR_EXIT(record_pb, experimental::InitRecordPb<RecordType>());
    PX_ASSIGN_OR_EXIT(size_t size, experimental::GetRecordSizePb(record_pb));
    total_bytes += size;

    record_pb->release_req();
    record_pb->release_resp();
  }

  state.counters["size"] = total_bytes / state.iterations();
}

template <typename RecordType>
// NOLINTNEXTLINE : runtime/references.
static void BM_serialize_json(benchmark::State& state) {
  RecordType* record;

  double total_bytes = 0;
  for (auto _ : state) {
    PX_ASSIGN_OR_EXIT(record, experimental::InitRecordJson<RecordType>());
    PX_ASSIGN_OR_EXIT(size_t size, experimental::GetRecordSizeJson(record));

    total_bytes += size;
    delete record;
  }

  state.counters["size"] = total_bytes / state.iterations();
}

BENCHMARK_TEMPLATE(BM_serialize_pb, DNSRecordPb);
BENCHMARK_TEMPLATE(BM_serialize_json, DNSRecordJson);
BENCHMARK_TEMPLATE(BM_serialize_pb, KafkaRecordPb);
BENCHMARK_TEMPLATE(BM_serialize_json, KafkaRecordJson);

}  // namespace px
