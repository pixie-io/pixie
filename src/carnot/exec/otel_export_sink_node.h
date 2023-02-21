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

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

struct SpanConfig {
  std::string name;
};

class OTelExportSinkNode : public SinkNode {
 public:
  virtual ~OTelExportSinkNode() = default;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  Status ConsumeMetrics(ExecState* exec_state, const table_store::schema::RowBatch& rb);
  Status ConsumeSpans(ExecState* exec_state, const table_store::schema::RowBatch& rb);

  std::unique_ptr<table_store::schema::RowDescriptor> input_descriptor_;
  opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse metrics_response_;
  opentelemetry::proto::collector::metrics::v1::MetricsService::StubInterface*
      metrics_service_stub_;
  opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse trace_response_;
  opentelemetry::proto::collector::trace::v1::TraceService::StubInterface* trace_service_stub_;
  std::unique_ptr<plan::OTelExportSinkOperator> plan_node_;

  std::unique_ptr<SpanConfig> span_config_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
