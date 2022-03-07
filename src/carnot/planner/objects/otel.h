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
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"

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

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
