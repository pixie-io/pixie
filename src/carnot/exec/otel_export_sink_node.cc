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

#include "src/carnot/exec/otel_export_sink_node.h"

#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "glog/logging.h"
#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/macros.h"
#include "src/common/uuid/uuid_utils.h"
#include "src/shared/types/typespb/types.pb.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string OTelExportSinkNode::DebugStringImpl() {
  return absl::Substitute("Exec::OTelExportSinkNode: $0", plan_node_->DebugString());
}

Status OTelExportSinkNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::OTEL_EXPORT_SINK_OPERATOR);
  if (input_descriptors_.size() != 1) {
    return error::InvalidArgument("OTel Export operator expects a single input relation, got $0",
                                  input_descriptors_.size());
  }

  input_descriptor_ = std::make_unique<RowDescriptor>(input_descriptors_[0]);
  const auto* sink_plan_node = static_cast<const plan::OTelExportSinkOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::OTelExportSinkOperator>(*sink_plan_node);
  return Status::OK();
}

Status OTelExportSinkNode::PrepareImpl(ExecState*) { return Status::OK(); }

Status OTelExportSinkNode::OpenImpl(ExecState* exec_state) {
  metrics_service_stub_ = exec_state->MetricsServiceStub(plan_node_->url());
  return Status::OK();
}

Status OTelExportSinkNode::CloseImpl(ExecState* exec_state) {
  if (sent_eos_) {
    return Status::OK();
  }

  LOG(INFO) << absl::Substitute("Closing OTelExportSinkNode $0 in query $1 before receiving EOS",
                                plan_node_->id(), exec_state->query_id().str());

  return Status::OK();
}

void AddAttributes(google::protobuf::RepeatedPtrField<
                       ::opentelemetry::proto::common::v1::KeyValue>* mutable_attributes,
                   const google::protobuf::RepeatedPtrField<planpb::OTelAttribute>& px_attributes,
                   const RowBatch& rb, int64_t row_idx) {
  for (const auto& px_attr : px_attributes) {
    auto otel_attr = mutable_attributes->Add();
    otel_attr->set_key(px_attr.name());
    auto attribute_col = rb.ColumnAt(px_attr.column().column_index()).get();
    otel_attr->mutable_value()->set_string_value(
        types::GetValueFromArrowArray<types::STRING>(attribute_col, row_idx));
  }
}

Status OTelExportSinkNode::ConsumeMetric(const RowBatch& rb) {
  grpc::ClientContext context;
  for (const auto& header : plan_node_->endpoint_headers()) {
    context.AddMetadata(header.first, header.second);
  }
  context.set_compression_algorithm(GRPC_COMPRESS_GZIP);

  metrics_response_.Clear();
  opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest request;

  const auto& resource_pb = plan_node_->resource();
  for (int64_t row_idx = 0; row_idx < rb.ColumnAt(0)->length(); ++row_idx) {
    auto resource_metrics = request.add_resource_metrics();
    auto resource = resource_metrics->mutable_resource();
    AddAttributes(resource->mutable_attributes(), resource_pb.attributes(), rb, row_idx);
    // TODO(philkuz) optimize by pooling metrics by resource within a batch.
    // TODO(philkuz) optimize by pooling data per metric per resource.

    auto library_metrics = resource_metrics->add_instrumentation_library_metrics();
    for (const auto& metric_pb : plan_node_->metrics()) {
      auto metric = library_metrics->add_metrics();
      metric->set_name(metric_pb.name());
      metric->set_description(metric_pb.description());
      metric->set_unit(metric_pb.unit());

      if (metric_pb.has_summary()) {
        auto summary = metric->mutable_summary();
        auto data_point = summary->add_data_points();
        AddAttributes(data_point->mutable_attributes(), metric_pb.attributes(), rb, row_idx);

        auto time_col = rb.ColumnAt(metric_pb.time_column_index()).get();
        data_point->set_time_unix_nano(
            types::GetValueFromArrowArray<types::TIME64NS>(time_col, row_idx));

        auto count_col = rb.ColumnAt(metric_pb.summary().count_column_index()).get();
        data_point->set_count(types::GetValueFromArrowArray<types::INT64>(count_col, row_idx));

        // The summary column is optional. It's not set if index < 0.
        if (metric_pb.summary().sum_column_index() >= 0) {
          auto sum_col = rb.ColumnAt(metric_pb.summary().sum_column_index()).get();
          data_point->set_sum(types::GetValueFromArrowArray<types::FLOAT64>(sum_col, row_idx));
        }

        for (const auto& px_qv : metric_pb.summary().quantile_values()) {
          auto qv = data_point->add_quantile_values();
          qv->set_quantile(px_qv.quantile());
          auto qv_col = rb.ColumnAt(px_qv.value_column_index()).get();
          qv->set_value(types::GetValueFromArrowArray<types::FLOAT64>(qv_col, row_idx));
        }
      } else if (metric_pb.has_gauge()) {
        auto gauge = metric->mutable_gauge();
        auto data_point = gauge->add_data_points();
        AddAttributes(data_point->mutable_attributes(), metric_pb.attributes(), rb, row_idx);

        auto time_col = rb.ColumnAt(metric_pb.time_column_index()).get();
        data_point->set_time_unix_nano(
            types::GetValueFromArrowArray<types::TIME64NS>(time_col, row_idx));
        if (metric_pb.gauge().has_float_column_index()) {
          auto double_col = rb.ColumnAt(metric_pb.gauge().float_column_index()).get();
          data_point->set_as_double(
              types::GetValueFromArrowArray<types::FLOAT64>(double_col, row_idx));
        } else {
          auto int_col = rb.ColumnAt(metric_pb.gauge().int_column_index()).get();
          data_point->set_as_int(types::GetValueFromArrowArray<types::INT64>(int_col, row_idx));
        }
      }
    }
  }

  grpc::Status status = metrics_service_stub_->Export(&context, request, &metrics_response_);
  if (!status.ok()) {
    return error::Internal(absl::Substitute(
        "OTelExportSinkNode $0 encountered error code $1 exporting data, message: $2 $3",
        plan_node_->id(), status.error_code(), status.error_message(), status.error_details()));
  }
  if (rb.eos()) {
    sent_eos_ = true;
  }
  return Status::OK();
}

Status OTelExportSinkNode::ConsumeNextImpl(ExecState*, const RowBatch& rb, size_t) {
  if (plan_node_->metrics().size()) {
    PL_RETURN_IF_ERROR(ConsumeMetric(rb));
  }
  if (plan_node_->spans().size()) {
    return error::Unimplemented("OTelExportSink only works for Metrics");
  }
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
