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

#include "src/carnot/planner/objects/otel.h"
#include <absl/container/flat_hash_set.h>

#include <functional>
#include <regex>
#include <utility>
#include <vector>

#include "src/carnot/planner/ir/otel_export_sink_ir.h"
#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/exporter.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/shared/types/typespb/types.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<OTelModule>> OTelModule::Create(CompilerState* compiler_state,
                                                         ASTVisitor* ast_visitor, IR* ir) {
  auto otel_module = std::shared_ptr<OTelModule>(new OTelModule(ast_visitor));
  PX_RETURN_IF_ERROR(otel_module->Init(compiler_state, ir));
  return otel_module;
}

StatusOr<std::shared_ptr<OTelMetrics>> OTelMetrics::Create(ASTVisitor* ast_visitor, IR* graph) {
  auto otel_metrics = std::shared_ptr<OTelMetrics>(new OTelMetrics(ast_visitor, graph));
  PX_RETURN_IF_ERROR(otel_metrics->Init());
  return otel_metrics;
}

StatusOr<std::shared_ptr<OTelTrace>> OTelTrace::Create(ASTVisitor* ast_visitor, IR* graph) {
  auto otel_trace = std::shared_ptr<OTelTrace>(new OTelTrace(ast_visitor, graph));
  PX_RETURN_IF_ERROR(otel_trace->Init());
  return otel_trace;
}

StatusOr<std::shared_ptr<EndpointConfig>> EndpointConfig::Create(
    ASTVisitor* ast_visitor, std::string url, std::vector<EndpointConfig::ConnAttribute> attributes,
    bool insecure, int64_t timeout) {
  return std::shared_ptr<EndpointConfig>(
      new EndpointConfig(ast_visitor, std::move(url), std::move(attributes), insecure, timeout));
}

Status ExportToOTel(const OTelData& data, const pypa::AstPtr& ast, Dataframe* df) {
  auto op = df->op();
  return op->graph()->CreateNode<OTelExportSinkIR>(ast, op, data).status();
}

StatusOr<std::string> GetArgAsString(const pypa::AstPtr& ast, const ParsedArgs& args,
                                     std::string_view arg_name) {
  PX_ASSIGN_OR_RETURN(StringIR * arg_ir, GetArgAs<StringIR>(ast, args, arg_name));
  return arg_ir->str();
}

Status ParseEndpointConfig(CompilerState* compiler_state, const QLObjectPtr& endpoint,
                           planpb::OTelEndpointConfig* pb) {
  if (NoneObject::IsNoneObject(endpoint)) {
    if (!compiler_state->endpoint_config()) {
      return endpoint->CreateError("no default config found for endpoint, please specify one");
    }

    *pb = *compiler_state->endpoint_config();
    return Status::OK();
  }
  // TODO(philkuz) determine how to handle a default configuration based on the plugin.
  if (endpoint->type() != EndpointConfig::EndpointType.type()) {
    return endpoint->CreateError("expected Endpoint type for 'endpoint' arg, received $0",
                                 endpoint->name());
  }

  return static_cast<EndpointConfig*>(endpoint.get())->ToProto(pb);
}

StatusOr<std::shared_ptr<OTelDataContainer>> OTelDataContainer::Create(
    ASTVisitor* ast_visitor, std::variant<OTelMetric, OTelSpan> data) {
  return std::shared_ptr<OTelDataContainer>(new OTelDataContainer(ast_visitor, std::move(data)));
}

StatusOr<std::vector<OTelAttribute>> ParseAttributes(DictObject* attributes) {
  auto values = attributes->values();
  auto keys = attributes->keys();
  DCHECK_EQ(values.size(), keys.size());
  std::vector<OTelAttribute> otel_attributes;
  for (const auto& [idx, keyobj] : Enumerate(keys)) {
    PX_ASSIGN_OR_RETURN(auto key, GetArgAs<StringIR>(keyobj, "attribute"));
    if (key->str().empty()) {
      return keyobj->CreateError("Attribute key must be a non-empty string");
    }
    if (!ExprObject::IsExprObject(values[idx])) {
      return values[idx]->CreateError(
          "Expr is not an Object. Expected column or string for attribute value, got '$0'",
          QLObjectTypeString(values[idx]->type()));
    }
    auto expr = static_cast<ExprObject*>(values[idx].get())->expr();
    if (Match(expr, ColumnNode())) {
      otel_attributes.push_back({key->str(), static_cast<ColumnIR*>(expr), ""});
    } else if (Match(expr, String())) {
      otel_attributes.push_back({key->str(), nullptr, static_cast<StringIR*>(expr)->str()});
    } else {
      return values[idx]->CreateError("Expected column or string for attribute value, got '$0'",
                                      expr->DebugString());
    }
  }
  return otel_attributes;
}

bool IsValidName(const std::string& name) {
  // Valid instrumentation name according to the OTel spec.
  // https://opentelemetry.io/docs/reference/specification/metrics/api/#instrument
  if (name.empty()) {
    return false;
  }
  static const std::regex rgx("[A-Za-z][A-Za-z0-9_.-]*");
  return std::regex_match(name, rgx);
}

StatusOr<std::string> ParseName(const QLObjectPtr& name) {
  PX_ASSIGN_OR_RETURN(auto name_ir, GetArgAs<StringIR>(name, "name"));
  if (!IsValidName(name_ir->str())) {
    return name->CreateError(
        "Metric name is invalid. Please follow the naming conventions here: "
        "https://opentelemetry.io/docs/reference/specification/metrics/api/#instrument");
  }
  return name_ir->str();
}

StatusOr<QLObjectPtr> GaugeDefinition(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                                      ASTVisitor* visitor) {
  OTelMetric metric;
  PX_ASSIGN_OR_RETURN(metric.name, ParseName(args.GetArg("name")));
  PX_ASSIGN_OR_RETURN(metric.description, GetArgAsString(ast, args, "description"));
  // We add the time_ column  automatically.
  PX_ASSIGN_OR_RETURN(metric.time_column,
                      graph->CreateNode<ColumnIR>(ast, "time_", /* parent_op_idx */ 0));

  PX_ASSIGN_OR_RETURN(auto val, GetArgAs<ColumnIR>(ast, args, "value"));
  metric.unit_column = val;
  if (!NoneObject::IsNoneObject(args.GetArg("unit"))) {
    PX_ASSIGN_OR_RETURN(auto unit, GetArgAsString(ast, args, "unit"));
    metric.unit_str = unit;
  }
  metric.metric = OTelMetricGauge{val};

  QLObjectPtr attributes = args.GetArg("attributes");
  if (!DictObject::IsDict(attributes)) {
    return attributes->CreateError("Expected attributes to be a dictionary, received $0",
                                   attributes->name());
  }

  PX_ASSIGN_OR_RETURN(metric.attributes,
                      ParseAttributes(static_cast<DictObject*>(attributes.get())));

  return OTelDataContainer::Create(visitor, std::move(metric));
}

StatusOr<QLObjectPtr> SummaryDefinition(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                                        ASTVisitor* visitor) {
  OTelMetric metric;
  PX_ASSIGN_OR_RETURN(metric.name, ParseName(args.GetArg("name")));
  PX_ASSIGN_OR_RETURN(metric.description, GetArgAsString(ast, args, "description"));
  // We add the time_ column  automatically.
  PX_ASSIGN_OR_RETURN(metric.time_column,
                      graph->CreateNode<ColumnIR>(ast, "time_", /* parent_op_idx */ 0));

  OTelMetricSummary summary;
  PX_ASSIGN_OR_RETURN(summary.count_column, GetArgAs<ColumnIR>(ast, args, "count"));
  PX_ASSIGN_OR_RETURN(summary.sum_column, GetArgAs<ColumnIR>(ast, args, "sum"));

  auto qvs = args.GetArg("quantile_values");
  if (!DictObject::IsDict(qvs)) {
    return qvs->CreateError("Expected quantile_values to be a dictionary, received $0",
                            qvs->name());
  }
  auto dict = static_cast<DictObject*>(qvs.get());
  auto values = dict->values();
  auto keys = dict->keys();
  CHECK_EQ(values.size(), keys.size());
  if (keys.empty()) {
    return qvs->CreateError("Summary must have at least one quantile value specified");
  }
  for (const auto& [idx, keyobj] : Enumerate(keys)) {
    PX_ASSIGN_OR_RETURN(auto quantile, GetArgAs<FloatIR>(keyobj, "quantile"));
    PX_ASSIGN_OR_RETURN(auto val, GetArgAs<ColumnIR>(values[idx], "quantile value column"));
    summary.quantiles.push_back({quantile->val(), val});
  }
  metric.unit_column = summary.quantiles[0].value_column;
  if (!NoneObject::IsNoneObject(args.GetArg("unit"))) {
    PX_ASSIGN_OR_RETURN(auto unit, GetArgAsString(ast, args, "unit"));
    metric.unit_str = unit;
  }
  metric.metric = summary;

  QLObjectPtr attributes = args.GetArg("attributes");
  if (!DictObject::IsDict(attributes)) {
    return attributes->CreateError("Expected attributes to be a dictionary, received $0",
                                   attributes->name());
  }

  PX_ASSIGN_OR_RETURN(metric.attributes,
                      ParseAttributes(static_cast<DictObject*>(attributes.get())));

  return OTelDataContainer::Create(visitor, std::move(metric));
}

StatusOr<QLObjectPtr> OTelDataDefinition(CompilerState* compiler_state, const pypa::AstPtr&,
                                         const ParsedArgs& args, ASTVisitor* visitor) {
  OTelData otel_data;
  PX_RETURN_IF_ERROR(
      ParseEndpointConfig(compiler_state, args.GetArg("endpoint"), &otel_data.endpoint_config));
  QLObjectPtr data = args.GetArg("data");
  if (!CollectionObject::IsCollection(data)) {
    return data->CreateError("Expected data to be a collection, received $0", data->name());
  }

  auto data_attr = static_cast<CollectionObject*>(data.get());
  if (data_attr->items().empty()) {
    return data_attr->CreateError("Must specify at least 1 data configuration");
  }
  for (const auto& data : data_attr->items()) {
    if (!OTelDataContainer::IsOTelDataContainer(data)) {
      return data->CreateError("Expected an OTelDataContainer, received $0", data->name());
    }
    auto container = static_cast<OTelDataContainer*>(data.get());

    std::visit(overloaded{
                   [&otel_data](const OTelMetric& metric) { otel_data.metrics.push_back(metric); },
                   [&otel_data](const OTelSpan& span) { otel_data.spans.push_back(span); },
               },
               container->data());
  }

  QLObjectPtr resource = args.GetArg("resource");
  if (!DictObject::IsDict(resource)) {
    return resource->CreateError("Expected resource to be a dictionary, received $0",
                                 resource->name());
  }

  PX_ASSIGN_OR_RETURN(otel_data.resource_attributes,
                      ParseAttributes(static_cast<DictObject*>(resource.get())));
  bool has_service_name = false;
  for (const auto& attribute : otel_data.resource_attributes) {
    has_service_name = (attribute.name == "service.name");
    if (has_service_name) break;
  }

  if (!has_service_name) {
    return resource->CreateError("'service.name' must be specified in resource");
  }

  // Append the Pixie resource attributes.
  for (const auto& attribute : compiler_state->debug_info().otel_debug_attrs) {
    otel_data.resource_attributes.push_back({attribute.name, nullptr, attribute.value});
  }

  return Exporter::Create(visitor, [otel_data](auto&& ast, auto&& df) -> Status {
    return ExportToOTel(otel_data, std::forward<decltype(ast)>(ast),
                        std::forward<decltype(df)>(df));
  });
}

StatusOr<QLObjectPtr> EndpointConfigDefinition(const pypa::AstPtr& ast, const ParsedArgs& args,
                                               ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(auto url, GetArgAsString(ast, args, "url"));

  std::vector<EndpointConfig::ConnAttribute> attributes;
  auto headers = args.GetArg("headers");
  if (!DictObject::IsDict(headers)) {
    return headers->CreateError("expected dict() for 'attributes' arg, received $0",
                                headers->name());
  }
  auto headers_dict = static_cast<DictObject*>(headers.get());
  for (const auto& [i, key] : Enumerate(headers_dict->keys())) {
    PX_ASSIGN_OR_RETURN(StringIR * key_ir, GetArgAs<StringIR>(ast, key, "header key"));
    PX_ASSIGN_OR_RETURN(StringIR * val_ir,
                        GetArgAs<StringIR>(ast, headers_dict->values()[i], "header value"));
    attributes.push_back(EndpointConfig::ConnAttribute{key_ir->str(), val_ir->str()});
  }

  PX_ASSIGN_OR_RETURN(BoolIR * insecure_ir, GetArgAs<BoolIR>(ast, args, "insecure"));

  PX_ASSIGN_OR_RETURN(IntIR * timeout_ir, GetArgAs<IntIR>(ast, args, "timeout"));

  return EndpointConfig::Create(visitor, url, attributes, insecure_ir->val(), timeout_ir->val());
}

Status OTelModule::Init(CompilerState* compiler_state, IR* ir) {
  // Setup methods.
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> data_fn,
      FuncObject::Create(kDataOpID, {"resource", "data", "endpoint"}, {{"endpoint", "None"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&OTelDataDefinition, compiler_state, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(data_fn->SetDocString(kDataOpDocstring));
  AddMethod(kDataOpID, data_fn);

  PX_ASSIGN_OR_RETURN(auto metric, OTelMetrics::Create(ast_visitor(), ir));
  PX_RETURN_IF_ERROR(AssignAttribute("metric", metric));

  PX_ASSIGN_OR_RETURN(auto trace, OTelTrace::Create(ast_visitor(), ir));
  PX_RETURN_IF_ERROR(AssignAttribute("trace", trace));

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> endpoint_fn,
      FuncObject::Create(kEndpointOpID, {"url", "headers", "insecure", "timeout"},
                         {{"headers", "{}"}, {"insecure", "False"}, {"timeout", "5"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&EndpointConfigDefinition, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  AddMethod(kEndpointOpID, endpoint_fn);
  PX_RETURN_IF_ERROR(endpoint_fn->SetDocString(kEndpointOpDocstring));
  return Status::OK();

  return Status::OK();
}

Status OTelMetrics::Init() {
  // Setup methods.
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> gauge_fn,
      FuncObject::Create(kGaugeOpID, {"name", "value", "description", "attributes", "unit"},
                         {{"description", "\"\""}, {"attributes", "{}"}, {"unit", "None"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&GaugeDefinition, graph_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(gauge_fn->SetDocString(kGaugeOpDocstring));
  AddMethod(kGaugeOpID, gauge_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> summary_fn,
      FuncObject::Create(
          kSummaryOpID,
          {"name", "count", "sum", "quantile_values", "description", "attributes", "unit"},
          {{"description", "\"\""}, {"attributes", "{}"}, {"unit", "None"}},
          /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(&SummaryDefinition, graph_, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3),
          ast_visitor()));
  PX_RETURN_IF_ERROR(summary_fn->SetDocString(kSummaryOpDocstring));
  AddMethod(kSummaryOpID, summary_fn);

  return Status::OK();
}

StatusOr<QLObjectPtr> SpanDefinition(const pypa::AstPtr& ast, const ParsedArgs& args,
                                     ASTVisitor* visitor) {
  OTelSpan span;
  auto name = args.GetArg("name");
  if (!ExprObject::IsExprObject(name)) {
    return name->CreateError("Expected string or column for 'name' '$0'",
                             QLObjectTypeString(name->type()));
  }
  auto name_expr = static_cast<ExprObject*>(name.get())->expr();
  if (StringIR::NodeMatches(name_expr)) {
    span.name = static_cast<StringIR*>(name_expr)->str();
  } else if (ColumnIR::NodeMatches(name_expr)) {
    span.name = static_cast<ColumnIR*>(name_expr);
  } else {
    return name->CreateError("Expected string or column for 'name' '$0'",
                             IRNode::TypeString(name_expr->type()));
  }

  PX_ASSIGN_OR_RETURN(span.start_time_column, GetArgAs<ColumnIR>(ast, args, "start_time"));
  PX_ASSIGN_OR_RETURN(span.end_time_column, GetArgAs<ColumnIR>(ast, args, "end_time"));
  auto span_id = args.GetArg("span_id");
  if (!NoneObject::IsNoneObject(span_id)) {
    PX_ASSIGN_OR_RETURN(span.span_id_column, GetArgAs<ColumnIR>(ast, args, "span_id"));
  }
  auto trace_id = args.GetArg("trace_id");
  if (!NoneObject::IsNoneObject(trace_id)) {
    PX_ASSIGN_OR_RETURN(span.trace_id_column, GetArgAs<ColumnIR>(ast, args, "trace_id"));
  }
  auto parent_span_id = args.GetArg("parent_span_id");
  if (!NoneObject::IsNoneObject(parent_span_id)) {
    PX_ASSIGN_OR_RETURN(span.parent_span_id_column,
                        GetArgAs<ColumnIR>(ast, args, "parent_span_id"));
  }
  PX_ASSIGN_OR_RETURN(auto span_kind, GetArgAs<IntIR>(ast, args, "kind"));
  span.span_kind = span_kind->val();

  QLObjectPtr attributes = args.GetArg("attributes");
  if (!DictObject::IsDict(attributes)) {
    return attributes->CreateError("Expected attributes to be a dictionary, received $0",
                                   attributes->name());
  }

  PX_ASSIGN_OR_RETURN(span.attributes, ParseAttributes(static_cast<DictObject*>(attributes.get())));

  return OTelDataContainer::Create(visitor, std::move(span));
}

// Parse the kind name and assign it the integer value.
Status OTelTrace::AddSpanKindAttribute(::opentelemetry::proto::trace::v1::Span::SpanKind kind) {
  auto ast = std::make_shared<pypa::Ast>(pypa::AstType::Number);
  PX_ASSIGN_OR_RETURN(IntIR * span_kind,
                      graph_->CreateNode<IntIR>(ast, static_cast<int64_t>(kind)));
  PX_ASSIGN_OR_RETURN(auto value, ExprObject::Create(span_kind, ast_visitor()));
  PX_RETURN_IF_ERROR(
      AssignAttribute(::opentelemetry::proto::trace::v1::Span::SpanKind_Name(kind), value));
  return Status::OK();
}

Status OTelTrace::Init() {
  // Setup methods.
  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> span_fn,
                      FuncObject::Create(kSpanOpID,
                                         {"name", "start_time", "end_time", "trace_id", "span_id",
                                          "parent_span_id", "attributes", "kind"},
                                         {{"trace_id", "None"},
                                          {"parent_span_id", "None"},
                                          {"span_id", "None"},
                                          {"attributes", "{}"},
                                          {"kind", "px.otel.trace.SPAN_KIND_SERVER"}},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(&SpanDefinition, std::placeholders::_1,
                                                   std::placeholders::_2, std::placeholders::_3),
                                         ast_visitor()));
  PX_RETURN_IF_ERROR(span_fn->SetDocString(kSpanOpDocstring));
  AddMethod(kSpanOpID, span_fn);

  PX_RETURN_IF_ERROR(
      AddSpanKindAttribute(::opentelemetry::proto::trace::v1::Span::SPAN_KIND_UNSPECIFIED));
  PX_RETURN_IF_ERROR(
      AddSpanKindAttribute(::opentelemetry::proto::trace::v1::Span::SPAN_KIND_INTERNAL));
  PX_RETURN_IF_ERROR(
      AddSpanKindAttribute(::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER));
  PX_RETURN_IF_ERROR(
      AddSpanKindAttribute(::opentelemetry::proto::trace::v1::Span::SPAN_KIND_CLIENT));
  PX_RETURN_IF_ERROR(
      AddSpanKindAttribute(::opentelemetry::proto::trace::v1::Span::SPAN_KIND_PRODUCER));
  PX_RETURN_IF_ERROR(
      AddSpanKindAttribute(::opentelemetry::proto::trace::v1::Span::SPAN_KIND_CONSUMER));

  return Status::OK();
}

Status EndpointConfig::ToProto(planpb::OTelEndpointConfig* pb) {
  pb->set_url(url_);
  for (const auto& attr : attributes_) {
    (*pb->mutable_headers())[attr.name] = attr.value;
  }
  pb->set_insecure(insecure_);
  pb->set_timeout(timeout_);
  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
