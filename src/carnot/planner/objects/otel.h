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
#include <absl/container/flat_hash_set.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/proto/trace/v1/trace.pb.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planpb/plan.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
class OTelModule : public QLObject {
 public:
  inline static constexpr char kOTelModule[] = "otel";
  static constexpr TypeDescriptor OTelModuleType = {
      /* name */ kOTelModule,
      /* type */ QLObjectType::kModule,
  };
  static StatusOr<std::shared_ptr<OTelModule>> Create(CompilerState* compiler_state,
                                                      ASTVisitor* ast_visitor, IR* ir);

  inline static constexpr char kDataOpID[] = "Data";
  inline static constexpr char kDataOpDocstring[] = R"doc(
  Specifies the transformation of a DataFrame into OpenTelemetry data.

  This function makes it easy to transform columnar DataFrame data into
  single element OpenTelemetry data. User must pass in data configuration(s)
  (ie `px.otel.metric.Gauge`) and a resource config where `service.name` is
  one of the attributes.

  Optionally, users can specify an endpoint using `px.otel.Endpoint`.

  :topic: otel

  Args:
    data (List[px.otel.Data]): The list of OTel configurations for the data, describing how the
      data is created in the DataFrame and what kind of metric to populate in OTel.
    resource (Dict[string, Column|string]): A description of the resource that creates
      this data. Is a mapping of attribute name to a string or a column that stores data about
      the attribute. Must include 'service.name' as one attribute.
    endpoint (px.otel.Endpoint, optional): The endpoint configuration value. The endpoint
      can be omitted if this script is run in the OTel plugin context where an endpoint
      is configured.
  Returns:
    Exporter: the description of how to map a DataFrame to OpenTelemetry Data. Can be passed
      into `px.export`.
  )doc";

  inline static constexpr char kEndpointOpID[] = "Endpoint";
  inline static constexpr char kEndpointOpDocstring[] = R"doc(
  The Collector destination for exported OpenTelemetry data.

  Describes the endpoint and any connection arguments necessary to talk to
  an OpenTelemetry collector. Passed as an argument to `px.otel.Data`.

  :topic: otel

  Args:
    url (string): The URL of the OTel collector.
    headers (Dict[string,string], optional): The connection metadata to add to the
      header of the request.
    insecure (bool, optional): Whether to allow insecure connections to the OpenTelemetry
      collector. False by default.
    timeout (int, optional): The number of seconds before the request should timeout when exporting to the OTel collector.
  )doc";

 protected:
  explicit OTelModule(ASTVisitor* ast_visitor) : QLObject(OTelModuleType, ast_visitor) {}
  Status Init(CompilerState* compiler_state, IR* ir);
};

class OTelMetrics : public QLObject {
 public:
  inline static constexpr char kOTelMetricsModule[] = "metric";
  static constexpr TypeDescriptor OTelMetricsModuleType = {
      /* name */ kOTelMetricsModule,
      /* type */ QLObjectType::kModule,
  };
  static StatusOr<std::shared_ptr<OTelMetrics>> Create(ASTVisitor* ast_visitor, IR* graph);

  inline static constexpr char kGaugeOpID[] = "Gauge";
  inline static constexpr char kGaugeOpDocstring[] = R"doc(
  Defines the DataFrame mapping to an OpenTelemetry Metric Gauge

  [Gauges](https://opentelemetry.io/docs/reference/specification/metrics/datamodel/#gauge)
  specify the "current value" of a metric at a given time. The source DataFrame must
  have a `time_` column of type `TIME64NS` or the compiler will throw an error.

  :topic: otel

  Args:
    name (string): The name of the metric. Must adhere to [OpenTelemetry's naming conventions](https://opentelemetry.io/docs/reference/specification/metrics/api/#instrument)
    value (Column): The column that contains the data. Must be either an INT64 or a FLOAT64.
    description (string, optional): A description of what the metric tracks.
    attributes (Dict[string, Column|string], optional): A mapping of attribute name to a string or the column
      that stores data about the attribute.
    unit (string, optional): The unit string to use for the metric. If not specified, will attempt
      to use the Semantic Type of the `value` to infer the unit string.
  Returns:
    OTelDataContainer: the mapping of DataFrame columns to OpenTelemetry Gauge fields. Can be passed
      into `px.otel.Data()` as the data argument.
  )doc";

  inline static constexpr char kSummaryOpID[] = "Summary";
  inline static constexpr char kSummaryOpDocstring[] = R"doc(
  Defines the DataFrame mapping to an OpenTelemetry Metric Summary

  Summaries describe distributions by recording quantile values (aka percentile values) such as p50, p90 and p99.
  User specify as many of these point values as they like, labeling them with the float equivalent position in the
  distribution. Ie p50 would be labeled `0.5`, p99 as `0.99`. The source DataFrame must have
  a `time_` column of type `TIME64NS` or the compiler will throw an error.
  The unit of this metric will be inferred from the [SemanticType](https://github.com/pixie-io/pixie/blob/main/src/api/proto/vizierpb/vizierapi.proto#L51)
  of the column `value`.

  :topic: otel

  Args:
    name (string): The name of the metric.
    count (Column): The column of the count of elements inside each summary. Must be INT64
    sum (Column): The column of the sum of elements in inside each summary to use for the Summary. Must be FLOAT64
    quantile_values (Dict[float, Column]): The mapping of the quantile value to the DataFrame column
      containing the quantile value information. Must be FLOAT64.
    description (string, optional): A description of what the metric tracks.
    attributes (Dict[string, Column|string], optional): A mapping of attribute name to a string or the column
      that stores data about the attribute.
    unit (string, optional): The unit string to use for the metric. If not specified, will attempt
      to use the Semantic Type of the quantile values to infer the unit string.
  Returns:
    OTelDataContainer: the mapping of DataFrame columns to OpenTelemetry Summary fields. Can be passed
      into `px.otel.Data()` as the data argument.
  )doc";

 protected:
  OTelMetrics(ASTVisitor* ast_visitor, IR* graph)
      : QLObject(OTelMetricsModuleType, ast_visitor), graph_(graph) {}
  Status Init();

 private:
  IR* graph_;
};

class OTelTrace : public QLObject {
 public:
  inline static constexpr char kOTelTraceModule[] = "trace";
  static constexpr TypeDescriptor OTelTraceModuleType = {
      /* name */ kOTelTraceModule,
      /* type */ QLObjectType::kModule,
  };
  static StatusOr<std::shared_ptr<OTelTrace>> Create(ASTVisitor* ast_visitor, IR* graph);

  inline static constexpr char kSpanOpID[] = "Span";
  inline static constexpr char kSpanOpDocstring[] = R"doc(
  Defines the OpenTelemetry Trace Span type.

  Span describes how to transform a pixie DataFrame into the OpenTelemetry
  Span type.

  :topic: otel

  Args:
    name (string,Column): The name of the span. Can be a string or a STRING column.
    start_time (Column): The column that marks the beginning of the span, must be TIME64NS.
    end_time (Column): The column that marks the end of the span, must be TIME64NS.
    trace_id (Column, optional): The column containing trace_ids, must be formatted as a lower-case hex
      with 32 hex characters (aka 16 bytes), or the engine will auto-generate a new ID. If not specified,
      the OpenTelemetry exporter will auto-generate a valid ID.
    span_id (Column, optional): The column containing trace_ids, must be formatted as a lower-case hex
      with 16 hex characters (aka 8 bytes), or the engine will auto-generate a new ID. If not specified,
      the OpenTelemetry exporter will auto-generate a valid ID.
    parent_span_id (Column, optional): The column containing parent_span_ids, must be formatted as a lower-case hex
      with 16 hex characters (aka 8 bytes), or the engine will write the data as empty. If not specified,
      will leave the parent_span_id field empty.
    attributes (Dict[string, Column|string], optional): A mapping of attribute name to a string or the column
      that stores data about the attribute.
    kind (int, optional): The OpenTelemetry SpanKind enum value to assign for all the spans. Defaults to SPAN_KIND_SERVER
      if not set.
  Returns:
    OTelDataContainer: the mapping of DataFrame columns to OpenTelemetry Span fields. Can be passed
      into `px.otel.Data()` as the data argument.
  )doc";

 protected:
  OTelTrace(ASTVisitor* ast_visitor, IR* graph)
      : QLObject(OTelTraceModuleType, ast_visitor), graph_(graph) {}
  Status Init();
  Status AddSpanKindAttribute(::opentelemetry::proto::trace::v1::Span::SpanKind kind);

 private:
  IR* graph_;
};

class EndpointConfig : public QLObject {
 public:
  struct ConnAttribute {
    std::string name;
    std::string value;
  };
  static constexpr TypeDescriptor EndpointType = {
      /* name */ "Endpoint",
      /* type */ QLObjectType::kOTelEndpoint,
  };
  static StatusOr<std::shared_ptr<EndpointConfig>> Create(
      ASTVisitor* ast_visitor, std::string url,
      std::vector<EndpointConfig::ConnAttribute> attributes, bool insecure, int64_t timeout);

  Status ToProto(planpb::OTelEndpointConfig* endpoint_config);

 protected:
  EndpointConfig(ASTVisitor* ast_visitor, std::string url,
                 std::vector<EndpointConfig::ConnAttribute> attributes, bool insecure,
                 int64_t timeout)
      : QLObject(EndpointType, ast_visitor),
        url_(std::move(url)),
        attributes_(std::move(attributes)),
        insecure_(insecure),
        timeout_(timeout) {}

 private:
  std::string url_;
  std::vector<EndpointConfig::ConnAttribute> attributes_;
  bool insecure_;
  int64_t timeout_;
};

class OTelDataContainer : public QLObject {
 public:
  static constexpr TypeDescriptor OTelDataContainerType = {
      /* name */ "OTelDataContainer",
      /* type */ QLObjectType::kOTelDataContainer,
  };

  static StatusOr<std::shared_ptr<OTelDataContainer>> Create(
      ASTVisitor* ast_visitor, std::variant<OTelMetric, OTelSpan> data);

  static bool IsOTelDataContainer(const QLObjectPtr& obj) {
    return obj->type() == OTelDataContainerType.type();
  }

  const std::variant<OTelMetric, OTelSpan>& data() const { return data_; }

 protected:
  OTelDataContainer(ASTVisitor* ast_visitor, std::variant<OTelMetric, OTelSpan> data)
      : QLObject(OTelDataContainerType, ast_visitor), data_(std::move(data)) {}

 private:
  std::variant<OTelMetric, OTelSpan> data_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
