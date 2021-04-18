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

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/message.h>

#include <string>

#include "src/benchmarks/proto/benchmark.pb.h"
#include "src/common/grpcutils/service_descriptor_database.h"

const size_t kRangeMultiplier = 10;
const size_t kRangeBegin = 1'000;
const size_t kRangeEnd = 10'000'000;

using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::Message;
using ::px::benchmarks::ChargeRequest;
using ::px::grpc::MethodInputOutput;
using ::px::grpc::ServiceDescriptorDatabase;

static ChargeRequest SampleChargeRequest() {
  ChargeRequest r;
  r.mutable_amount()->set_currency_code("USD");
  r.mutable_amount()->set_units(1000);
  r.mutable_amount()->set_nanos(1'000'000);
  r.mutable_credit_card()->set_credit_card_number("1234 1234 1234 1234");
  r.mutable_credit_card()->set_credit_card_cvv(123);
  r.mutable_credit_card()->set_credit_card_expiration_year(2022);
  r.mutable_credit_card()->set_credit_card_expiration_month(12);
  return r;
}

// NOLINTNEXTLINE : runtime/references.
static void BM_dynamic_message_parsing(benchmark::State& state) {
  const ChargeRequest sample_charge_req = SampleChargeRequest();
  const std::string serialized_charge_req = sample_charge_req.SerializeAsString();

  FileDescriptorSet fd_set;
  ChargeRequest::descriptor()->file()->CopyTo(fd_set.add_file());
  ServiceDescriptorDatabase db(fd_set);
  MethodInputOutput in_out = db.GetMethodInputOutput("px.benchmarks.PaymentService.Charge");

  for (auto _ : state) {
    for (size_t i = 0; i < static_cast<size_t>(state.range(0)); ++i) {
      std::unique_ptr<Message> t(in_out.input->New());
      t->ParseFromString(serialized_charge_req);
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// NOLINTNEXTLINE : runtime/references.
static void BM_compiled_message_parsing(benchmark::State& state) {
  const ChargeRequest sample_charge_req = SampleChargeRequest();
  const std::string serialized_charge_req = sample_charge_req.SerializeAsString();

  for (auto _ : state) {
    for (size_t i = 0; i < static_cast<size_t>(state.range(0)); ++i) {
      ChargeRequest t;
      t.ParseFromString(serialized_charge_req);
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(BM_dynamic_message_parsing)
    ->RangeMultiplier(kRangeMultiplier)
    ->Range(kRangeBegin, kRangeEnd);
BENCHMARK(BM_compiled_message_parsing)
    ->RangeMultiplier(kRangeMultiplier)
    ->Range(kRangeBegin, kRangeEnd);
