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

#include <utility>
#include <vector>

#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service_mock.grpc.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service_mock.grpc.pb.h"

#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/carnotpb/carnot_mock.grpc.pb.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/common/uuid/uuid_utils.h"
#include "src/shared/types/types.h"
#include "src/table_store/schemapb/schema.pb.h"

namespace px {
namespace carnot {
namespace exec {

using carnotpb::MockResultSinkServiceStub;
using carnotpb::ResultSinkService;
using carnotpb::TransferResultChunkRequest;
using carnotpb::TransferResultChunkResponse;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using testing::proto::EqualsProto;
namespace otelmetricscollector = opentelemetry::proto::collector::metrics::v1;
namespace oteltracecollector = opentelemetry::proto::collector::trace::v1;

class OTelExportSinkNodeTest : public ::testing::Test {
 public:
  OTelExportSinkNodeTest() {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();

    metrics_mock_unique_ = std::make_unique<otelmetricscollector::MockMetricsServiceStub>();
    metrics_mock_ = metrics_mock_unique_.get();

    trace_mock_unique_ = std::make_unique<oteltracecollector::MockTraceServiceStub>();
    trace_mock_ = trace_mock_unique_.get();

    exec_state_ = std::make_unique<ExecState>(
        func_registry_.get(), table_store, MockResultSinkStubGenerator,
        [this](const std::string& url,
               bool) -> std::unique_ptr<otelmetricscollector::MetricsService::StubInterface> {
          url_ = url;
          return std::move(metrics_mock_unique_);
        },
        [this](const std::string& url,
               bool) -> std::unique_ptr<oteltracecollector::TraceService::StubInterface> {
          url_ = url;
          return std::move(trace_mock_unique_);
        },
        sole::uuid4(), nullptr, nullptr, [](grpc::ClientContext*) {});
  }

 protected:
  std::string url_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
  otelmetricscollector::MockMetricsServiceStub* metrics_mock_;
  oteltracecollector::MockTraceServiceStub* trace_mock_;

 private:
  // Ownership will be transferred to the GRPC node, so access this ptr via `metrics_mock_` in the
  // tests.
  std::unique_ptr<otelmetricscollector::MockMetricsServiceStub> metrics_mock_unique_;
  std::unique_ptr<oteltracecollector::MockTraceServiceStub> trace_mock_unique_;
};

TEST_F(OTelExportSinkNodeTest, endpoint_config) {
  std::string operator_pb_txt = R"(
endpoint_config {
  url: "otlp.px.dev"
    headers {
      key: "api_key"
      value: "abcd"
    }
}
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  gauge { int_column_index: 1 }
})";
  planpb::OTelExportSinkOperator otel_sink_op;

  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(operator_pb_txt, &otel_sink_op));
  auto plan_node = std::make_unique<plan::OTelExportSinkOperator>(1);
  auto s = plan_node->Init(otel_sink_op);
  RowDescriptor input_rd({types::TIME64NS, types::FLOAT64});
  RowDescriptor output_rd({});

  std::shared_ptr<const grpc::AuthContext> auth_context;

  EXPECT_CALL(*metrics_mock_, Export(_, _, _))
      .Times(1)
      .WillRepeatedly(Invoke([&auth_context](const auto& ctx, const auto&, const auto&) {
        auth_context = ctx->auth_context();
        return grpc::Status::OK;
      }));

  auto tester = exec::ExecNodeTester<OTelExportSinkNode, plan::OTelExportSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  auto rb1 = RowBatchBuilder(input_rd, 1, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Time64NSValue>({10})
                 .AddColumn<types::Float64Value>({1.0})
                 .get();
  tester.ConsumeNext(rb1, 1, 0);

  EXPECT_EQ(url_, "otlp.px.dev");
}

TEST_F(OTelExportSinkNodeTest, non_utf_8) {
  std::string operator_pb_txt = R"(
endpoint_config {
  url: "otlp.px.dev"
    headers {
      key: "api_key"
      value: "abcd"
    }
}
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  gauge { int_column_index: 1 }
  attributes {
    name: "test"
    column {
      column_type: STRING
      column_index: 2
    }
  }
})";
  planpb::OTelExportSinkOperator otel_sink_op;

  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(operator_pb_txt, &otel_sink_op));
  auto plan_node = std::make_unique<plan::OTelExportSinkOperator>(1);
  auto s = plan_node->Init(otel_sink_op);
  RowDescriptor input_rd({types::TIME64NS, types::FLOAT64, types::STRING});
  RowDescriptor output_rd({});

  std::shared_ptr<const grpc::AuthContext> auth_context;

  std::vector<otelmetricscollector::ExportMetricsServiceRequest> actual_protos(1);
  size_t i = 0;
  EXPECT_CALL(*metrics_mock_, Export(_, _, _))
      .Times(1)
      .WillRepeatedly(Invoke(
          [&i, &actual_protos, &auth_context](const auto& ctx, const auto& proto, const auto&) {
            auth_context = ctx->auth_context();
            actual_protos[i] = proto;
            ++i;
            return grpc::Status::OK;
          }));

  auto tester = exec::ExecNodeTester<OTelExportSinkNode, plan::OTelExportSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  std::string non_utf_8_bytes(1, 0xC0);
  auto rb1 = RowBatchBuilder(input_rd, 1, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Time64NSValue>({10})
                 .AddColumn<types::Float64Value>({1.0})
                 .AddColumn<types::StringValue>({non_utf_8_bytes})
                 .get();
  tester.ConsumeNext(rb1, 1, 0);
  EXPECT_EQ(non_utf_8_bytes, actual_protos[0]
                                 .resource_metrics(0)
                                 .instrumentation_library_metrics(0)
                                 .metrics(0)
                                 .gauge()
                                 .data_points(0)
                                 .attributes(0)
                                 .value()
                                 .bytes_value());
}

struct TestCase {
  std::string name;
  std::string operator_proto;
  std::vector<std::string> incoming_rowbatches;
  std::vector<std::string> expected_otel_protos;
};

class OTelMetricsTest : public OTelExportSinkNodeTest,
                        public ::testing::WithParamInterface<TestCase> {};

TEST_P(OTelMetricsTest, process_data) {
  auto tc = GetParam();
  std::vector<otelmetricscollector::ExportMetricsServiceRequest> actual_protos(
      tc.expected_otel_protos.size());
  size_t i = 0;
  EXPECT_CALL(*metrics_mock_, Export(_, _, _))
      .Times(tc.expected_otel_protos.size())
      .WillRepeatedly(Invoke([&i, &actual_protos](const auto&, const auto& proto, const auto&) {
        actual_protos[i] = proto;
        ++i;
        return grpc::Status::OK;
      }));

  planpb::OTelExportSinkOperator otel_sink_op;

  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(tc.operator_proto, &otel_sink_op));
  auto plan_node = std::make_unique<plan::OTelExportSinkOperator>(1);
  auto s = plan_node->Init(otel_sink_op);

  // Load a RowBatch to get the Input RowDescriptor.
  table_store::schemapb::RowBatchData row_batch_proto;
  EXPECT_TRUE(
      google::protobuf::TextFormat::ParseFromString(tc.incoming_rowbatches[0], &row_batch_proto));
  RowDescriptor input_rd = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie()->desc();
  RowDescriptor output_rd({});

  auto tester = exec::ExecNodeTester<OTelExportSinkNode, plan::OTelExportSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  for (const auto& rb_pb_txt : tc.incoming_rowbatches) {
    table_store::schemapb::RowBatchData row_batch_proto;
    EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(rb_pb_txt, &row_batch_proto));
    auto rb = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie();
    tester.ConsumeNext(*rb.get(), 1, 0);
  }

  for (size_t i = 0; i < tc.expected_otel_protos.size(); ++i) {
    EXPECT_THAT(actual_protos[i], EqualsProto(tc.expected_otel_protos[i]));
  }
}

INSTANTIATE_TEST_SUITE_P(OTelMetrics, OTelMetricsTest,
                         ::testing::ValuesIn(std::vector<TestCase>{
                             {"summary",
                              R"pb(
metrics {
  name: "http.resp.latency"
  attributes {
    name: "http.method"
    column {
      column_type: STRING
      column_index: 1
    }
  }
  time_column_index: 0
  summary {
    count_column_index: 2
    sum_column_index: 3
    quantile_values {
      quantile: 0.5
      value_column_index: 4
    }
    quantile_values {
      quantile: 0.99
      value_column_index: 5
    }
  }
}
)pb",
                              {R"pb(
cols { time64ns_data { data: 10 data: 11 } }
cols { string_data { data: "GET" data: "POST" } }
cols { int64_data { data: 10 data: 100 } }
cols { float64_data { data: 100 data: 2000 } }
cols { float64_data { data: 15 data: 150 } }
cols { float64_data { data: 60 data: 600 } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      summary {
        data_points {
          time_unix_nano: 10
          count: 10
          sum: 100
          quantile_values {
            quantile: 0.5
            value: 15
          }
          quantile_values {
            quantile: 0.99
            value: 60
          }
          attributes {
            key: "http.method"
            value {
              string_value: "GET"
            }
          }
        }
      }
    }
  }
}
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      summary {
        data_points {
          time_unix_nano: 11
          count: 100
          sum: 2000
          quantile_values {
            quantile: 0.5
            value: 150
          }
          quantile_values {
            quantile: 0.99
            value: 600
          }
          attributes {
            key: "http.method"
            value {
              string_value: "POST"
            }
          }
        }
      }
    }
  }
})pb"}},
                             {"gauge_float_and_attributes",
                              R"pb(
resource {
  attributes {
    name: "service.name"
    column {
      column_type: STRING
      column_index: 1
      can_be_json_encoded_array: true
    }
  }
}
metrics {
  name: "http.resp.latency"
  attributes {
    name: "http.method"
    column {
      column_type: STRING
      column_index: 2
    }
  }
  time_column_index: 0
  gauge { float_column_index: 3 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 data: 11 } }
cols { string_data { data: "pl/querybroker" data: "pl/metadata" } }
cols { string_data { data: "GET" data: "POST" } }
cols { float64_data { data: 15 data: 150 } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "pl/querybroker"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_double: 15
          attributes {
            key: "http.method"
            value {
              string_value: "GET"
            }
          }
        }
      }
    }
  }
}
resource_metrics {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "pl/metadata"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 11
          as_double: 150
          attributes {
            key: "http.method"
            value {
              string_value: "POST"
            }
          }
        }
      }
    }
  }
})pb"}},

                             {"gauge_int",
                              R"pb(
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  gauge { int_column_index: 1 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 data: 11 } }
cols { int64_data { data: 15 data: 150 } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
}
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 11
          as_int: 150
        }
      }
    }
  }
})pb"}},

                             {"first_column_not_time_column",
                              R"pb(
metrics {
  name: "http.resp.latency"
  time_column_index: 1
  gauge { int_column_index: 0 }
})pb",
                              {R"pb(
cols { int64_data { data: 15 data: 150 } }
cols { time64ns_data { data: 10 data: 11 } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
}
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 11
          as_int: 150
        }
      }
    }
  }
})pb"}},
                             {"description_and_unit",
                              R"pb(
metrics {
  name: "http.resp.latency"
  description: "tracks the response latency of http requests"
  unit: "ns"
  time_column_index: 0
  gauge { int_column_index: 1 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 } }
cols { int64_data { data: 15  } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      description: "tracks the response latency of http requests"
      unit: "ns"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
})pb"}},
                             {"multi_batch",
                              R"pb(
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  gauge { int_column_index: 1 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 } }
cols { int64_data { data: 15 } }
num_rows: 1)pb",
                               R"pb(
cols { time64ns_data { data: 11 } }
cols { int64_data { data: 150 } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
})pb",
                               R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 11
          as_int: 150
        }
      }
    }
  }
})pb"}},
                             {"summary_optional_sum_column",
                              R"pb(
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  summary {
    count_column_index: 1
    sum_column_index: -1
    quantile_values {
      quantile: 0.99
      value_column_index: 2
    }
  }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10  } }
cols { int64_data { data: 10 } }
cols { float64_data { data: 600 } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      summary {
        data_points {
          time_unix_nano: 10
          count: 10
          quantile_values {
            quantile: 0.99
            value: 600
          }
        }
      }
    }
  }
})pb"}},
                             {"many_metrics",
                              R"pb(
metrics {
  name: "metric1"
  time_column_index: 0
  gauge { int_column_index: 1 }
}
metrics {
  name: "metric2"
  time_column_index: 0
  gauge { int_column_index: 1 }
})pb",

                              {R"pb(
cols { time64ns_data { data: 10 } }
cols { int64_data { data: 15 } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "metric1"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
    metrics {
      name: "metric2"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
})pb"}},
                             {"gauge_json_encoded_array",
                              R"pb(
resource {
  attributes {
    name: "service.name"
    column {
      column_type: STRING
      column_index: 1
      can_be_json_encoded_array: true
    }
  }
  attributes {
    name: "pod.name"
    column {
      column_type: STRING
      column_index: 2
      can_be_json_encoded_array: true
    }
  }
}
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  gauge { float_column_index: 3 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10  } }
cols { string_data { data: "[\"pl/querybroker\", \"pl/metadata\"]"  } }
cols { string_data { data: "[\"1111\", \"2222\"]"  } }
cols { float64_data { data: 15  } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "pl/querybroker"
      }
    }
    attributes {
      key: "pod.name"
      value {
        string_value: "1111"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_double: 15
        }
      }
    }
  }
}
resource_metrics {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "pl/metadata"
      }
    }
    attributes {
      key: "pod.name"
      value {
        string_value: "1111"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_double: 15
        }
      }
    }
  }
}
resource_metrics {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "pl/querybroker"
      }
    }
    attributes {
      key: "pod.name"
      value {
        string_value: "2222"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_double: 15
        }
      }
    }
  }
}
resource_metrics {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "pl/metadata"
      }
    }
    attributes {
      key: "pod.name"
      value {
        string_value: "2222"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_double: 15
        }
      }
    }
  }
})pb"}},
                             {"all_attribute_types",
                              R"pb(
metrics {
  name: "http.resp.latency"
  attributes {
    name: "http.status.code"
    column {
      column_type: INT64
      column_index: 2
    }
  }
  attributes {
    name: "http.status.method"
    column {
      column_type: STRING
      column_index: 3
    }
  }
  attributes {
    name: "http.status.did_error"
    column {
      column_type: BOOLEAN
      column_index: 4
    }
  }
  attributes {
    name: "http.status.proportion"
    column {
      column_type: FLOAT64
      column_index: 5
    }
  }
  time_column_index: 0
  gauge { float_column_index: 1 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 } }
cols { float64_data { data: 100 } }
cols { int64_data { data: 200 } }
cols { string_data { data: "GET" } }
cols { boolean_data { data: false  } }
cols { float64_data { data: 0.5 } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_double: 100
          attributes {
            key: "http.status.code"
            value {
              int_value: 200
            }
          }
          attributes {
            key: "http.status.method"
            value {
              string_value: "GET"
            }
          }
          attributes {
            key: "http.status.did_error"
            value {
              bool_value: false
            }
          }
          attributes {
            key: "http.status.proportion"
            value {
              double_value: 0.5
            }
          }
        }
      }
    }
  }
})pb"}},
                             {"string_value_attribute",
                              R"pb(
resource{
  attributes {
    name: "pixie_cloud_addr"
    string_value: "dev.withpixie.dev"
  }
}
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  attributes {
    name: "req_path"
    string_value: "/api/v1/query"
  }
  gauge { int_column_index: 1 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 data: 11 } }
cols { int64_data { data: 15 data: 150 } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {
    attributes {
      key: "pixie_cloud_addr"
      value {
        string_value: "dev.withpixie.dev"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          attributes {
            key: "req_path"
            value {
              string_value: "/api/v1/query"
            }
          }
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
}
resource_metrics {
  resource {
    attributes {
      key: "pixie_cloud_addr"
      value {
        string_value: "dev.withpixie.dev"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          attributes {
            key: "req_path"
            value {
              string_value: "/api/v1/query"
            }
          }
          time_unix_nano: 11
          as_int: 150
        }
      }
    }
  }
})pb"}},
                         }),
                         [](const ::testing::TestParamInfo<TestCase>& info) {
                           return info.param.name;
                         });
class OTelSpanTest : public OTelExportSinkNodeTest,
                     public ::testing::WithParamInterface<TestCase> {};

TEST_P(OTelSpanTest, process_data) {
  auto tc = GetParam();
  std::vector<oteltracecollector::ExportTraceServiceRequest> actual_protos(
      tc.expected_otel_protos.size());
  size_t i = 0;
  EXPECT_CALL(*trace_mock_, Export(_, _, _))
      .Times(tc.expected_otel_protos.size())
      .WillRepeatedly(Invoke([&i, &actual_protos](const auto&, const auto& proto, const auto&) {
        actual_protos[i] = proto;
        ++i;
        return grpc::Status::OK;
      }));

  planpb::OTelExportSinkOperator otel_sink_op;

  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(tc.operator_proto, &otel_sink_op));
  auto plan_node = std::make_unique<plan::OTelExportSinkOperator>(1);
  auto s = plan_node->Init(otel_sink_op);

  // Load a RowBatch to get the Input RowDescriptor.
  table_store::schemapb::RowBatchData row_batch_proto;
  EXPECT_TRUE(
      google::protobuf::TextFormat::ParseFromString(tc.incoming_rowbatches[0], &row_batch_proto));
  RowDescriptor input_rd = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie()->desc();
  RowDescriptor output_rd({});

  auto tester = exec::ExecNodeTester<OTelExportSinkNode, plan::OTelExportSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  for (const auto& rb_pb_txt : tc.incoming_rowbatches) {
    table_store::schemapb::RowBatchData row_batch_proto;
    EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(rb_pb_txt, &row_batch_proto));
    auto rb = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie();
    tester.ConsumeNext(*rb.get(), 1, 0);
  }

  for (size_t i = 0; i < tc.expected_otel_protos.size(); ++i) {
    EXPECT_THAT(actual_protos[i], EqualsProto(tc.expected_otel_protos[i]));
  }
}

INSTANTIATE_TEST_SUITE_P(OTelSpan, OTelSpanTest,
                         ::testing::ValuesIn(std::vector<TestCase>{
                             {"name_as_a_string",
                              R"pb(
resource {
  attributes {
    name: "service.name"
    column {
      column_type: STRING
      column_index: 2
      can_be_json_encoded_array: true
    }
  }
}
spans {
  name_string: "span"
  start_time_column_index: 0
  end_time_column_index: 1
  attributes {
   column {
      column_type: STRING
      column_index: 2
    }
  }
  trace_id_column_index: 3
  span_id_column_index: 4
  parent_span_id_column_index: 5
  kind_value: 3
})pb",

                              {R"pb(
cols { time64ns_data { data: 10 } }
cols { time64ns_data { data: 12 } }
cols { string_data { data: "aaaa" } }
cols { string_data { data: "00112233445566778899aabbccddeeff" } }
cols { string_data { data: "1a2b3c4d5e6f7890" } }
cols { string_data { data: "0987f6e5d4c3b2a1" } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_spans {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "aaaa"
      }
    }
  }
  instrumentation_library_spans {
    spans {
      name: "span"
      start_time_unix_nano: 10
      end_time_unix_nano: 12
      trace_id: "\000\021\"3DUfw\210\231\252\273\314\335\356\377"
      span_id: "\032+<M^ox\220"
      parent_span_id: "\t\207\366\345\324\303\262\241"
      attributes {
        value {
          string_value: "aaaa"
        }
      }
      kind: SPAN_KIND_CLIENT
      status {}
    }
  }
})pb"}},
                             {"span_kind_out_of_max",
                              R"pb(
resource { }
spans {
  name_string: "span"
  start_time_column_index: 0
  end_time_column_index: 1
  trace_id_column_index: 2
  span_id_column_index: 3
  parent_span_id_column_index: 4
  kind_value: 35
})pb",

                              {R"pb(
cols { time64ns_data { data: 10 } }
cols { time64ns_data { data: 12 } }
cols { string_data { data: "00112233445566778899aabbccddeeff" } }
cols { string_data { data: "1a2b3c4d5e6f7890" } }
cols { string_data { data: "0987f6e5d4c3b2a1" } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_spans {
  resource { }
  instrumentation_library_spans {
    spans {
      name: "span"
      start_time_unix_nano: 10
      end_time_unix_nano: 12
      trace_id: "\000\021\"3DUfw\210\231\252\273\314\335\356\377"
      span_id: "\032+<M^ox\220"
      parent_span_id: "\t\207\366\345\324\303\262\241"
      kind: 35
      status {}
    }
  }
})pb"}},
                             {"name_as_a_column",
                              R"pb(
spans {
  name_column_index: 2
  start_time_column_index: 0
  end_time_column_index: 1
  trace_id_column_index: 3
  span_id_column_index: 4
  parent_span_id_column_index: 5
  kind_value: 2
})pb",

                              {R"pb(
cols { time64ns_data { data: 10 data: 20 } }
cols { time64ns_data { data: 12 data: 22 } }
cols { string_data { data: "span1" data: "span2" } }
cols { string_data { data: "00112233445566778899aabbccddeeff" data: "ffeeddccbbaa99887766554433221100" } }
cols { string_data { data: "1a2b3c4d5e6f7890" data: "0987f6e5d4c3b2a1" } }
cols { string_data { data: "0987f6e5d4c3b2a1" data: "1a2b3c4d5e6f7890" } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_spans {
  resource {}
  instrumentation_library_spans {
    spans {
      name: "span1"
      start_time_unix_nano: 10
      end_time_unix_nano: 12
      trace_id: "\000\021\"3DUfw\210\231\252\273\314\335\356\377"
      span_id: "\032+<M^ox\220"
      parent_span_id: "\t\207\366\345\324\303\262\241"
      kind: SPAN_KIND_SERVER
      status {}
    }
  }
}
resource_spans {
  resource {}
  instrumentation_library_spans {
    spans {
      name: "span2"
      start_time_unix_nano: 20
      end_time_unix_nano: 22
      trace_id: "\377\356\335\314\273\252\231\210wfUD3\"\021\000"
      span_id: "\t\207\366\345\324\303\262\241"
      parent_span_id: "\032+<M^ox\220"
      kind: SPAN_KIND_SERVER
      status {}
    }
  }
})pb"}},
                             {"resource_json_encoded_array",
                              R"pb(
resource {
  attributes {
    name: "service.name"
    column {
      column_type: STRING
      column_index: 2
      can_be_json_encoded_array: true
    }
  }
  attributes {
    name: "pod.name"
    column {
      column_type: STRING
      column_index: 3
      can_be_json_encoded_array: true
    }
  }
}
spans {
  name_string: "span"
  start_time_column_index: 0
  end_time_column_index: 1
  trace_id_column_index: 4
  span_id_column_index: 5
  parent_span_id_column_index: -1
  kind_value: 2
})pb",

                              {R"pb(
cols { time64ns_data { data: 10 data: 20 } }
cols { time64ns_data { data: 12 data: 22 } }
cols { string_data { data: "[\"aaaa\", \"bbbb\"]" data: "cccc" } }
cols { string_data { data: "[\"1111\", \"2222\"]" data: "3333" } }
cols { string_data { data: "00112233445566778899aabbccddeeff" data: "00112233445566778899aabbccddeeff" } }
cols { string_data { data: "1a2b3c4d5e6f7890" data: "1a2b3c4d5e6f7890" } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_spans {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "aaaa"
      }
    }
    attributes {
      key: "pod.name"
      value {
        string_value: "1111"
      }
    }
  }
  instrumentation_library_spans {
    spans {
      name: "span"
      start_time_unix_nano: 10
      end_time_unix_nano: 12
      trace_id: "\000\021\"3DUfw\210\231\252\273\314\335\356\377"
      span_id: "\032+<M^ox\220"
      kind: SPAN_KIND_SERVER
      status {}
    }
  }
}
resource_spans {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "bbbb"
      }
    }
    attributes {
      key: "pod.name"
      value {
        string_value: "1111"
      }
    }
  }
  instrumentation_library_spans {
    spans {
      name: "span"
      start_time_unix_nano: 10
      end_time_unix_nano: 12
      trace_id: "\000\021\"3DUfw\210\231\252\273\314\335\356\377"
      span_id: "\032+<M^ox\220"
      kind: SPAN_KIND_SERVER
      status {}
    }
  }
}
resource_spans {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "aaaa"
      }
    }
    attributes {
      key: "pod.name"
      value {
        string_value: "2222"
      }
    }
  }
  instrumentation_library_spans {
    spans {
      name: "span"
      start_time_unix_nano: 10
      end_time_unix_nano: 12
      trace_id: "\000\021\"3DUfw\210\231\252\273\314\335\356\377"
      span_id: "\032+<M^ox\220"
      kind: SPAN_KIND_SERVER
      status {}
    }
  }
}
resource_spans {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "bbbb"
      }
    }
    attributes {
      key: "pod.name"
      value {
        string_value: "2222"
      }
    }
  }
  instrumentation_library_spans {
    spans {
      name: "span"
      start_time_unix_nano: 10
      end_time_unix_nano: 12
      trace_id: "\000\021\"3DUfw\210\231\252\273\314\335\356\377"
      span_id: "\032+<M^ox\220"
      kind: SPAN_KIND_SERVER
      status {}
    }
  }
}
resource_spans {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "cccc"
      }
    }
    attributes {
      key: "pod.name"
      value {
        string_value: "3333"
      }
    }
  }
  instrumentation_library_spans {
    spans {
      name: "span"
      start_time_unix_nano: 20
      end_time_unix_nano: 22
      trace_id: "\000\021\"3DUfw\210\231\252\273\314\335\356\377"
      span_id: "\032+<M^ox\220"
      kind: SPAN_KIND_SERVER
      status {}
    }
  }
})pb"}},
                             {"all_attribute_types",
                              R"pb(
spans {
  name_string: "span"
  start_time_column_index: 0
  end_time_column_index: 1
  trace_id_column_index: 6
  span_id_column_index: 7
  parent_span_id_column_index: -1
  kind_value: 2
  attributes {
    name: "http.status.code"
    column {
      column_type: INT64
      column_index: 2
    }
  }
  attributes {
    name: "http.status.method"
    column {
      column_type: STRING
      column_index: 3
    }
  }
  attributes {
    name: "http.status.did_error"
    column {
      column_type: BOOLEAN
      column_index: 4
    }
  }
  attributes {
    name: "http.status.proportion"
    column {
      column_type: FLOAT64
      column_index: 5
    }
  }
})pb",

                              {R"pb(
cols { time64ns_data { data: 10  } }
cols { time64ns_data { data: 12  } }
cols { int64_data { data: 200 } }
cols { string_data { data: "GET" } }
cols { boolean_data { data: false  } }
cols { float64_data { data: 0.5 } }
cols { string_data { data: "00112233445566778899aabbccddeeff" } }
cols { string_data { data: "1a2b3c4d5e6f7890" } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_spans {
  resource {}
  instrumentation_library_spans {
    spans {
      name: "span"
      start_time_unix_nano: 10
      end_time_unix_nano: 12
      trace_id: "\000\021\"3DUfw\210\231\252\273\314\335\356\377"
      span_id: "\032+<M^ox\220"
      attributes {
        key: "http.status.code"
        value {
          int_value: 200
        }
      }
      attributes {
        key: "http.status.method"
        value {
          string_value: "GET"
        }
      }
      attributes {
        key: "http.status.did_error"
        value {
          bool_value: false
        }
      }
      attributes {
        key: "http.status.proportion"
        value {
          double_value: 0.5
        }
      }
      kind: SPAN_KIND_SERVER
      status {}
    }
  }
})pb"}},
                         }),
                         [](const ::testing::TestParamInfo<TestCase>& info) {
                           return info.param.name;
                         });
struct IDCompare {
  bool generated;
  size_t size;
  std::string value;

  void Compare(const std::string& actual) {
    if (generated) {
      EXPECT_EQ(actual.size(), size);
      return;
    }
    EXPECT_EQ(value, actual);
  }
};
const auto GeneratedID = [](size_t size) { return IDCompare{true, size, ""}; };

const auto SpecificID = [](std::string value, size_t size) {
  return IDCompare{false, size, std::move(value)};
};

struct SpanIDTestCase {
  std::string name;
  std::string operator_proto;
  std::string row_batch;
  std::vector<IDCompare> expected_trace_ids;
  std::vector<IDCompare> expected_span_ids;
  std::vector<IDCompare> expected_parent_span_ids;
};
class SpanIDTests : public OTelExportSinkNodeTest,
                    public ::testing::WithParamInterface<SpanIDTestCase> {};

TEST_P(SpanIDTests, generate_ids) {
  auto tc = GetParam();
  oteltracecollector::ExportTraceServiceRequest actual_proto;
  EXPECT_CALL(*trace_mock_, Export(_, _, _))
      .Times(1)
      .WillRepeatedly(Invoke([&actual_proto](const auto&, const auto& proto, const auto&) {
        actual_proto = proto;
        return grpc::Status::OK;
      }));

  planpb::OTelExportSinkOperator otel_sink_op;

  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(tc.operator_proto, &otel_sink_op));
  auto plan_node = std::make_unique<plan::OTelExportSinkOperator>(1);
  auto s = plan_node->Init(otel_sink_op);

  // Load a RowBatch to get the Input RowDescriptor.
  table_store::schemapb::RowBatchData row_batch_proto;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(tc.row_batch, &row_batch_proto));
  RowDescriptor input_rd = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie()->desc();
  RowDescriptor output_rd({});

  auto tester = exec::ExecNodeTester<OTelExportSinkNode, plan::OTelExportSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  auto rb = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie();
  tester.ConsumeNext(*rb.get(), 1, 0);

  for (const auto& [s_idx, span] : Enumerate(actual_proto.resource_spans())) {
    for (const auto& ilm : span.instrumentation_library_spans()) {
      for (const auto& span : ilm.spans()) {
        SCOPED_TRACE(absl::Substitute("span $0", s_idx));
        {
          SCOPED_TRACE("trace_id");
          tc.expected_trace_ids[s_idx].Compare(span.trace_id());
        }
        {
          SCOPED_TRACE("span_id");
          tc.expected_span_ids[s_idx].Compare(span.span_id());
        }
        {
          SCOPED_TRACE("parent_span_id");
          tc.expected_parent_span_ids[s_idx].Compare(span.parent_span_id());
        }
      }
    }
  }
}
INSTANTIATE_TEST_SUITE_P(
    SpanIDGenerateTests, SpanIDTests,
    ::testing::ValuesIn(std::vector<SpanIDTestCase>{
        {
            "generate_all_columns",
            R"pb(
spans {
  name_string: "span"
  start_time_column_index: 0
  end_time_column_index: 1
  trace_id_column_index: -1
  span_id_column_index: -1
  parent_span_id_column_index: -1
})pb",
            R"pb(
cols { time64ns_data { data: 10 data: 20 } }
cols { time64ns_data { data: 12 data: 22 } }
num_rows: 2
eow: true
eos: true)pb",
            {GeneratedID(16), GeneratedID(16)},
            {GeneratedID(8), GeneratedID(8)},
            {SpecificID("", 8), SpecificID("", 8)},
        },
        {
            "generate_some_values",
            R"pb(
spans {
  name_string: "span"
  start_time_column_index: 0
  end_time_column_index: 1
  trace_id_column_index: 2
  span_id_column_index: 3
  parent_span_id_column_index: -1
})pb",
            R"pb(
cols { time64ns_data { data: 10 data: 20 data: 30 } }
cols { time64ns_data { data: 12 data: 22 data: 32 } }
cols { string_data { data: "00112233445566778899aabbccddeeff" data: "445566778899aabb" data: "invalid_hex" } }
cols { string_data { data: "invalid_hex" data: "012345678" data: "0123456789abcdef" } }
num_rows: 3
eow: true
eos: true)pb",
            {SpecificID({'\x00', '\x11', '\x22', '\x33', '\x44', '\x55', '\x66', '\x77', '\x88',
                         '\x99', '\xaa', '\xbb', '\xcc', '\xdd', '\xee', '\xff'},
                        16),
             SpecificID({'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x44',
                         '\x55', '\x66', '\x77', '\x88', '\x99', '\xaa', '\xbb'},
                        16),
             GeneratedID(16)},
            {GeneratedID(8), GeneratedID(8),
             SpecificID({'\x01', '\x23', '\x45', '\x67', '\x89', '\xab', '\xcd', '\xef'}, 8)},
            {SpecificID("", 8), SpecificID("", 8), SpecificID("", 8)},
        },
        {
            "parent_span_id_only_set_when_exact",
            R"pb(
spans {
  name_string: "span"
  start_time_column_index: 0
  end_time_column_index: 1
  trace_id_column_index: -1
  span_id_column_index: -1
  parent_span_id_column_index: 2
})pb",
            R"pb(
cols { time64ns_data { data: 10 data: 20 } }
cols { time64ns_data { data: 12 data: 22 } }
cols { string_data { data: "invalid_hex" data: "0123456789abcdef" } }
num_rows: 2
eow: true
eos: true)pb",
            {GeneratedID(16), GeneratedID(16)},
            {GeneratedID(8), GeneratedID(8)},
            {SpecificID("", 8),
             SpecificID({'\x01', '\x23', '\x45', '\x67', '\x89', '\xab', '\xcd', '\xef'}, 8)},
        },

        {
            "wrong_lengths_same_as_no_columns",
            R"pb(
spans {
  name_string: "span"
  start_time_column_index: 0
  end_time_column_index: 1
  trace_id_column_index: 2
  span_id_column_index: 3
  parent_span_id_column_index: 4
})pb",
            R"pb(
cols { time64ns_data { data: 10  } }
cols { time64ns_data { data: 12  } }
cols { string_data { data: "a1b2"  } }
cols { string_data { data: "c3d4"  } }
cols { string_data { data: "e5f6"  } }
num_rows: 1
eow: true
eos: true)pb",
            {GeneratedID(16), GeneratedID(16)},
            {GeneratedID(8), GeneratedID(8)},
            {SpecificID("", 8), SpecificID("", 8)},
        },
    }),
    [](const ::testing::TestParamInfo<SpanIDTestCase>& info) { return info.param.name; });
TEST_F(OTelExportSinkNodeTest, span_stub_errors) {
  EXPECT_CALL(*trace_mock_, Export(_, _, _))
      .Times(1)
      .WillRepeatedly(Invoke(
          [&](const auto&, const auto&, const auto&) { return grpc::Status(grpc::INTERNAL, ""); }));

  planpb::OTelExportSinkOperator otel_sink_op;

  std::string operator_proto = R"pb(
spans {
  name_string: "span"
  start_time_column_index: 0
  end_time_column_index: 1
  trace_id_column_index: -1
  span_id_column_index: -1
  parent_span_id_column_index: -1
})pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(operator_proto, &otel_sink_op));
  auto plan_node = std::make_unique<plan::OTelExportSinkOperator>(1);
  auto s = plan_node->Init(otel_sink_op);
  std::string row_batch = R"pb(
cols { time64ns_data { data: 10 data: 20 } }
cols { time64ns_data { data: 12 data: 22 } }
num_rows: 2
eow: true
eos: true)pb";

  // Load a RowBatch to get the Input RowDescriptor.
  table_store::schemapb::RowBatchData row_batch_proto;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(row_batch, &row_batch_proto));
  RowDescriptor input_rd = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie()->desc();
  RowDescriptor output_rd({});

  auto tester = exec::ExecNodeTester<OTelExportSinkNode, plan::OTelExportSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  auto rb = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie();
  auto retval = tester.node()->ConsumeNext(exec_state_.get(), *rb.get(), 1);
  EXPECT_NOT_OK(retval);
  EXPECT_THAT(retval.ToString(), ::testing::MatchesRegex(".*INTERNAL.*"));
}

TEST_F(OTelExportSinkNodeTest, metrics_stub_errors) {
  EXPECT_CALL(*metrics_mock_, Export(_, _, _))
      .Times(1)
      .WillRepeatedly(Invoke(
          [&](const auto&, const auto&, const auto&) { return grpc::Status(grpc::INTERNAL, ""); }));

  planpb::OTelExportSinkOperator otel_sink_op;

  std::string operator_proto = R"pb(
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  gauge { int_column_index: 1 }
})pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(operator_proto, &otel_sink_op));
  auto plan_node = std::make_unique<plan::OTelExportSinkOperator>(1);
  auto s = plan_node->Init(otel_sink_op);
  std::string row_batch = R"pb(
cols { time64ns_data { data: 10 data: 11 } }
cols { int64_data { data: 15 data: 150 } }
num_rows: 2
eow: true
eos: true)pb";

  // Load a RowBatch to get the Input RowDescriptor.
  table_store::schemapb::RowBatchData row_batch_proto;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(row_batch, &row_batch_proto));
  RowDescriptor input_rd = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie()->desc();
  RowDescriptor output_rd({});

  auto tester = exec::ExecNodeTester<OTelExportSinkNode, plan::OTelExportSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  auto rb = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie();
  auto retval = tester.node()->ConsumeNext(exec_state_.get(), *rb.get(), 1);
  EXPECT_NOT_OK(retval);
  EXPECT_THAT(retval.ToString(), ::testing::MatchesRegex(".*INTERNAL.*"));
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
