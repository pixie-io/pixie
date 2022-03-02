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

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planpb/plan.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class OTelTraceModule : public QLObject {
 public:
  inline static constexpr char kOTelTraceModule[] = "oteltrace";
  static constexpr TypeDescriptor OTelTraceModuleType = {
      /* name */ kOTelTraceModule,
      /* type */ QLObjectType::kModule,
  };
  static StatusOr<std::shared_ptr<OTelTraceModule>> Create(ASTVisitor* ast_visitor);

  inline static constexpr char kSpanOpID[] = "Span";
  inline static constexpr char kSpanOpDocstring[] = R"doc(
  Defines the OpenTelemetry Span configuration from a DataFrame

  Span creates the OpenTelemetry Span configuration from a DataFrame.
  The function defines a mapping of the DataFrame columns to the fields
  of an OpenTelemetry Span. Each row of the DataFrame transforms into a
  single Span.

  Args:
    name (string): A description of the span's operation.
    start_time_unix_nano (string): Name of the column containing the start time
      of a Span. Must be TIME64NS.
    end_time_unix_nano (string): Name of the column containing the end time of a Span.
      Must be TIME64NS.
    span_id (string, optional): Name of the column containing the id of each span.
      Each entry should be unique. Will be generated if not specified.
    parent_span_id (string, optional): Name of the column containing span's
      parent id. Will be generated if not specified.
    trace_id (string, optional): Name of the column containing the trace ID
      that contains the corresponding span. Will be generated if not specified.
    kind (px.otel.trace.SpanKind, optional): Distinguishes between spans created
      in a particular context. Defaults to px.otel.SPAN_KIND_SERVER.
    status (string, optional): Name of the column containing the status value. If not set,
      will be set to OK.
    endpoint (px.otel.Endpoint, optional): The endpoint configuration value.
      If not set, will grab the value set by the OTel Plugin if it exists.
    attributes (Dict[string, string], optional): A mapping of attribute name to
      the column name that stores data about the attribute.
  Returns:
    Exporter: the description of how to map a DataFrame to an OTel Span. Can be
      passed into `px.export`.
  )doc";

 protected:
  explicit OTelTraceModule(ASTVisitor* ast_visitor) : QLObject(OTelTraceModuleType, ast_visitor) {}
  Status Init();
};

class OTelMetricsModule : public QLObject {
 public:
  inline static constexpr char kOTelMetricsModule[] = "otelmetrics";
  static constexpr TypeDescriptor OTelMetricsModuleType = {
      /* name */ kOTelMetricsModule,
      /* type */ QLObjectType::kModule,
  };
  static StatusOr<std::shared_ptr<OTelMetricsModule>> Create(ASTVisitor* ast_visitor);

  inline static constexpr char kMetricOpID[] = "Metric";
  inline static constexpr char kMetricOpDocstring[] = R"doc(
  Defines the OpenTelemetry Metric configuration from a DataFrame

  Metric creates the OpenTelemetry Metric configuration from a DataFrame.
  The function defines a mapping of the DataFrame columns to the fields
  of an OpenTelemetry Metric configuration. When run in the engine, each
  row of the DataFrame becomes a new metric value in OpenTelemetry.

  Args:
    name (string): The name of the metric.
    description (string): A description of what the metric tracks.
    data (px.otel.metrics.Data): The data configuration for the metric, describing how the
      data is created in the DataFrame and what kind of metric to populate in OTel.
    attributes (Dict[string, string], optional): A mapping of attribute name to the column
      name that stores data about the attribute.
    endpoint (px.otel.Endpoint, optional): The endpoint configuration value. If left out,
      must be set in the OTel plugin settings.
  Returns:
    Exporter: the description of how to map a DataFrame to an OTel Metric. Can be passed
      into `px.export`.
  )doc";

  inline static constexpr char kGaugeOpID[] = "Gauge";
  inline static constexpr char kGaugeOpDocstring[] = R"doc(
  Defines the OpenTelemetry Metric Gauge type.

  Gauge describes how to transform a pixie DataFrame to write into an OpenTelemetry
  Metric Gauge.

  Args:
    start_time_unix_nano (string): Name of the column containing the start time of the Gauge.
      Must be TIME64NS.
    time_unix_nano (string): Name of the column containing the end time of the Gauge.
      Must be TIME64NS.
    value (string): The column name of the value to use for the Gauge.
  Returns:
    px.otel.metric.Data: A configuration for a metric. Can be passed into `px.otel.metric.Metric`.
  )doc";

  inline static constexpr char kSummaryOpID[] = "Summary";
  inline static constexpr char kSummaryOpDocstring[] = R"doc(
  Defines the OpenTelemetry Metric Summary type.

  Summary describes how to transform a pixie DataFrame to write into an OpenTelemetry
  Metric Summary. A summary describes a distribution of data by specifying quantile data
  as well as the sum and count of the distribution. Each row is a separate distribution.

  Args:
    start_time_unix_nano (string): Name of the column containing the start time of the Summary.
      Must be TIME64NS.
    time_unix_nano (string): Name of the column containing the end time of the Summary.
      Must be TIME64NS.
    count (string): The column name of the count of elements to use for the Summary.
    sum (string): The column name of the sum of elements in the particular distirbution to use for the Summary.
    quantile_values(Dict[float, string]): The mapping of the quantile value to the DataFrame column name
      containing the quantile value information.
  Returns:
    px.otel.metric.Data: A configuration for the Summary metric. Can be passed into `px.otel.metric.Metric`.
  )doc";

 protected:
  explicit OTelMetricsModule(ASTVisitor* ast_visitor)
      : QLObject(OTelMetricsModuleType, ast_visitor) {}
  Status Init();
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
      std::vector<EndpointConfig::ConnAttribute> attributes);

  inline static constexpr char kOtelEndpointOpID[] = "Endpoint";
  inline static constexpr char kOtelEndpointOpDocstring[] = R"doc(
  Defines the desitination of OpenTelemetry data.

  Configures the endpoint and any connection arguments necessary to talk to
  an OpenTelemetry collector. Used as an argument to the different OTel data
  type configurations

  Args:
    url (string): The URL of the OTel collector.
    attributes (Dict[string,string], optional): The connection parameters to add to the
      header of the request.
  )doc";

  Status ToProto(planpb::OTelEndpointConfig* endpoint_config);

 protected:
  explicit EndpointConfig(ASTVisitor* ast_visitor, std::string url,
                          std::vector<EndpointConfig::ConnAttribute> attributes)
      : QLObject(EndpointType, ast_visitor),
        url_(std::move(url)),
        attributes_(std::move(attributes)) {}
  Status Init();

 private:
  std::string url_;
  std::vector<EndpointConfig::ConnAttribute> attributes_;
};

class OTelMetricData : public QLObject {
 public:
  static constexpr TypeDescriptor OTelMetricDataType = {
      /* name */ "MetricData",
      /* type */ QLObjectType::kOTelMetricData,
  };
  static bool IsType(const QLObjectPtr& o) { return o->type() == OTelMetricDataType.type(); }
  static StatusOr<std::shared_ptr<OTelMetricData>> Create(ASTVisitor* ast_visitor,
                                                          planpb::OTelMetric pb,
                                                          std::vector<ExpectedColumn> columns);

  planpb::OTelMetric ToProto() { return pb_; }
  const std::vector<ExpectedColumn>& columns() { return columns_; }

 protected:
  OTelMetricData(ASTVisitor* ast_visitor, planpb::OTelMetric pb,
                 std::vector<ExpectedColumn> columns)
      : QLObject(OTelMetricDataType, ast_visitor),
        pb_(std::move(pb)),
        columns_(std::move(columns)) {}

 private:
  planpb::OTelMetric pb_;
  std::vector<ExpectedColumn> columns_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
