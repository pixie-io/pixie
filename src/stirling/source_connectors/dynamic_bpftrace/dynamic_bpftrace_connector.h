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

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/bpf_tools/bpftrace_wrapper.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"

namespace px {
namespace stirling {

class DynamicBPFTraceConnector : public SourceConnector {
 public:
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{100};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};

  static StatusOr<std::unique_ptr<SourceConnector> > Create(
      std::string_view source_name,
      const dynamic_tracing::ir::logical::TracepointDeployment::Tracepoint& tracepoint);

  DynamicBPFTraceConnector() = delete;
  ~DynamicBPFTraceConnector() override = default;

 protected:
  explicit DynamicBPFTraceConnector(std::string_view source_name,
                                    std::unique_ptr<DynamicDataTableSchema> table_schema,
                                    std::unique_ptr<bpf_tools::BPFTraceWrapper> bpftrace);
  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx) override;

 private:
  void HandleEvent(uint8_t* data);

  std::string name_;
  std::unique_ptr<DynamicDataTableSchema> table_schema_;
  std::unique_ptr<bpf_tools::BPFTraceWrapper> bpftrace_;

  // The types according to the BPFTrace printf format.
  std::vector<bpftrace::Field> output_fields_;

  // Used by HandleEvent so that when a callback is triggered, HandleEvent knows the context.
  DataTable* data_table_ = nullptr;
};

}  // namespace stirling
}  // namespace px
